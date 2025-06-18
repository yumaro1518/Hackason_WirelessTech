/*****************************************************************
  hdr_scope_csv_10tries.ino
  ---------------------------------------------------------------
  D7 : LASER_PIN  → 2SC2240 → LM-101-A2 (+5 V)
  A0 : PD_PIN     ← フォトダイオード / フォトトランジスタ出力
*****************************************************************/
#include <Arduino.h>

/* === ハード設定 === */
const uint8_t LASER_PIN = 7;
const uint8_t LED_DBG   = LED_BUILTIN;
const uint8_t PD_PIN    = A0;

/* === 通信パラメータ === */
const uint16_t UNIT    = 60;              // dot [ms]
const uint16_t GAP_FRM = 7 * UNIT;        // フレーム間 [ms]
const char    *HEADER  = "-.-.-";

/* === ロガ設定 === */
const uint32_t SAMPLE_US = 1000;          // 1 kHz
const uint8_t  MAX_TRIAL = 10;            // 10 回送信したら停止

/* --- 内部状態 --- */
enum class TxState : uint8_t { Idle, On, Off };
TxState  txState   = TxState::Idle;
uint8_t  trialCnt  = 0;                   // 試行回数
uint32_t stateT0   = 0;                   // 状態開始時刻 [ms]
uint16_t stateDur  = 0;                   // 状態長 [ms]
uint8_t  hdrIdx    = 0;                   // HEADER 位置
uint32_t nextAutoT = 0;                   // 次自動送信 [ms]

/* === 送信ステートマシン === */
void txStart()
{
  txState = TxState::On;
  hdrIdx  = 0;
  stateDur = (HEADER[0] == '-') ? 3*UNIT : UNIT;
  stateT0  = millis();
  digitalWrite(LASER_PIN, HIGH);
  digitalWrite(LED_DBG,  HIGH);
}
void txService()
{
  if (trialCnt >= MAX_TRIAL) return;      // 規定回数を超えたら何もしない

  uint32_t now = millis();

  /* 自動送信トリガ */
  if (txState == TxState::Idle && now >= nextAutoT) {
    txStart();
    nextAutoT = now + GAP_FRM;            // “次” はフレーム間待機後
  }

  /* 状態遷移 */
  if (txState == TxState::On && (now - stateT0) >= stateDur) {
    /* ON → OFF(unit) */
    digitalWrite(LASER_PIN, LOW);
    digitalWrite(LED_DBG,  LOW);
    txState  = TxState::Off;
    stateDur = UNIT;
    stateT0  = now;
  }
  else if (txState == TxState::Off && (now - stateT0) >= stateDur) {
    /* OFF 終了 → 次シンボル or 完了 */
    if (++hdrIdx < strlen(HEADER)) {      // まだ続く
      txState  = TxState::On;
      stateDur = (HEADER[hdrIdx]=='-') ? 3*UNIT : UNIT;
      stateT0  = now;
      digitalWrite(LASER_PIN, HIGH);
      digitalWrite(LED_DBG,  HIGH);
    } else {
      /* ヘッダー送信完了 */
      txState  = TxState::Idle;
      trialCnt++;
      if (trialCnt < MAX_TRIAL)
        nextAutoT = now + GAP_FRM;
      else {
        /* ---- 送信終了処理 ---- */
        digitalWrite(LASER_PIN, LOW);
        digitalWrite(LED_DBG,  LOW);
      }
    }
  }
}

/* === アナログスコープ === */
void scopeService()
{
  static uint32_t nextSample = micros();
  uint32_t now = micros();
  if ((int32_t)(now - nextSample) >= 0) {
    nextSample += SAMPLE_US;
    uint16_t v = analogRead(PD_PIN);      // 0-1023
    Serial.print(now);  Serial.print(',');  // 時刻[µs]
    Serial.println(v);                     // ADC値
  }
}

/* === SETUP === */
void setup()
{
  pinMode(LASER_PIN, OUTPUT);  digitalWrite(LASER_PIN, LOW);
  pinMode(LED_DBG,  OUTPUT);   digitalWrite(LED_DBG,  LOW);
  pinMode(PD_PIN,   INPUT);
  Serial.begin(115200);
  Serial.println("# timestamp(us),adc");
  nextAutoT = millis();                       // すぐ 1 回目を送信
}

/* === LOOP === */
void loop()
{
  if (trialCnt < MAX_TRIAL) {       // 規定回数の間だけ実行
    txService();
    scopeService();
  } else {
    Serial.println("## FINISHED");
    while (true);                   // 完全停止
  }
}