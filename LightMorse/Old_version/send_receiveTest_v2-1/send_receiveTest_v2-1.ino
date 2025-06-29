/*****************************************************************
  header_tx_rx_flagged.ino      2025-06-15
  ---------------------------------------------------------------
  • D7 : LASER_PIN  HIGH=ON → 2SC2240 → LM-101-A2
  • A0 : PD_PIN     ← フォトダイオード / TIA
  ---------------------------------------------------------------
  変更点
    ✓ headerFlag / digitFlag / footerFlag を表示
    ✓ フッター検出後に RX:xxxxx を一括表示
*****************************************************************/

/* ==== 閾値（固定） ==== */
const uint16_t TH_H = 800;
const uint16_t TH_L = 300;

/* ==== マーク長（ヘッダー／フッター） ==== */
const uint32_t MARK_MS  = 2000;
const uint32_t GAP_MS   = 1000;

/* ==== モールス仕様 ==== */
const uint16_t DOT_MS   = 70;
const uint16_t TOL_MS   = DOT_MS * 35 / 100;
const uint16_t CHAR_GAP_MS = DOT_MS * 3;

/* ==== 数字モールス 0-7 ==== */
const char* MORSE_DIGIT[8] = {
  "-----", ".----", "..---", "...--",
  "....-", ".....", "-....", "--..."
};

/* ==== ピン ==== */
const uint8_t LASER_PIN = 7;
const uint8_t PD_PIN    = A0;

/* ===== 送信部 ===== */
inline void laser(bool on){ digitalWrite(LASER_PIN,on); }

void txDot (){ laser(true);  delay(DOT_MS);   laser(false); delay(DOT_MS); }
void txDash(){ laser(true);  delay(3*DOT_MS); laser(false); delay(DOT_MS); }

void sendHeader(){ laser(true); delay(MARK_MS); laser(false); delay(GAP_MS); }
void sendFooter(){ laser(true); delay(MARK_MS); laser(false);               }

void transmitDigits(const char* str)
{
  sendHeader();
  for(const char* p=str; *p; ++p){
    if(*p<'0'||*p>'7') continue;
    const char* pat=MORSE_DIGIT[*p-'0'];
    for(const char*s=pat;*s;++s) (*s=='-')?txDash():txDot();
    delay(CHAR_GAP_MS);
  }
  sendFooter();
}

/* ===== 受信部 ===== */
bool     headerFlag=false, digitFlag=false, footerFlag=false;

bool     inFrame=false, stateHi=false;
uint32_t tEdge=0, hiStart=0;

char     curChar[8]; uint8_t curLen=0;
char     msgBuf[32]; uint8_t msgLen=0;

inline bool rawHigh(){
  uint16_t v=analogRead(PD_PIN);
  return v>(stateHi?TH_L:TH_H);
}
char classify(uint32_t d){
  if(abs((int)d-(int)DOT_MS)<=TOL_MS)         return '.';
  if(abs((int)d-(int)3*DOT_MS)<=3*TOL_MS)     return '-';
  return '?';
}
void pushChar(char c){
  if(curLen<sizeof(curChar)-1) curChar[curLen++]=c;
}
void flushCurChar(){
  if(!curLen) return;
  curChar[curLen]='\0';
  for(uint8_t d=0; d<8; ++d)
    if(!strcmp(curChar,MORSE_DIGIT[d])){
      msgBuf[msgLen++]='0'+d;
      Serial.print(F("[digitFlag] "));Serial.println((char)('0'+d));
      digitFlag=true;
      break;
    }
  curLen=0;
}

void processEdge(bool prevHi,uint32_t dur){
  /*── ヘッダー／フッター 判定 ──*/
  if(prevHi && dur>=MARK_MS){
    if(!inFrame){                          // ヘッダー
      inFrame=true; headerFlag=true;
      Serial.println(F("[headerFlag]"));
      msgLen=0; curLen=0;
    }else{                                 // フッター
      flushCurChar();                      // 残りを確定
      footerFlag=true;
      Serial.println(F("[footerFlag]"));
      msgBuf[msgLen]='\0';
      Serial.print(F("RX:")); Serial.println(msgBuf);
      inFrame=false;
    }
    return;
  }

  /*── 本文解析 ──*/
  if(!inFrame) return;

  if(prevHi){                              // dot/dash
    char s=classify(dur);
    if(s!='?') pushChar(s);
  }else{                                   // LOW 区間
    if(dur>=CHAR_GAP_MS-TOL_MS) flushCurChar();
  }
}

/* ===== Arduino Core ===== */
void setup(){
  pinMode(LASER_PIN,OUTPUT); laser(false);
  pinMode(PD_PIN,INPUT);
  Serial.begin(115200);
  while(!Serial);
  Serial.println(F("=== 2-s Mark Header/Footer + Flagging ==="));

  /* 起動テスト送信 (数字 "0") */
  transmitDigits("0");
  tEdge=millis();
}

void loop(){
  /* 受信サンプリング 1 kHz */
  static uint32_t next=micros();
  if((int32_t)(micros()-next)>=0){
    next+=1000;
    bool hi=rawHigh();
    if(hi!=stateHi){
      uint32_t now=millis();
      uint32_t dur=now-tEdge; tEdge=now;
      processEdge(stateHi,dur);
      stateHi=hi;
    }
  }

  /* キーボード入力 → 送信 */
  if(Serial.available()){
    static char ibuf[32]; static uint8_t idx=0;
    char c=Serial.read();
    if(c=='\n'||c=='\r'){ ibuf[idx]='\0'; if(idx) transmitDigits(ibuf); idx=0; }
    else if(idx<sizeof(ibuf)-1) ibuf[idx++]=c;
  }
}