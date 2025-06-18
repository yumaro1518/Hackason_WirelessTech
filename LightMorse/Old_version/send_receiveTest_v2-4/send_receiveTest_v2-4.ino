/*  LoopbackMorse_fixed.ino  –  Arduino UNO R4 WiFi 1 台で
 *  レーザー送信 → フォトトランジスタ受信を順番に行うループバック実験
 *  DOT_MS=200 ms ／ THRESH=100 ／ Serial 9600 bps
 */

#include <Arduino.h>

/* ───── 送信タイミング ───── */
const uint16_t DOT_MS  = 200;
const uint16_t DASH_MS = 3 * DOT_MS;
const uint16_t SYM_GAP = DOT_MS;
const uint16_t CHR_GAP = 3 * DOT_MS;
const uint16_t MSG_GAP = 7 * DOT_MS;

/* ───── 受信判定 ───── */
const uint16_t DASH_MS_THRESH = 2 * DOT_MS;
const uint16_t CHR_GAP_THRESH = 2 * DOT_MS;
const uint16_t MSG_GAP_THRESH = 6 * DOT_MS;
const uint16_t THRESH         = 100;

/* ───── ピン ───── */
const uint8_t LASER_PIN  = 7;
const uint8_t SENSOR_PIN = A0;

/* ───── モールス表 ───── */
struct Item { const char* pat; char chr; };
const Item MORSE_LIST[] = {
  { "-.-.-", 'H' },
  { "-----", '0' }, { ".----", '1' }, { "..---", '2' },
  { "...--", '3' }, { "....-", '4' }, { ".....", '5' },
  { "-....", '6' }, { "--...", '7' },
  { "...-.-", 'F' }
};
const uint8_t LIST_LEN = sizeof(MORSE_LIST) / sizeof(Item);

/* ───── 送信ユーティリティ ───── */
inline void laserOn()  { digitalWrite(LASER_PIN, HIGH); }
inline void laserOff() { digitalWrite(LASER_PIN, LOW);  }

void sendDot () { laserOn(); delay(DOT_MS);  laserOff(); delay(SYM_GAP); }
void sendDash() { laserOn(); delay(DASH_MS); laserOff(); delay(SYM_GAP); }

void sendPattern(const char* p) {
  for (; *p; ++p) (*p == '.') ? sendDot() : sendDash();
  delay(CHR_GAP - SYM_GAP);
}

void transmitMessage() {
  Serial.println(F("<TX> Header-0-7-Footer"));
  for (uint8_t i = 0; i < LIST_LEN; ++i) sendPattern(MORSE_LIST[i].pat);
  laserOff();
}

/* ───── 受信ユーティリティ ───── */
char decodePattern(const String& pat) {
  for (const auto& it : MORSE_LIST) if (pat == it.pat) return it.chr;
  return '?';
}

String receiveMessage(uint32_t timeout_ms = 8000) {
  String pat = "";
  String digits = "";
  bool headerSeen = false;

  uint32_t t0 = millis(), lastChange = t0;
  bool lightPrev = false;

  while (millis() - t0 < timeout_ms) {
    uint16_t v = analogRead(SENSOR_PIN);
    bool lightNow = (v >= THRESH);

    if (lightNow != lightPrev) {
      uint32_t now = millis();
      uint32_t dur = now - lastChange;
      lastChange = now;

      if (lightPrev) {                               // ON→OFF
        pat += (dur < DASH_MS_THRESH) ? '.' : '-';
      } else {                                       // OFF→ON
        if (dur >= MSG_GAP_THRESH && pat.length()) {
          char c = decodePattern(pat);  pat = "";
          if      (c == 'H') headerSeen = true, digits = "";
          else if (c == 'F') return headerSeen ? digits : "(no-header)";
          else if (headerSeen && isDigit(c)) digits += c;
        }
        else if (dur >= CHR_GAP_THRESH && pat.length()) {
          char c = decodePattern(pat);  pat = "";
          if      (c == 'H') headerSeen = true;
          else if (c == 'F') return headerSeen ? digits : "(no-header)";
          else if (headerSeen && isDigit(c)) digits += c;
        }
      }
      lightPrev = lightNow;
    }
  }
  return "(timeout)";
}

/* ───── setup / loop ───── */
void setup() {
  pinMode(LASER_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
  laserOff();

  Serial.begin(9600);
  Serial.println(F("LoopbackMorse_fixed start"));
}

void loop() {
  transmitMessage();
  delay(MSG_GAP);

  Serial.println(F("<RX> waiting…"));
  String result = receiveMessage();
  Serial.print(F("<RX> result: ")); Serial.println(result);

  delay(3000);
}