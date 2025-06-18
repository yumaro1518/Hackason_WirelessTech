/*****************************************************************
  combo_morse_laser_pd.ino
  ---------------------------------------------------------------
  • D7 : LASER_PIN  (HIGH = ON)      → 2SC2240 → LM-101-A2 (+5V)
  • A0 : PD_PIN     (Analog input)   → Photodiode cathode
           └ 4.3 kΩ → GND  (LOAD)      anode → +5V (逆バイアス)
*****************************************************************/
#include <Arduino.h>
#include <string.h>

/* ===== ハード設定 ===== */
const uint8_t LASER_PIN = 7;
const uint8_t PD_PIN    = A0;

/* ===== 通信パラメータ ===== */
const uint16_t UNIT   = 60;            // dot = 60 ms
const uint16_t TOL    = 50;            // 許容誤差 ±50 ms
const uint16_t THRESH = 180;           // 受光しきい値
const uint16_t GAPCHR = 3*UNIT;        // 文字間 LOW
const uint16_t GAPFRM = 7*UNIT;        // フレーム間 LOW

const char *HEADER = "-.-.-";          // CT
const char *FOOTER = ".-.-.";          // AR
const char *NUMPAT[8] =
  { "-", ".-", "..-", "...-", "....-", ".....", "-....", "-..." };

/* ===== 送信ヘルパ ===== */
inline void laserOn(bool on){ digitalWrite(LASER_PIN,on); }
inline void pulseDot (){
  laserOn(true);  delay(UNIT);
  laserOn(false); delay(UNIT);
}
inline void pulseDash(){
  laserOn(true);  delay(3*UNIT);
  laserOn(false); delay(UNIT);
}
void sendPattern(const char *p){
  for(;*p;++p){
    if(*p=='.') pulseDot();
    else if(*p=='-') pulseDash();
    else if(*p==' ') delay(2*UNIT);
  }
}
void transmit(uint8_t n){
  sendPattern(HEADER);
  sendPattern(NUMPAT[n]);
  sendPattern(FOOTER);
  delay(GAPFRM);
}

/* ===== 受信ヘルパ ===== */
inline bool isHi(){ return analogRead(PD_PIN) > THRESH; }
char classifyPulse(uint32_t len){
  if(abs((int)len-(int)UNIT)     <= TOL)     return '.';
  if(abs((int)len-(int)(3*UNIT)) <= 3*TOL)   return '-';
  return '?';
}
void push(char *buf,uint8_t &len,char c,uint8_t max=32){
  if(len<max) buf[len++]=c;
  else { memmove(buf,buf+1,max-1); buf[max-1]=c; }
}

/* ===== パルス幅ロガー ===== */
bool runPulseLogger(uint16_t edges=200){
  static uint16_t cnt=0; static uint32_t t0=0; static bool lastHi=false;
  if(cnt==0){ t0 = micros(); lastHi = isHi(); }

  bool hi = isHi();
  if(hi!=lastHi){
    uint32_t now = micros();
    Serial.println(now - t0);   // μs
    t0 = now; lastHi = hi;
    if(++cnt >= edges){
      Serial.println(F("# Logger done."));
      return true;              // 終了
    }
  }
  return false;                 // 続行中
}

/* ===== デコーダ本体 ===== */
void decodeLoop(){
  static char buf[32]; static uint8_t len=0;
  static bool lastHi = isHi();  static uint32_t tPrev=millis();

  bool  hi  = isHi();
  uint32_t now = millis();

  if(hi!=lastHi){                         // エッジ
    uint32_t dur = now - tPrev; tPrev=now;

    if(lastHi){                           // HIGH 終了 → dot/dash
      char s = classifyPulse(dur);
      if(s!='?') push(buf,len,s);
    }else{                                // LOW 終了 → ギャップ
      if(dur >= GAPCHR) push(buf,len,' ');
    }

    /* ヘッダー検知 → バッファクリア */
    if(len>=strlen(HEADER) &&
       strncmp(buf+len-strlen(HEADER),HEADER,strlen(HEADER))==0){
      len=0; lastHi=hi; return;
    }

    /* フッター検知 → 数値判定 */
    if(len>=strlen(FOOTER) &&
       strncmp(buf+len-strlen(FOOTER),FOOTER,strlen(FOOTER))==0){
      len -= strlen(FOOTER); buf[len]='\0';
      for(uint8_t i=0;i<8;i++)
        if(strcmp(buf,NUMPAT[i])==0){
          Serial.print(F("RX value = ")); Serial.println(i);
          break;
        }
      len=0;
    }
    lastHi=hi;
  }
}

/* ===== SETUP ===== */
void setup(){
  pinMode(LASER_PIN,OUTPUT); laserOn(false);
  pinMode(PD_PIN,INPUT);
  Serial.begin(9600);
  Serial.println(F("# Start logger (200 edges) ..."));
}

/* ===== LOOP ===== */
void loop(){
  /* ---------- ① パルス幅ロガー ---------- */
  static bool loggingDone=false;
  if(!loggingDone){
    loggingDone = runPulseLogger();   // true でロガー完了
    return;                           // 完了まで他処理は行わない
  }

  /* ---------- ② 送信コマンド ---------- */
  if(Serial.available()){
    char ch=Serial.read();
    if(ch>='0' && ch<='7'){
      Serial.print(F("TX ")); Serial.println(ch);
      transmit(ch-'0');
    }
  }

  /* ---------- ③ 受信処理 ---------- */
  decodeLoop();
}