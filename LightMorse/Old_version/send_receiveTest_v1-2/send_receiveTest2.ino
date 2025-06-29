/* =================================================================
   COMBO: LM-101-A2 Laser TX  ×  S6967 PIN-PD RX
   ---------------------------------------------------------------
   - D7  : LASER_PIN  (HIGH = 発光)   ─→ 2N2222 → LM-101-A2 (+5 V)
   - A0  : PD_PIN     (アナログ入力) ─→ S6967 アノード
            └─ 3.3 kΩ ─ 1 kΩ ─→ GND   (計 4.3 kΩ RLOAD)
   - S6967 カソード → +5 V  ←逆バイアス
   ================================================================= */
#include <Arduino.h>
#include <string.h>

/* --- ハード設定 --- */
const uint8_t  LASER_PIN = 7;
const uint8_t  PD_PIN    = A0;

/* --- 通信パラメータ --- */
const uint16_t UNIT      = 60;        // dot = 60 ms
const uint16_t DOT_TOL   = 25;        // 許容誤差 ±25 ms
const uint16_t THRESH    = 180;       // 手動しきい値 (要実測調整)
const uint16_t FRAME_GAP = 7*UNIT;    // フレーム間サイレンス

const char* HEADER = "-.-.-";         // CT
const char* FOOTER = ".-.-.";         // AR
const char* NUMPAT[8] =
  { "-", ".-", "..-", "...-", "....-", ".....", "-....", "-..." };

/* --- グローバル --- */
volatile bool txBusy=false;           // 送信中フラグ
uint32_t tPrev = 0;                   // 受信パルス計測

/* ---------------- TX ユーティリティ ---------------- */
inline void pulseDot (){
  digitalWrite(LASER_PIN,HIGH); delay(UNIT);
  digitalWrite(LASER_PIN,LOW ); delay(UNIT);
}
inline void pulseDash(){
  digitalWrite(LASER_PIN,HIGH); delay(3*UNIT);
  digitalWrite(LASER_PIN,LOW ); delay(UNIT);
}
void sendPattern(const char* p){
  for(;*p;++p){
    if(*p=='.') pulseDot();
    else if(*p=='-') pulseDash();
    else if(*p==' '){ digitalWrite(LASER_PIN,LOW); delay(2*UNIT); }
  }
}
void transmit(uint8_t n){
  txBusy=true;
  sendPattern(HEADER);
  sendPattern(NUMPAT[n]);
  sendPattern(FOOTER);
  txBusy=false;
  delay(FRAME_GAP);
}

/* --------------- RX ユーティリティ ---------------- */
inline bool isHigh(){ return analogRead(PD_PIN)>THRESH; }

char classifyPulse(uint16_t len){
  if(abs((int)len-(int)UNIT) <= DOT_TOL)          return '.';
  if(abs((int)len-(int)(3*UNIT)) <= 3*DOT_TOL)    return '-';
  return '?';
}

void pushChar(char* buf,uint8_t& len,char c,uint8_t max=32){
  if(len<max) buf[len++]=c;
  else { memmove(buf,buf+1,max-1); buf[max-1]=c; }
}

void decodeLoop(){
  static char buf[32]; static uint8_t len=0;
  static bool lastHi=false;

  bool hi=isHigh();
  uint32_t now=millis();

  if(hi!=lastHi){                     // エッジ
    uint32_t dur = now - tPrev;
    tPrev = now;

    if(lastHi){                       // HIGH 区間 -> dot/dash
      char s=classifyPulse(dur);
      if(s!='?') pushChar(buf,len,s);
    }else{                            // LOW 区間 -> 文字/フレーム間
      if(dur > 2*UNIT) pushChar(buf,len,' ');
    }

    /* ヘッダー検出 → バッファクリア */
    if(len>=strlen(HEADER) &&
       !strncmp(buf+len-strlen(HEADER),HEADER,strlen(HEADER))){
      len=0;                          // 新フレーム開始
      return;
    }

    /* フッター検出 → 数値化して表示 */
    if(len>=strlen(FOOTER) &&
       !strncmp(buf+len-strlen(FOOTER),FOOTER,strlen(FOOTER))){
      len -= strlen(FOOTER);
      buf[len]='\0';
      for(uint8_t i=0;i<8;++i){
        if(!strcmp(buf,NUMPAT[i])){
          Serial.print(F("RX value = ")); Serial.println(i);
          break;
        }
      }
      len=0;
    }
    lastHi=hi;
  }
}

/* ------------------ SETUP ------------------ */
void setup(){
  pinMode(LASER_PIN,OUTPUT); digitalWrite(LASER_PIN,LOW);
  pinMode(PD_PIN,INPUT);
  Serial.begin(9600);
  Serial.println(F("Enter 0–7 to TX. Receiving simultaneously..."));
}

/* ------------------- LOOP ------------------ */
void loop(){
  /* --- 送信コマンド処理 --- */
  if(Serial.available() && !txBusy){
    char ch = Serial.read();
    if(ch>='0' && ch<='7'){ Serial.print(F("TX: ")); Serial.println(ch);
      transmit(ch-'0');
    }
  }

  /* --- 受信処理 --- */
  if(!txBusy) decodeLoop();
}