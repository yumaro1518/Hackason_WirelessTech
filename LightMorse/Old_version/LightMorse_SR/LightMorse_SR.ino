/* --------------  必要なヘッダ  -------------- */
#include <Arduino.h>
#include <string.h>        // strcmp, strlen, memmove

/* --------------  共有定数  ------------------- */
const uint8_t  LED_PIN   = 7;     // TX 用 LED
const uint8_t  PHOTO_PIN = A0;     // RX 用フォトセンサ
const uint16_t UNIT      = 60;     // 1 ドット長 [ms]
const uint16_t DOT_TOL   = 25;     // ドット／ダッシュ判定許容
const uint16_t FRAME_TO  = 2000;   // 無信号タイムアウト [ms]

const char* HEADER  = "-.-.-";     // CT (KA)
const char* FOOTER  = ".-.-.";     // AR
const char* NUM_PAT[8] = { "-", ".-", "..-", "...-", "....-",
                           ".....", "-....", "-..." };

uint16_t  threshold = 512;         // RX しきい値（動的に更新）
bool      txBusy    = false;       // 送信中フラグ

/* --------------  TX ユーティリティ  ---------- */
inline void dot ()  { digitalWrite(LED_PIN,HIGH); delay(UNIT);
                      digitalWrite(LED_PIN,LOW ); delay(UNIT); }
inline void dash()  { digitalWrite(LED_PIN,HIGH); delay(3*UNIT);
                      digitalWrite(LED_PIN,LOW ); delay(UNIT); }

void sendPattern(const char* pat)
{
  for (const char* p = pat; *p; ++p) {
    if (*p=='.') dot();
    else if (*p=='-') dash();
    else if (*p==' ') { digitalWrite(LED_PIN,LOW); delay(2*UNIT); }
  }
}

void transmit(uint8_t val)
{
  txBusy = true;                          // ==== TX 開始 ====
  sendPattern(HEADER);
  sendPattern(NUM_PAT[val]);
  sendPattern(FOOTER);
  txBusy = false;                         // ==== TX 終了 ====
  delay(7*UNIT);                          // フレーム間サイレンス
}

/* --------------  RX ユーティリティ  ---------- */
inline bool isHigh() { return analogRead(PHOTO_PIN) > threshold; }

uint16_t measure(bool wantHigh)
{
  uint32_t t0 = millis();
  while (isHigh()!=wantHigh) if (millis()-t0 > FRAME_TO) return 0;

  uint32_t s = millis();
  while (isHigh()==wantHigh) if (millis()-s > 5000) break;
  return (uint16_t)(millis()-s);
}

char classify(uint16_t len)
{
  if (abs((int)len-(int)UNIT)       <= DOT_TOL)     return '.';
  if (abs((int)len-(int)(3*UNIT))   <= 3*DOT_TOL)   return '-';
  return '?';
}

void pushChar(char* buf,uint8_t& len,char c,uint8_t max=32)
{
  if (len<max) buf[len++]=c;
  else { memmove(buf,buf+1,max-1); buf[max-1]=c; }
}

void calibrate(uint16_t ms=400)            // 周囲光しきい値を更新
{
  uint32_t sum=0; uint16_t cnt=0; uint32_t t0=millis();
  while (millis()-t0<ms) { sum+=analogRead(PHOTO_PIN); ++cnt; }
  threshold = (sum/cnt)+30;
}

/* ---- HEADER 待ち → 見つかったら true --------------- */
bool waitHeader()
{
  calibrate();                              // 毎フレーム再キャリブ
  char buf[16]={0}; uint8_t len=0;

  while (!txBusy) {                         // 自送信中は受信禁止
    uint16_t h=measure(true);  if(!h)return false;
    uint16_t l=measure(false); if(!l)return false;

    char s=classify(h);
    if(s=='?'){ len=0; continue; }
    pushChar(buf,len,s);

    if(len>=strlen(HEADER) &&
       !strncmp(buf+len-strlen(HEADER),HEADER,strlen(HEADER)))
      return true;
  }
  return false;
}

/* ---- データ＋フッター受信 → 0-7 / 0xFF --------------- */
uint8_t receivePacket()
{
  char seq[32]={0}; uint8_t len=0;
  while (!txBusy) {
    uint16_t h=measure(true);  if(!h)return 0xFF;
    uint16_t l=measure(false); if(!l)return 0xFF;

    char s=classify(h); if(s=='?')return 0xFF;
    pushChar(seq,len,s);

    if(len>=strlen(FOOTER) &&
       !strncmp(seq+len-strlen(FOOTER),FOOTER,strlen(FOOTER)))
    {
      len-=strlen(FOOTER); seq[len]='\0';
      for(uint8_t n=0;n<8;++n) if(!strcmp(seq,NUM_PAT[n])) return n;
      return 0xFF;
    }
    if(len>=25) return 0xFF;
  }
  return 0xFF;
}

/* --------------  SETUP & LOOP  ---------------- */
void setup()
{
  pinMode(LED_PIN,OUTPUT);  digitalWrite(LED_PIN,LOW);
  pinMode(PHOTO_PIN,INPUT);
  Serial.begin(9600);
  Serial.println(F("Enter 0–7 + <Enter> to TX, or just watch to RX."));
}

void loop()
{
  /* ---------- 送信リクエスト ---------- */
  if (Serial.available()) {
    int ch = Serial.read();           // 1 byte だけ読む
    if ('0'<=ch && ch<='7') {
      Serial.print(F("TX: ")); Serial.println((char)ch);
      transmit(ch-'0');
    }
  }

  /* ---------- 受信側 ---------- */
  if (txBusy) return;                 // 自送信中はスキップ

  if (!waitHeader()) return;          // ヘッダー検出

  uint8_t v = receivePacket();        // 本文
  if (v==0xFF) { Serial.println(F("RX error")); return; }

  Serial.print(F("Received: ")); Serial.println(v);
}