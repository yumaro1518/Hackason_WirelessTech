
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


// 行と列から文字を取得する関数
char getChar(uint8_t r, uint8_t c) {
  if (r >= 8 || c >= 8) return '?'; // 範囲外なら ? を返す
  return pgm_read_byte(&glyph[r][c]);
}

void setup(){
  Serial.begin(9600);
  while(!Serial){}

  Serial.println(F("使用可能は文字列はA-Z,a-z,!?,.:;_@$% です"));
  Serial.println(F("入力して改行するその文字列に対応した行と列のペアを出力します"));
}

void loop() {
  static String buf;

  if (!Serial.available()) return;

  char ch = Serial.read();
  if (ch == '\n' || ch == '\r') {
    if (buf.length() == 0) return;

    // 数値→文字復元処理（カンマ区切りの偶数要素でrow, colを取得）
    uint8_t values[64]; // 最大32文字分
    uint8_t count = 0;
    char *token = strtok((char*)buf.c_str(), ",");
    while (token != NULL && count < 64) {
      values[count++] = atoi(token);
      token = strtok(NULL, ",");
    }

    if (count % 2 != 0) {
      Serial.println(F("ペアになっていない数値があります。"));
    } else {
      for (uint8_t i = 0; i < count; i += 2) {
        char c = getChar(values[i], values[i + 1]);
        Serial.print(c);
      }
      Serial.println(); // 出力の改行
    }

    buf = "";
  } else {
    buf += ch;
  }
}

