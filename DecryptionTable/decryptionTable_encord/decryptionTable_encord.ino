#include "Arduino_LED_Matrix.h"
ArduinoLEDMatrix matrix;

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


// findPos関数　指定文字 c の (row,col) を探す。見つかったら true を返し、参照引数 r,c に代入。
bool findPos(char c, uint8_t &r, uint8_t &cidx)
{
  for (r = 0; r < 8; r++) {
    for (cidx = 0; cidx < 8; cidx++) {
      if (pgm_read_byte(&glyph[r][cidx]) == c) 
      return true;       //復号テーブル内に該当の文字がある場合はtrueを返す
    }
  }
  return false;          // テーブルに存在しない場合にはfalseを返す
}

// Arduino側のプログラム

void setup(){
  Serial.begin(9600);
  while(!Serial){}

  Serial.println(F("使用可能は文字列はA-Z,a-z,!?,.:;_@$% です"));
  Serial.println(F("入力して改行するその文字列に対応した行と列のペアを出力します"));
}

void loop(){
  static String buf;

  if(!Serial.available())
  return;

  char ch = Serial.read();
  if (ch == '\n' || ch == '\r') {
    if (buf.length() == 0) 
    return;           
   bool flag = true;

    for (uint16_t i = 0; i < buf.length(); i++) {
      uint8_t row, col;
      if (findPos(buf[i], row, col)) {
        Serial.print(row);
        Serial.print(',');
        Serial.print(col);
        if (i < buf.length() - 1) Serial.print(',');
      } else {
        Serial.println();
        Serial.print(F("対応していない文字「"));
        Serial.print(buf[i]);
        Serial.println(F("」が含まれています。もう一度入力してください。"));
        flag = false;
        break;
      }
    }
    if (flag = true) Serial.println();             // 正常終了で改行
    buf = "";
  }
  /* 改行以外の文字はバッファに追加 */
  else {
    buf += ch;
  }
}