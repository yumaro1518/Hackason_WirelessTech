/*****************************************************************
  autoMarkDigitTx_adcMonitor.ino     2025-06-15
  ---------------------------------------------------------------
  自動送信:  [Header(2s)] 0 1 2 3 4 5 6 7 [Footer(2s)]
             ↑これを NUM_TRIALS 回くり返す
  受  信 :   A0 を 1 kHz で読み出し "millis,adc" 表示
*****************************************************************/

/* === ユーザ設定 === */
const uint8_t  NUM_TRIALS = 3;     // 何セット送るか
const uint16_t TH_H = 800;         // ADC High 判定用
const uint16_t TH_L = 300;         // ADC Low 復帰用

/* === パルス仕様 === */
const uint32_t MARK_MS = 2000;     // ヘッダー/フッター = 2s 点灯
const uint32_t GAP_MS  = 1000;     // ヘッダー後サイレンス
const uint16_t DOT_MS  = 70;       // dot 長さ
const uint16_t CHAR_GAP_MS = DOT_MS * 3;

const char* MORSE[8]={
  "-----",".----","..---","...--","....-",".....","-....","--..."
};

/* === ピン === */
const uint8_t LASER_PIN = 7;
const uint8_t PD_PIN    = A0;

/* === 送信ヘルパ === */
inline void laser(bool on){ digitalWrite(LASER_PIN,on); }
void txDot (){ laser(true);  delay(DOT_MS);   laser(false); delay(DOT_MS); }
void txDash(){ laser(true);  delay(3*DOT_MS); laser(false); delay(DOT_MS); }
void sendHeader(){ laser(true); delay(MARK_MS); laser(false); delay(GAP_MS); }
void sendFooter(){ laser(true); delay(MARK_MS); laser(false); }

void sendOneSequence(){
  sendHeader();
  for(uint8_t d=0; d<8; ++d){
    for(const char* p=MORSE[d]; *p; ++p) (*p=='-')?txDash():txDot();
    delay(CHAR_GAP_MS);
  }
  sendFooter();
}

/* === ADC モニタ設定 === */
bool plotON = true;
const uint32_t SAMPLE_US = 1000;   // 1 kHz

/* === SETUP === */
void setup(){
  pinMode(LASER_PIN,OUTPUT); laser(false);
  pinMode(PD_PIN,INPUT);
  Serial.begin(115200);
  while(!Serial);
  Serial.println(F("=== Auto Tx (Header 0-7 Footer) + ADC monitor ==="));
  Serial.println(F("# millis,adc"));
}

/* === LOOP === */
void loop(){
  /* 1) ADC 出力 (1 kHz) */
  static uint32_t nextSample = micros();
  if((int32_t)(micros()-nextSample) >= 0){
    nextSample += SAMPLE_US;
    if(plotON){
      Serial.print(millis()); Serial.print(',');
      Serial.println(analogRead(PD_PIN));
    }
  }

  /* 2) 自動送信トリガ (blocking) */
  static uint8_t sentCnt = 0;
  if(sentCnt < NUM_TRIALS){
    sendOneSequence();
    ++sentCnt;
  }

  /* 3) シリアルコマンド */
  if(Serial.available()){
    char c=Serial.read();
    if(c=='p'||c=='P'){
      plotON=!plotON;
      Serial.print(F("--- Plot "));Serial.println(plotON?F("ON"):F("OFF"));
    }
  }
}