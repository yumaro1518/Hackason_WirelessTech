#include <SPI.h>
#include "Arduino_LED_Matrix.h"
ArduinoLEDMatrix matrix;

// ピン定義
#define PIN_SPI_MOSI 11
#define PIN_SPI_MISO 12
#define PIN_SPI_SCK  13
#define PIN_SPI_SS   10
#define PIN_BUSY     9

// ─────────────────────────────────────
// 変換処理を削除したため、文字テーブルや findPos() は不要
// ─────────────────────────────────────

// ──────────── L6470 初期化周り ────────────
void L6470_setup();
void L6470_resetdevice();
void L6470_setparam_acc(uint16_t);
void L6470_setparam_dec(uint16_t);
void L6470_setparam_maxspeed(uint16_t);
void L6470_setparam_minspeed(uint16_t);
void L6470_setparam_fsspd(uint16_t);
void L6470_setparam_kvalhold(uint8_t);
void L6470_setparam_kvalrun(uint8_t);
void L6470_setparam_kvalacc(uint8_t);
void L6470_setparam_kvaldec(uint8_t);
void L6470_setparam_stepmood(uint8_t);
void L6470_goto(long);
void L6470_busydelay(unsigned long);

// ──────────── ユーティリティ ────────────
// インデックス→ステップ換算
int indexToStep(int label) {
  int mm[] = {200,250,300,350,400,450,500,600,700};
  if (label >= 0 && label <= 8)
    return (int)(mm[label] * 200.0 * 8 / 60.0);
  return 0;
}
// 指定ラベル位置へ移動
void moveTo(int label) {
  L6470_goto(indexToStep(label));
  L6470_busydelay(1000);
}

// ─────────────────────────────────────
// SETUP
// ─────────────────────────────────────
void setup() {
  delay(1000);
  pinMode(PIN_SPI_MOSI, OUTPUT);
  pinMode(PIN_SPI_MISO, INPUT);
  pinMode(PIN_SPI_SCK,  OUTPUT);
  pinMode(PIN_SPI_SS,   OUTPUT);
  pinMode(PIN_BUSY,     INPUT);
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE3));
  Serial.begin(9600);
  digitalWrite(PIN_SPI_SS, HIGH);

  L6470_resetdevice();
  L6470_setup();
  L6470_goto(0);
  L6470_busydelay(1000);

  Serial.println(F("数値ラベル (0-8) を入力して改行すると、その位置へ移動します"));
}

// ─────────────────────────────────────
// LOOP
// ─────────────────────────────────────
void loop() {
  static String buf;

  if (!Serial.available()) return;

  char ch = Serial.read();
  if (ch == '\n' || ch == '\r') {
    if (buf.length() == 0) return;

    int label = buf.toInt();   // 文字列→整数
    if (label < 0 || label > 8) {
      Serial.println(F("0〜8 の数字を入力してください"));
    } else {
      moveTo(label);
      Serial.print(F("Moved to label "));
      Serial.println(label);
    }
    buf = "";
  } else {
    buf += ch;                 // 改行以外をバッファへ
  }
}

// ─────────────────────────────────────
// L6470 設定 (元コードと同じ)
// ─────────────────────────────────────
void L6470_setup() {
  L6470_setparam_acc(0x40);
  L6470_setparam_dec(0x40);
  L6470_setparam_maxspeed(0x40);
  L6470_setparam_minspeed(0x01);
  L6470_setparam_fsspd(0x3ff);
  L6470_setparam_kvalhold(0x50);
  L6470_setparam_kvalrun(0x50);
  L6470_setparam_kvalacc(0x50);
  L6470_setparam_kvaldec(0x50);
  L6470_setparam_stepmood(0x03);
}