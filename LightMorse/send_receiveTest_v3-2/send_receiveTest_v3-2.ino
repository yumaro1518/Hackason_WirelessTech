/*  FixedMorseLoopback.ino  ─  Arduino UNO R4 WiFi　->送信内容固定バージョン
 *  ──────────────────────────────────────────────
 *  1. Header → TX_NUMBER → Footer をレーザー (D9) で送信
 *  2. 約 1 メッセージ分休止してフォトトランジスタ (A0) で受信
 *  3. デコード結果をシリアルに表示
 *  4. 以後ループ
 *
 *  ■配線
 *    D9  ──> レーザーモジュール V+      (必要なら NPN トランジスタでスイッチ)
 *    GND ──> レーザーモジュール GND
 *    +5V ──> フォトトランジスタ COLLECTOR
 *    A0  ──> フォトトランジスタ + 10 kΩ–GND プルダウンの分岐点
 *    GND ──> フォトトランジスタ EMITTER & 抵抗下端
 */

#include <Arduino.h>

/*──────────────────── 送受共通パラメータ ───────────────────*/
const uint16_t UNIT   = 100;               // モールス基本時間 [ms]（調整可）
const uint16_t DOT    = UNIT;
const uint16_t DASH   = 3 * UNIT;
const uint16_t GAP_SYM= UNIT;
const uint16_t GAP_CHR= 3 * UNIT;
const uint16_t GAP_MSG= 7 * UNIT;

/*──────────────────── 送信設定 ───────────────────*/
const uint8_t  LASER_PIN = 9;
const uint8_t  TX_NUMBER = 01234567;              // ★ここを 0〜7 に変えるだけ★

/* モールス符号配列（正: 1=DOT,3=DASH / 負: -1=内部, -2=文字間）*/
const int Hdr[] = {1,1,-1,3,3,3,-1,1,1};                     // ..---..
const int Ftr[] = {1,-1,3,-1,1,-1,3,-1,1};                   // .-.-.
const int N0[]  = {3,3,3,3,3};                               // -----
const int N1[]  = {1,3,3,3,3};                               // .----
const int N2[]  = {1,1,3,3,3};                               // ..---
const int N3[]  = {1,1,1,3,3};                               // ...--
const int N4[]  = {1,1,1,1,3};                               // ....-
const int N5[]  = {1,1,1,1,1};                               // .....
const int N6[]  = {3,1,1,1,1};                               // -....
const int N7[]  = {3,3,1,1,1};                               // --...
const int*  NUM_PAT[8] = {N0,N1,N2,N3,N4,N5,N6,N7};
const uint8_t NUM_LEN[8]= {
  sizeof(N0)/2,sizeof(N1)/2,sizeof(N2)/2,sizeof(N3)/2,
  sizeof(N4)/2,sizeof(N5)/2,sizeof(N6)/2,sizeof(N7)/2};

/*──────────────────── 受信設定 ───────────────────*/
const uint8_t  SENSOR_PIN   = A0;
const uint16_t THRESH       = 500;         // 暗:0-3 / 明:200-1023 ⇒ 中間より少し下
const uint16_t DASH_T       = 2 * UNIT;    // これ未満=dot, 以上=dash
const uint16_t GAPCHR_T     = 2 * UNIT;    // 文字区切り
const uint16_t GAPMSG_T     = 6 * UNIT;    // メッセージ区切り
const uint32_t RX_TIMEOUT   = 6000;        // [ms] 受信待ち打ち切り

/*──────────────────── 送信ユーティリティ ───────────────────*/
inline void laserOn()  { digitalWrite(LASER_PIN, HIGH); }
inline void laserOff() { digitalWrite(LASER_PIN, LOW);  }

void sendDot () { laserOn(); delay(DOT);  laserOff(); delay(GAP_SYM); }
void sendDash() { laserOn(); delay(DASH); laserOff(); delay(GAP_SYM); }

void sendPattern(const int* pat, uint8_t len)
{
  for (uint8_t i=0;i<len;i++){
    int v=pat[i];
    if (v==1) sendDot();
    else if(v==3) sendDash();
    else if(v==-1) { laserOff(); delay(GAP_SYM);}          // 符号間
    else if(v==-2) { laserOff(); delay(GAP_CHR);}          // 文字間
  }
  laserOff();
  delay(GAP_CHR);                                          // パターン後文字間
}

void transmitMessage()
{
  Serial.print(F("<TX> ")); Serial.println(TX_NUMBER);
  sendPattern(Hdr, sizeof(Hdr)/2);
  sendPattern(NUM_PAT[TX_NUMBER], NUM_LEN[TX_NUMBER]);
  sendPattern(Ftr, sizeof(Ftr)/2);
}

/*──────────────────── 受信ユーティリティ ───────────────────*/
String patBuf="", digits="";
bool headerSeen=false;

char decode(const String& pat)
{
  if (pat=="...---...") return 'H';
  if (pat==".-.-.")     return 'F';
  if (pat=="-----")     return '0';
  if (pat==".----")     return '1';
  if (pat=="..---")     return '2';
  if (pat=="...--")     return '3';
  if (pat=="....-")     return '4';
  if (pat==".....")     return '5';
  if (pat=="-....")     return '6';
  if (pat=="--...")     return '7';
  return '?';
}

String receiveMessage()
{
  patBuf=""; digits=""; headerSeen=false;
  uint32_t tStart=millis(), lastSw=millis();
  bool lightPrev=false;

  while (millis()-tStart < RX_TIMEOUT) {
    bool lightNow = analogRead(SENSOR_PIN) >= THRESH;
    if (lightNow!=lightPrev){
      uint32_t dur = millis()-lastSw;
      lastSw = millis();
      if (lightPrev){                                    // ON→OFF
        patBuf += (dur < DASH_T)?'.':'-';
      }else{                                             // OFF→ON
        if (dur >= GAPMSG_T && patBuf.length()){
          char c=decode(patBuf); patBuf="";
          if(c=='H'){headerSeen=true; digits="";}
          else if(c=='F'){ return headerSeen?digits:"(no-hdr)";}
          else if(headerSeen && isDigit(c)) digits+=c;
        }else if(dur >= GAPCHR_T && patBuf.length()){
          char c=decode(patBuf); patBuf="";
          if(c=='H') headerSeen=true;
          else if(c=='F') return headerSeen?digits:"(no-hdr)";
          else if(headerSeen && isDigit(c)) digits+=c;
        }
      }
      lightPrev=lightNow;
    }
  }
  return "(timeout)";
}

/*──────────────────── setup / loop ───────────────────*/
void setup(){
  pinMode(LASER_PIN, OUTPUT); laserOff();
  pinMode(SENSOR_PIN, INPUT);
  Serial.begin(9600);
  Serial.println(F("FixedMorseLoopback ready"));
}

void loop(){
  transmitMessage();                  // 送信
  delay(GAP_MSG);                     // 送→受 切替猶予
  Serial.println(F("<RX> wait..."));
  String r=receiveMessage();          // 受信
  Serial.print(F("<RX> result: ")); Serial.println(r);
  delay(2000);                        // 次ループまで少し休む
}