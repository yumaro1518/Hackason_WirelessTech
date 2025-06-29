const uint8_t  LED_PIN = 13;         // 送信用 LED
const uint16_t UNIT    = 200;        // 1 ドット長 [ms]  ← 好みに応じて変更(50 ~ 120)

/* --- ヘッダー／フッター（ITU プロシグナル） --- */
const char* HEADER = "-.-.-";  // CT (= KA) ×2 : 「送信開始」
const char* FOOTER = ".-.-.";  // AR ×2        : 「送信終了」

/* --- 0〜7 の略体パターン表 --- */
const char* NUM_PATTERNS[8] = {
  "-",       // 0  (T)
  ".-",      // 1  (A)
  "..-",     // 2  (U)
  "...-",    // 3  (V)
  "....-",   // 4
  ".....",   // 5
  "-....",   // 6
  "-..."     // 7  (B)
};

/* ========================================================= */
inline void dot () {                       // HIGH 1T
  digitalWrite(LED_PIN, HIGH); delay(UNIT);
  digitalWrite(LED_PIN, LOW ); delay(UNIT); // パーツ間 LOW 1T
}
inline void dash() {                       // HIGH 3T
  digitalWrite(LED_PIN, HIGH); delay(3*UNIT);
  digitalWrite(LED_PIN, LOW ); delay(UNIT);
}
/* ========================================================= */
void sendPattern(const char* pat)
{
  for (const char* p = pat; *p; ++p) {
    if (*p == '.') dot ();
    else if (*p == '-') dash();
    else if (*p == ' ') {                  // 文字間 LOW 3T
      digitalWrite(LED_PIN, LOW);
      delay(2*UNIT);                       // 3T-1T (直前のパーツ間 1T を含め 3T)
    }
  }
}

/* ========================================================= */
void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(9600);
  Serial.println(F("Enter 0–7 and press <return> to transmit."));
}

/* ========================================================= */
void loop()
{
  if (Serial.available()) {
    int ch = Serial.read();
    if ('0' <= ch && ch <= '7') {
      uint8_t n = ch - '0';
      Serial.print(F("TX: ")); Serial.println(n);

      sendPattern(HEADER);                   // ヘッダー
      sendPattern(NUM_PATTERNS[n]);          // 本データ
      sendPattern(FOOTER);                   // フッター
      delay(7*UNIT);                         // フレーム間サイレンス
    }
  }
}