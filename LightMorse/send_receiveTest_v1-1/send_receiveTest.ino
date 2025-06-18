/*****************************************************************
  photoreflector_laser_blink.ino
  ---------------------------------------------------------------
  • PHOTO_PIN (A0) : フォトリフレクタ出力
  • LASER_PIN (D7) : 5 V レーザーモジュールを 0.5 s ON / 0.5 s OFF
  • STAT_LED  (D13): センサ値がしきい値を超えたら点灯
  • 100 ms ごとに「時刻, raw値」をシリアルへ出力
*****************************************************************/
#include <Arduino.h>

const uint8_t  PHOTO_PIN  = A0;
const uint8_t  LASER_PIN  = 7;          // トランジスタ経由が安全
const uint8_t  STAT_LED   = LED_BUILTIN;

const uint16_t THRESHOLD  = 300;        // ← 固定値・必要なら手動調整
const uint16_t SAMPLE_INT = 100;        // 100 ms ごとに計測
const uint16_t BLINK_INT  = 0;        // レーザー 0.5 s 間隔

void setup()
{
  pinMode(PHOTO_PIN, INPUT);
  pinMode(LASER_PIN, OUTPUT);
  pinMode(STAT_LED,  OUTPUT);
  digitalWrite(LASER_PIN, LOW);         // 初期 OFF
  digitalWrite(STAT_LED,  LOW);

  Serial.begin(9600);
  while (!Serial) ;                     // USB CDC ボード用
  Serial.println(F("=== Photoreflector + Laser Blink Test ==="));
  Serial.print  (F("Fixed threshold = ")); Serial.println(THRESHOLD);
}

void loop()
{
  static uint32_t lastSample = 0;
  static uint32_t lastBlink  = 0;

  /* ---- レーザー点滅 ---- */
  //if (millis() - lastBlink >= BLINK_INT) {
    //lastBlink += BLINK_INT;
    //digitalWrite(LASER_PIN, !digitalRead(LASER_PIN));  // トグル
  //}

  digitalWrite(LASER_PIN, HIGH);
  /* ---- センサ読み取り & 表示 ---- */
  if (millis() - lastSample >= SAMPLE_INT) {
    lastSample += SAMPLE_INT;

    uint16_t v  = analogRead(PHOTO_PIN);
    bool     hi = v > THRESHOLD;
    digitalWrite(STAT_LED, hi ? HIGH : LOW);

    Serial.print(millis()); Serial.print(','); Serial.println(v);
  }
}