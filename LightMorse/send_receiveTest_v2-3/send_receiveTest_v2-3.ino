/*  LoopbackMorse.ino  ─ Arduino UNO R4 WiFi
    1. 送信:  Header → 0-7 → Footer をレーザーで発光
    2. 受信:  同じ数字列をフォトトランジスタで読み取りデコード
    ──────────────────────────────────────
    Laser   : D9  (HIGH = ON)
    Sensor  : A0  (光 > THRESH で “1” 判定)
*/

#include <Arduino.h>

/* ===== モールス仕様とタイミング ===== */
const uint16_t DOT_MS   = 100;          // ・の長さ (調整可)
const uint16_t DASH_MS  = 3 * DOT_MS;
const uint16_t SYM_GAP  = DOT_MS;       // 同一文字内 OFF
const uint16_t CHR_GAP  = 3 * DOT_MS;   // 文字間 OFF
const uint16_t MSG_GAP  = 7 * DOT_MS;   // 送信↔受信切替用 OFF

/* ===== ピン定義 ===== */
const uint8_t  LASER_PIN   = 7;
const uint8_t  SENSOR_PIN  = A0;

/* ===== 受信用閾値 (要調整) ===== */
const uint16_t THRESH              = 600;        // analogRead() ≥ で光有り
const uint16_t DASH_MS_THRESH      = DOT_MS * 2; // dot/dash 判定
const uint16_t CHR_GAP_THRESH      = DOT_MS * 2; // 文字間判定
const uint16_t MSG_GAP_THRESH      = DOT_MS * 6; // メッセージ区切り

/* ===== モールステーブル ===== */
struct Item { const char *pat; char chr; };

const Item MORSE_LIST[] = {
  {"-.-.-", 'H'},                           // Header ("KA")
  {"-----", '0'}, {".----", '1'}, {"..---", '2'},
  {"...--", '3'}, {"....-", '4'}, {".....", '5'},
  {"-....", '6'}, {"--...", '7'},
  {"...-.-", 'F'}                           // Footer ("SK")
};
const uint8_t LIST_LEN = sizeof(MORSE_LIST) / sizeof(Item);

/* ------------------------------------------------------------------
   送信側ユーティリティ
------------------------------------------------------------------ */
inline void laserOn () { digitalWrite(LASER_PIN, HIGH); }
inline void laserOff() { digitalWrite(LASER_PIN, LOW);  }

void sendDot () { laserOn();  delay(DOT_MS);  laserOff(); delay(SYM_GAP); }
void sendDash() { laserOn();  delay(DASH_MS); laserOff(); delay(SYM_GAP); }

void sendPattern(const char *p)
{
  while (*p) (*p++ == '.') ? sendDot() : sendDash();
  delay(CHR_GAP - SYM_GAP);                 // 文字終端
}

void transmitMessage()
{
  Serial.println("<TX> Header-0-7-Footer");
  for (uint8_t i = 0; i < LIST_LEN; ++i) {
    sendPattern(MORSE_LIST[i].pat);
  }
  laserOff();
}

/* ------------------------------------------------------------------
   受信側ユーティリティ
------------------------------------------------------------------ */
char decodePattern(const String &pat)
{
  for (auto &item : MORSE_LIST)
    if (pat == item.pat) return item.chr;
  return '?';
}

String receiveMessage(uint32_t timeLimit_ms = 8000)
{
  String pat    = "";            // 現在構築中のパターン
  String digits = "";            // 受信した 0-7 を順次格納
  bool headerSeen = false;

  uint32_t start = millis();
  bool lightPrev = false;
  uint32_t lastChange = millis();

  while (millis() - start < timeLimit_ms) {
    int val = analogRead(SENSOR_PIN);
    bool lightNow = (val >= THRESH);

    if (lightNow != lightPrev) {
      uint32_t now = millis();
      uint32_t dur = now - lastChange;
      lastChange = now;

      if (lightPrev) {                           // ON → OFF : dot/dash 決定
        if      (dur < DASH_MS_THRESH) pat += '.';
        else                           pat += '-';
      } else {                                   // OFF → ON : 区切り判定
        if (dur >= MSG_GAP_THRESH && pat.length()) {
          char c = decodePattern(pat);
          if (c == 'H') { headerSeen = true; digits = ""; }
          else if (c == 'F' && headerSeen)  return digits;   // 完了
          else if (headerSeen && isDigit(c)) digits += c;
          pat = "";
        } else if (dur >= CHR_GAP_THRESH && pat.length()) {
          char c = decodePattern(pat);
          if (headerSeen && isDigit(c))     digits += c;
          else if (c == 'H')                headerSeen = true;
          else if (c == 'F' && headerSeen)  return digits;
          pat = "";
        }
      }
      lightPrev = lightNow;
    }
  }
  return "(timeout)";
}

/* ------------------------------------------------------------------
   setup / loop
------------------------------------------------------------------ */
void setup()
{
  pinMode(LASER_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
  laserOff();

  Serial.begin(115200);
  Serial.println("LoopbackMorse start");
}

void loop()
{
  /* 1 → 送信 */
  transmitMessage();
  delay(MSG_GAP);                 // 送信終了後、受信側へ切り替える余裕

  /* 2 → 受信 */
  Serial.println("<RX> waiting...");
  String got = receiveMessage();  // ヘッダ〜フッタを受け取るか 8 s で timeout
  Serial.print("<RX> result: "); Serial.println(got);

  /* 3 → 次の試行まで少し休む */
  delay(3000);
}