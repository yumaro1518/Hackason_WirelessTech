/* =========================================================
 *  光モールス受信：略体 0–7
 *  送信側設定 ─ UNIT = 60 ms, HEADER = "-.-.-", FOOTER = ".-.-."
 * ---------------------------------------------------------
 *  回路例
 *      +5V ─┬─[10 kΩ]──┐
 *           │           ├─ A0  (フォトトランジスタ COLLECTOR)
 *  Sensor ──┘           │
 *      GND ─────────────┘   (フォトトランジスタ EMITTER)
 * ========================================================= */
#include <Arduino.h>

/* ---------- ユーザー設定 ---------- */
const uint8_t  PHOTO_PIN    = A0;   // 光センサ
const uint16_t UNIT         = 60;   // 送信側と同じドット長 [ms]
const uint16_t DOT_TOL      = 25;   // dot / dash 判定許容 [ms]
const uint16_t FRAME_TOUT   = 2000; // 無信号タイムアウト [ms]
/* ---------------------------------- */

/* パターン定義（送信側と同じ） */
const char* HEADER  = "-.-.-";      // CT (KA)
const char* FOOTER  = ".-.-.";      // AR
const char* NUM_PAT[8] = { "-", ".-", "..-", "...-", "....-", ".....",
                           "-....", "-..." };

/* しきい値は動的に求める */
uint16_t threshold = 512;

/* ---------------------------------------------------------
 *  サンプリングして HIGH/LOW 判定
 * --------------------------------------------------------- */
inline bool isHigh()
{
  return analogRead(PHOTO_PIN) > threshold;
}

/* ---------------------------------------------------------
 *  パルス / スペース長(ms) を計測
 *    wantHigh = true  : HIGH 区間長
 *    wantHigh = false : LOW  区間長
 * --------------------------------------------------------- */
uint16_t measure(bool wantHigh)
{
  uint32_t t0 = millis();
  /* 対象状態になるまで待機 */
  while (isHigh() != wantHigh) {
    if (millis() - t0 > FRAME_TOUT) return 0;
  }
  /* 長さを測定 */
  uint32_t start = millis();
  while (isHigh() == wantHigh) {
    if (millis() - start > 5000) break;          // 異常に長い
  }
  return (uint16_t)(millis() - start);
}

/* ---------------------------------------------------------
 *  dot / dash 判定
 * --------------------------------------------------------- */
char classify(uint16_t len)
{
  if (abs((int)len - (int)UNIT)       <= DOT_TOL)        return '.';
  if (abs((int)len - (int)(3*UNIT))   <= 3*DOT_TOL)      return '-';
  return '?';   // 判定不能
}

/* ---------------------------------------------------------
 *  スライディングバッファへ追加
 * --------------------------------------------------------- */
void pushChar(char *buf, uint8_t &len, char c, uint8_t maxLen = 32)
{
  if (len < maxLen) buf[len++] = c;
  else {
    memmove(buf, buf + 1, maxLen - 1);
    buf[maxLen - 1] = c;
  }
}

/* ---------------------------------------------------------
 *  キャリブレーション：環境光平均 → threshold 設定
 * --------------------------------------------------------- */
void calibrate(uint16_t ms = 400)
{
  uint32_t sum = 0; uint16_t cnt = 0; uint32_t t0 = millis();
  while (millis() - t0 < ms) { sum += analogRead(PHOTO_PIN); ++cnt; }
  threshold = (sum / cnt) + 30; // “暗”平均 + マージン
}

/* ---------------------------------------------------------
 *  HEADER 検出ループ
 *  成功時：buf クリアして true
 * --------------------------------------------------------- */
bool waitHeader()
{
  char   buf[16]; uint8_t len = 0;
  uint32_t idleT0 = millis();

  while (true) {
    uint16_t h = measure(true);   if (!h) return false;
    uint16_t l = measure(false);  if (!l) return false;

    char sym = classify(h);
    if (sym == '?')        { len = 0; continue; }    // 誤判定 → バッファ捨て
    pushChar(buf, len, sym);

    if (len >= strlen(HEADER) &&
        !strncmp(buf + len - strlen(HEADER), HEADER, strlen(HEADER)))
    {
      return true;         // 同期完了
    }

    if (millis() - idleT0 > FRAME_TOUT) return false; // 長時間何も合わない
  }
}

/* ---------------------------------------------------------
 *  フッター検出＋データ抽出
 *  返り値： 0–7 = 受信値 / 0xFF = エラー
 * --------------------------------------------------------- */
uint8_t receivePacket()
{
  char seq[32]; uint8_t len = 0;

  while (true) {
    uint16_t h = measure(true);   if (!h) return 0xFF;
    uint16_t l = measure(false);  if (!l) return 0xFF;

    char sym = classify(h);
    if (sym == '?') return 0xFF;      // ノイズ
    pushChar(seq, len, sym);

    /* フッター検出 */
    if (len >= strlen(FOOTER) &&
        !strncmp(seq + len - strlen(FOOTER), FOOTER, strlen(FOOTER)))
    {
      len -= strlen(FOOTER);          // ← データ長（ヘッダー除去済）
      seq[len] = '\0';

      /* ヘッダー直後に入ったデータ（1〜5符号以内）と一致? */
      for (uint8_t n = 0; n < 8; ++n) {
        if (!strcmp(seq, NUM_PAT[n])) return n;
      }
      return 0xFF;                    // 照合失敗
    }

    /* フレーム全体が異常に長い場合タイムアウト */
    if (len >= 25) return 0xFF;
  }
}

/* ---------------------------------------------------------
 *  SETUP
 * --------------------------------------------------------- */
void setup()
{
  Serial.begin(9600);
  pinMode(PHOTO_PIN, INPUT);
  calibrate();
  Serial.print(F("Threshold = ")); Serial.println(threshold);
  Serial.println(F("Waiting for frames..."));
}

/* ---------------------------------------------------------
 *  MAIN LOOP
 * --------------------------------------------------------- */
void loop()
{
  /* 1) ヘッダー待ち */
  if (!waitHeader())      { Serial.println(F("Header timeout")); return; }

  /* 2) データ＋フッター受信 */
  uint8_t val = receivePacket();
  if (val == 0xFF)        { Serial.println(F("Decode error"));   return; }

  Serial.print(F("Received value = "));
  Serial.println(val);

  /* 3) フレーム間サイレンスを待って再開 */
  uint32_t t0 = millis();
  while (millis() - t0 < 7 * UNIT) { if (isHigh()) t0 = millis(); }
}