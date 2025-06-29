#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <vector>

// TCS34725センサーの初期化。統合時間とゲインを設定
Adafruit_TCS34725 tcs(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// 色コードの定義（送信側と合わせる）
const int COLOR_RED        = 0;  // 赤色
const int COLOR_LOWBLUE       = 1;  // 青色
const int COLOR_LOWGREEN      = 2;  // 緑色
const int COLOR_CYAN       = 3;  // シアン色
const int COLOR_BLUE    = 4;  // 薄い青色（ヘッダー）
const int COLOR_GREEN   = 5;  // 薄い緑色（フッター）
const int COLOR_LOWCYAN    = 6;  // 薄いシアン色（区切り色）
const int COLOR_OFF        = 7;  // 新たに追加: LED消灯状態を示すコード

// ヘッダー、フッター、セパレーターの定義
const int HEADER_CODE    = COLOR_BLUE;    // ヘッダーは薄い青
const int FOOTER_CODE    = COLOR_GREEN;   // フッターは薄い緑
const int SEPARATOR_CODE = COLOR_LOWCYAN;    // 区切りは薄いシアン

// データに変換されるカラーペアとそれに対応する数値（送信側と合わせる）
const int colorPairs[8][2] = {
  {0, 1}, {0, 2}, {0, 3}, // 0: 赤+青, 1: 赤+緑, 2: 赤+シアン
  {1, 0}, {1, 2}, {1, 3}, // 3: 青+赤, 4: 青+緑, 5: 青+シアン
  {2, 0}, {2, 1}          // 6: 緑+赤, 7: 緑+青
};

// 受信状態を管理する列挙型
enum ReceiveState {
  STATE_WAIT_FOR_HEADER,
  STATE_RECEIVING_DATA_COLOR1,
  STATE_RECEIVING_DATA_COLOR2,
  STATE_WAIT_FOR_DATA_SEPARATOR,
  STATE_COMPLETE
};

ReceiveState currentState = STATE_WAIT_FOR_HEADER;
int detectedColorA = -1;
int detectedColorB = -1;

int previousDetectedCodeInLoop = -1;

std::vector<int> receivedData;

// === 文字変換用の定義 ===
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
// === 文字変換用の定義終わり ===


void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!tcs.begin()) {
    Serial.println("Sensor not found! Check wiring.");
    while (1);
  }
  Serial.println("--- Sensor found. Waiting for header ---");
}

int detectColor(uint16_t r, uint16_t g, uint16_t b, uint16_t c_val) {
  if (c_val <= 24) { // C値24以下を消灯状態とみなす
    return COLOR_OFF;
  }

  // 0 (赤) の閾値:
  if (r >= 14 && r <= 24 &&
      g >= 3 && g <= 9 &&
      b >= 2 && b <= 8 &&
      c_val >= 25 && c_val <= 50) return COLOR_RED;

  // 1 (青) の閾値
  if (r >= 8 && r <= 18 &&
      g >= 74 && g <= 84 &&
      b >= 246 && b <= 266 &&
      c_val >= 345 && c_val <= 365) return COLOR_BLUE;

  // 2 (緑) の閾値
  if (r >= 11 && r <= 27 &&
      g >= 75 && g <= 101 &&
      b >= 17 && b <= 43 &&
      c_val >= 129 && c_val <= 155) return COLOR_GREEN;//プラマイ３

  // 3 (シアン) の閾値
  if (r >= 17 && r <= 27 &&
      g >= 154 && g <= 174 &&
      b >= 280 && b <= 300 &&
      c_val >= 474 && c_val <= 494) return COLOR_CYAN;

  // 4 (薄い青 - ヘッダー) の閾値
  if (r >= 4 && r <= 20 &&
      g >= 36 && g <= 52 &&
      b >= 120 && b <= 146 &&
      c_val >= 180 && c_val <= 200) return COLOR_LOWBLUE;//プラマイ３

  // 5 (薄い緑 - フッター) の閾値
  if (r >= 6 && r <= 22 &&
      g >= 34 && g <= 60 &&
      b >= 9 && b <= 25 &&
      c_val >= 67 && c_val <= 93) return COLOR_LOWGREEN;//プラマイ３

  // 6 (薄いシアン - 区切り) の閾値
  if (r >= 11 && r <= 21 &&
      g >= 75 && g <= 95 &&
      b >= 137 && b <= 160 &&
      c_val >= 231 && c_val <= 273) return COLOR_LOWCYAN;//プラマイ9

  return -1; // どの色も検出されない場合（消灯以外で定義外のRGB値）
}

int getCodeFromPair(int c1, int c2) {
  for (int i = 0; i < 8; i++) {
    if (c1 == colorPairs[i][0] && c2 == colorPairs[i][1]) {
      return i;
    }
  }
  return -1;
}

void loop() {
  if (currentState == STATE_COMPLETE) {
    Serial.println("--- TRANSMISSION COMPLETE ---");
    Serial.print("Received Data Codes: ");
    for (size_t i = 0; i < receivedData.size(); ++i) {
      Serial.print(receivedData[i]);
      if (i < receivedData.size() - 1) {
        Serial.print(", ");
      }
    }
    Serial.println(); // データコード出力の改行


// receivedDataから文字を復元して出力
    Serial.print("Converted Text (Pair-wise): ");
    // receivedDataのサイズが奇数の場合、最後の数値はペアになれないため無視されます
    for (size_t i = 0; i < receivedData.size(); i += 2) {
      if (i + 1 < receivedData.size()) { // i+1が存在することを確認
        int row = receivedData[i];     // receivedDataの現在の値を行として使用
        int col = receivedData[i+1];   // receivedDataの次の値を列として使用

        // 行と列が0-7の範囲内にあるか確認（glyphテーブルのサイズに基づく）
        if (row >= 0 && row < 8 && col >= 0 && col < 8) {
          Serial.print(getChar(row, col));
        } else {
          Serial.print("?"); // 範囲外の場合は'?'を出力
        }
      } else {
        Serial.println("\nWarning: Last data code has no pair. Ignoring.");
      }
    }
    Serial.println(); // 復元された文字の改行

    Serial.println("Program Halted."); // プログラム停止メッセージ
    while(true); // ここでプログラムを完全に停止させる
  }

  uint16_t r, g, b, c_val;
  tcs.getRawData(&r, &g, &b, &c_val);

  int currentDetectedCode = detectColor(r, g, b, c_val);

  // 前回ループで処理したコードと同じであれば、何もせずに次のループへ進む。
  // ただし、COLOR_OFFまたは-1からの変化は常に新しい検出とみなす。
  // -1 の場合はシリアル出力しないため、previousDetectedCodeInLoop は -1 には更新しない。
  // そのため、ここでは currentDetectedCode が -1 ではないことと、
  // currentDetectedCode が previousDetectedCodeInLoop と同じであればスキップする。
  if (currentDetectedCode != -1 && currentDetectedCode == previousDetectedCodeInLoop) {
      return;
  }

  // COLOR_OFFが検出された場合、previousDetectedCodeInLoopを更新し、次のループへ進む
  if (currentDetectedCode == COLOR_OFF) {
      previousDetectedCodeInLoop = COLOR_OFF;
      return;
  }

  // currentDetectedCode が -1 の場合は、シリアル出力もせず、
  // previousDetectedCodeInLoop も更新せず、次のループへ進む。
  // これにより、-1 の出力が抑制され、かつ -1 が挟まっても連続検出防止ロジックが破綻しない。
  if (currentDetectedCode == -1) {
      return;
  }

  // ここに到達したということは、新しい有効な色（-1ではない、COLOR_OFFではない）が検出されたか、
  // またはCOLOR_OFFから別の有効な色に切り替わったということ。
  // この時のみ、シリアル出力と状態遷移処理を行う。
  previousDetectedCodeInLoop = currentDetectedCode; // 今回処理する有効な色を記憶

  switch (currentState) {
    case STATE_WAIT_FOR_HEADER:
      if (currentDetectedCode == HEADER_CODE) {
        Serial.println("--- HEADER DETECTED ---");
        currentState = STATE_RECEIVING_DATA_COLOR1;
      } else { // -1 は既にフィルタされているので、ここに来るのは「予期せぬ有効な色」
        Serial.println("Waiting for Header. Unexpected color detected.");
      }
      break;

    case STATE_RECEIVING_DATA_COLOR1:
      if (currentDetectedCode == FOOTER_CODE) {
        Serial.println("--- FOOTER DETECTED1 ---");
        currentState = STATE_COMPLETE;
      } else if (currentDetectedCode == SEPARATOR_CODE) {
        Serial.println("ERROR: Unexpected SEPARATOR in DATA_COLOR1. Resetting.");
        detectedColorA = -1;
        detectedColorB = -1;
        receivedData.clear();
        currentState = STATE_WAIT_FOR_HEADER;
      } else if (currentDetectedCode >= COLOR_RED && currentDetectedCode <= COLOR_CYAN) {
        detectedColorA = currentDetectedCode;
        currentState = STATE_RECEIVING_DATA_COLOR2;
      } else { // -1 は既にフィルタされているので、ここに来るのは「予期せぬ有効な色」
        Serial.println("ERROR: Unexpected color in DATA_COLOR1. Resetting.");
        detectedColorA = -1;
        detectedColorB = -1;
        receivedData.clear();
        currentState = STATE_WAIT_FOR_HEADER;
      }
      break;

    case STATE_RECEIVING_DATA_COLOR2:
      if (currentDetectedCode == FOOTER_CODE) {
        Serial.println("--- FOOTER DETECTED2 ---");
        currentState = STATE_COMPLETE;
      } else if (currentDetectedCode == SEPARATOR_CODE) {
        Serial.println("ERROR: Unexpected SEPARATOR in DATA_COLOR2. Resetting.");
        detectedColorA = -1;
        detectedColorB = -1;
        receivedData.clear();
        currentState = STATE_WAIT_FOR_HEADER;
      } else if (currentDetectedCode >= COLOR_RED && currentDetectedCode <= COLOR_CYAN) {
        detectedColorB = currentDetectedCode;
        int dataCode = getCodeFromPair(detectedColorA, detectedColorB);

        if (dataCode != -1) {
          Serial.print("Converted Data Code: "); Serial.println(dataCode);
          receivedData.push_back(dataCode);
          currentState = STATE_WAIT_FOR_DATA_SEPARATOR;
        } else {
          Serial.println("ERROR: Invalid data pair detected. Resetting.");
          detectedColorA = -1;
          detectedColorB = -1;
          receivedData.clear();
          currentState = STATE_WAIT_FOR_HEADER;
        }
      } else { // -1 は既にフィルタされているので、ここに来るのは「予期せぬ有効な色」
        Serial.println("ERROR: Unexpected color in DATA_COLOR2. Resetting.");
        detectedColorA = -1;
        detectedColorB = -1;
        receivedData.clear();
        currentState = STATE_WAIT_FOR_HEADER;
      }
      break;

    case STATE_WAIT_FOR_DATA_SEPARATOR:
      if (currentDetectedCode == SEPARATOR_CODE) {
        currentState = STATE_RECEIVING_DATA_COLOR1;
        detectedColorA = -1;
        detectedColorB = -1;
      } else if (currentDetectedCode == FOOTER_CODE) {
        Serial.println("--- FOOTER DETECTED3 ---");
        currentState = STATE_COMPLETE;
      } else { // -1 は既にフィルタされているので、ここに来るのは「予期せぬ有効な色」
        Serial.println("ERROR: Expected SEPARATOR or FOOTER. Unexpected color. Resetting.");
        detectedColorA = -1;
        detectedColorB = -1;
        receivedData.clear();
        currentState = STATE_WAIT_FOR_HEADER;
      }
      break;
  }

  delay(20);
}