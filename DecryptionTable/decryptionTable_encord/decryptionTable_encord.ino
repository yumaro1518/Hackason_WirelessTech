#include <Arduino.h>
#include <avr/pgmspace.h>
#include "Arduino_LED_Matrix.h"
ArduinoLEDMatrix matrix;

/* ── 8×8 復号テーブル ───────────────────────── */
const char glyph[8][8] PROGMEM = {
  { 'A','B','C','D','E','F','G','H' },
  { 'I','J','K','L','M','N','O','P' },
  { 'Q','R','S','T','U','V','W','X' },
  { 'Y','Z','a','b','c','d','e','f' },
  { 'g','h','i','j','k','l','m','n' },
  { 'o','p','q','r','s','t','u','v' },
  { 'w','x','y','z','!','?','.',',' },
  { ':',';','-','_','@','$','%',' ' }
};

/* ── (row,col) 検索  ─────────────────────────── */
bool findPos(char c, uint8_t &r, uint8_t &cidx)
{
  for (r = 0; r < 8; r++) {
    for (cidx = 0; cidx < 8; cidx++) {
      if (pgm_read_byte(&glyph[r][cidx]) == c) return true;
    }
  }
  return false;          // テーブルに存在しない
}

/* ── 行列ペア保存用バッファ  ───────────────────── */
uint8_t myArray[64];      // row,col, … 最大 32 文字分
int     arrLen = 0;

//配列確認用の関数dumpArrayAsCSV(必要に応じてコメントアウト)
void dumpArrayAsCSV()
{
  for (int i = 0; i < arrLen; ++i) {
    Serial.print(myArray[i]);
    if (i < arrLen - 1) Serial.print(',');
  }
  Serial.println();
}

void setup()
{
  Serial.begin(9600);
  while (!Serial) {}

  Serial.println(F("-- Row/Col encoder ready --"));
  Serial.println(F("使用可能文字: A-Z,a-z,!?,.:;_@$%"));
  Serial.println(F("文字列を入力→改行で (row,col) 配列を出力します"));
}

void loop()
{
  static String buf;

  if (!Serial.available()) return;

  char ch = Serial.read();

  /* 改行で 1 つの入力が完了 ------------------------------ */
  if (ch == '\n' || ch == '\r') {
    if (buf.length() == 0) return;           // 空行は無視

    arrLen = 0;                              // 新しい入力に備えてリセット
    bool ok = true;

    for (uint16_t i = 0; i < buf.length() && arrLen < 64; i++) {
      uint8_t row, col;
      if (findPos(buf[i], row, col)) {
        /* シリアル表示 */
        Serial.print(row);
        Serial.print(',');
        Serial.print(col);
        if (i < buf.length() - 1) Serial.print(',');

        /* バッファへ保存 */
        myArray[arrLen++] = row;
        myArray[arrLen++] = col;
      } else {
        Serial.println();
        Serial.print(F("!! 未対応文字「"));
        Serial.print(buf[i]);
        Serial.println(F("」が含まれています。再入力してください。"));
        ok = false;
        break;
      }
    }

    Serial.println();                        // 行末改行
    //以下のif文は必要に応じてコメントアウト
    if (ok) {
      Serial.print(F(">> myArray = { "));
      dumpArrayAsCSV();                      // 格納内容を確認表示
      Serial.println(F("}"));
    }

    buf = "";                                // バッファクリア
  }
  /* 改行以外の文字をバッファへ --------------------------- */
  else {
    buf += ch;
  }
}