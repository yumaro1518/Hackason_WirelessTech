/*****************************************************************
  photoreflector_test.ino
  ---------------------------------------------------------------
  フォトリフレクタ（アナログ出力タイプ）簡易テスト用
    • A0 にセンサ出力を接続（電源は 5 V / GND）
    • 起動後 1 秒かけて「暗環境の平均値」を測り、自動しきい値を決定
    • 100 ms ごとに A0 を読み取り:
          - シリアルモニタへ生値 (0–1023) 出力
          - しきい値を超えたら LED_BUILTIN (D13) を HIGH
    • シリアルプロッタに切り替えるとグラフで動作確認が容易
 *****************************************************************/
const uint8_t  PHOTO_PIN   = A0;        // センサ入力
const uint8_t  LED_PIN     = LED_BUILTIN;
const uint16_t CALIB_TIME  = 1000;      // キャリブ 1 s
const uint16_t SAMPLE_INT  = 100;       // 100 ms ごとに表示

uint16_t darkAvg   = 0;                 // 暗電圧平均
uint16_t threshold = 0;                 // 自動しきい値

void setup()
{
  pinMode(PHOTO_PIN, INPUT);
  pinMode(LED_PIN,   OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(9600);
  while (!Serial) ;                     // Leonardo系で必須

  Serial.println(F("=== Photoreflector Quick Test ==="));
  Serial.println(F("Keep sensor covered for 1 second..."));

  /* --- キャリブレーション（暗環境平均） --- */
  uint32_t t0 = millis(); uint32_t sum = 0; uint16_t cnt = 0;
  while (millis() - t0 < CALIB_TIME) {
    sum += analogRead(PHOTO_PIN); cnt++;
    delay(10);
  }
  darkAvg   = sum / cnt;
  threshold = darkAvg + 50;             // 任意マージン (50/1023 ≈ 0.24 V)

  Serial.print (F("Dark average = "));  Serial.println(darkAvg);
  Serial.print (F("Threshold    = "));  Serial.println(threshold);
  Serial.println(F("Wave your hand / reflect light to test!"));
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last >= SAMPLE_INT) {
    last = millis();

    uint16_t raw = analogRead(PHOTO_PIN);
    bool hit     = raw > threshold;

    digitalWrite(LED_PIN, hit ? HIGH : LOW);

    // CSV 形式: 時間[ms], センサ値
    Serial.print(millis());
    Serial.print(',');
    Serial.println(raw);
  }
}