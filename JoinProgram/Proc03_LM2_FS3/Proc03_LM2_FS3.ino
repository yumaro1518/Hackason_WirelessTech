// arduino_r4_wifi_morse_receiver_with_color_output.ino
// 受信側プログラム兼カラー送信プログラム: Arduino R4 WiFi
// フォトトランジスタで光モールス符号を受信し、デコードしてシリアル出力します。
// モールス符号を一つでも受信したら、その時点で数値を出力
// ヘッダーとフッターを受信したら、その時点でHeader detectedやFooter detectedを出力
// 認識した数値をint getNum[]配列に格納し、フッター受信後にその中身を表示
// さらに、受信したgetNum[]配列の内容を元に、RGB LEDで色を出力します。
// 仕様変更: ヘッダー受信時のLED出力は行わず、フッター受信後にヘッダー色を出力します。

#include <Arduino.h>

// --- グローバル設定 ---
// モールス符号の基本単位時間 (ミリ秒)
// 送信側と受信側で同じ値を設定してください。
const int UNIT_TIME = 80; // 例: 100ms

// --- 受信側設定 (モールス符号用) ---
const int PHOTOTRANSISTOR_PIN = A0; // フォトトランジスタが接続されているアナログピン
const int LIGHT_THRESHOLD = 500;    // 光の閾値 (要調整: 環境光やレーザーの強度による)
const int DEBOUNCE_DELAY = 10;      // 立ち上がり/立ち下がり時のノイズ・チャタリング対策のためのデバウンス時間 (ミリ秒)
const int TIMEOUT_DURATION = 7 * UNIT_TIME; // 語間隔 (長すぎる空白を検出してメッセージ終了と判断するため)

// モールス符号解読用バッファ
char morseBuffer[200]; // 最大80符号分のドット/ダッシュを格納 (ヘッダー+データ+フッター用)
int bufferIndex = 0;

// 認識した数字を格納する配列とインデックス
// 最大で送信する数字の数に合わせてサイズを設定します。
// 例: ヘッダー + 8文字 + フッター の場合、8文字分のスペースが必要。
// 便宜上、最大20文字分の数字が格納できるとしています。
int getNum[40];
int getNumIndex = 0;

// モールス符号の定義 (ドット/ダッシュ文字列)
//const char* MORSE_CODE_0_STR = "-----";
//const char* MORSE_CODE_1_STR = ".----";
//const char* MORSE_CODE_2_STR = "..---";
//const char* MORSE_CODE_3_STR = "...--";
//const char* MORSE_CODE_4_STR = "....-";
//const char* MORSE_CODE_5_STR = ".....";
//const char* MORSE_CODE_6_STR = "-....";
//const char* MORSE_CODE_7_STR = "--...";

const char* MORSE_CODE_0_STR = "---";
const char* MORSE_CODE_1_STR = "--.";
const char* MORSE_CODE_2_STR = "-.-";
const char* MORSE_CODE_3_STR = "-..";
const char* MORSE_CODE_4_STR = ".--";
const char* MORSE_CODE_5_STR = ".-.";
const char* MORSE_CODE_6_STR = "..-";
const char* MORSE_CODE_7_STR = "...";

// ヘッダーとフッター (文字列)
// 送信側と文字列が一致している必要があります。
const char* HEADER_STR = "..---.."; // ご提示のヘッダー "..---.."
const char* FOOTER_STR = ".-.-.";     // AR:  .-.-.

// --- 受信処理用グローバル変数 ---
static unsigned long lastLightStateChangeTime = 0; // 光の状態が変化した最後の時間
static unsigned long lastLightOnTime = 0;          // 最後に光がONになった時間
static unsigned long lastLightOffTime = 0;         // 最後に光がOFFになった時間
static bool currentLightState = false;             // 現在の光の状態 (true:ON, false:OFF)

// --- RGB LED出力側設定 ---
const int R_PIN = 9;  // RGB LEDの赤色ピン
const int G_PIN = 3;  // RGB LEDの緑色ピン
const int B_PIN = 11; // RGB LEDの青色ピン

// 色コードの定義 (送信側と合わせる)
const int COLOR_RED        = 0;
const int COLOR_LOWBLUE    = 1;
const int COLOR_LOWGREEN   = 2;
const int COLOR_CYAN       = 3;
const int COLOR_BLUE       = 4; // ヘッダーの色として使用
const int COLOR_GREEN      = 5; // フッターの色として使用
const int COLOR_LOWCYAN    = 6; // 区切り色として使用

// データに変換されるカラーペア (送信側と合わせる)
const int colorPairs[8][2] = {
  {COLOR_RED, COLOR_LOWBLUE},   // 0: 赤+薄い青
  {COLOR_RED, COLOR_LOWGREEN},  // 1: 赤+薄い緑
  {COLOR_RED, COLOR_CYAN},      // 2: 赤+シアン
  {COLOR_LOWBLUE, COLOR_RED},   // 3: 薄い青+赤
  {COLOR_LOWBLUE, COLOR_LOWGREEN},// 4: 薄い青+薄い緑
  {COLOR_LOWBLUE, COLOR_CYAN},  // 5: 薄い青+シアン
  {COLOR_LOWGREEN, COLOR_RED},  // 6: 薄い緑+赤
  {COLOR_LOWGREEN, COLOR_LOWBLUE}// 7: 薄い緑+薄い青
};

// 発光時間を調整 (ミリ秒)
const int LIGHT_ON_DURATION = 300; // 各色1.5秒点灯
const int INTERVAL_DURATION = 100;  // 色間の消灯時間

// --- 関数プロトタイプ宣言 ---
void addToMorseBuffer(char morseChar);
void resetMorseBuffer();
void addSpaceToBuffer();
void processBufferImmediately();
void resetGetNumBuffer();
void displayGetNumBuffer();
void turnOffLights(); // RGB LED用
void sendColor(int code); // RGB LED用
void outputColorsFromGetNum(); // 新しく追加する関数


void setup() {
  Serial.begin(9600);
  // フォトトランジスタのピン設定は不要 (アナログ入力なので)

  // RGB LEDのピンを出力モードに設定
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  turnOffLights(); // 初期状態はLED消灯

  Serial.println("Arduino Morse Code RECEIVER Ready.");
  Serial.println("Waiting for Morse code...");

  // 受信処理用の初期設定
  resetMorseBuffer();
  resetGetNumBuffer(); // getNum配列も初期化
  lastLightStateChangeTime = millis();
  lastLightOffTime = millis(); // 初期状態は光OFFとみなす
  currentLightState = false;

  // 初期値のADC値を表示して、LIGHT_THRESHOLD調整の参考に
  Serial.print("Initial A0 value (no light): ");
  Serial.println(analogRead(PHOTOTRANSISTOR_PIN));
  Serial.print("Set LIGHT_THRESHOLD to: ");
  Serial.println(LIGHT_THRESHOLD);
  Serial.println("Make sure phototransistor is shielded from strong ambient light.");
}

void loop() {
  int lightValue = analogRead(PHOTOTRANSISTOR_PIN);

  bool newLightState;
  if (lightValue > LIGHT_THRESHOLD) {
    newLightState = true;  // 光ON
  } else {
    newLightState = false; // 光OFF
  }

  // 光の状態検出とデバウンス処理
  if (newLightState != currentLightState) {
    // 状態が変化したか、またはデバウンス期間が終了した場合
    if (millis() - lastLightStateChangeTime > DEBOUNCE_DELAY) {
      currentLightState = newLightState;
      lastLightStateChangeTime = millis();

      if (currentLightState) { // 光がONになった (立ち上がりエッジ)
        unsigned long noLightDuration = millis() - lastLightOffTime;

        // 文字間隔の検出時に、それまでのバッファ内容を処理
        if (noLightDuration >= (3 * UNIT_TIME - UNIT_TIME / 2)) { // 文字間隔またはそれ以上の空白の場合
            if (bufferIndex > 0) {
                processBufferImmediately(); // バッファを即座に処理
            }
            // その後、新しい文字のためのバッファをリセット
            resetMorseBuffer();
            Serial.print("   "); // シリアルモニタでの文字間隔表示
        } else if (noLightDuration >= (UNIT_TIME - UNIT_TIME / 2)) { // 符号内間隔 (約0.5UNIT_TIME以上)
            // 符号内間隔は特に文字を追加しないが、シリアルモニタで区切りを表示
            Serial.print(" "); // シリアルモニタでの表示
        }
        lastLightOnTime = millis();

      } else { // 光がOFFになった (立ち下がりエッジ)
        unsigned long lightDuration = millis() - lastLightOnTime;

        // ドット: UNIT_TIME前後 (UNIT_TIME * 0.7 から UNIT_TIME * 2)
        if (lightDuration >= (UNIT_TIME * 0.7) && lightDuration < (UNIT_TIME * 2)) {
          addToMorseBuffer('.');
        }
        // ダッシュ: 3 * UNIT_TIME前後 (UNIT_TIME * 2.5 から UNIT_TIME * 4)
        else if (lightDuration >= (UNIT_TIME * 2.5) && lightDuration <= (UNIT_TIME * 4)) {
          addToMorseBuffer('-');
        }
        lastLightOffTime = millis();
      }
    }
  }

  // タイムアウトによるモールス符号の確定 (メッセージ全体の終了または語間隔)
  // 光がOFFの状態で、最後に光がOFFになった時間から十分な時間が経過した場合
  if (!currentLightState && (millis() - lastLightOffTime > TIMEOUT_DURATION)) {
    if (bufferIndex > 0) { // バッファに何かある場合
      processBufferImmediately(); // タイムアウト時も即座に処理
    }
    resetMorseBuffer(); // バッファをリセット
    Serial.println("\n--- Long Silence Detected, Buffer Reset ---");
    lastLightOffTime = millis(); // タイムアウト検出後、時間をリセットして新たな検出に備える
  }
  delay(1); // ポーリング間隔
}

// --- モールス符号受信関連の関数 ---

// モールス符号バッファに文字を追加
void addToMorseBuffer(char morseChar) {
  if (bufferIndex < sizeof(morseBuffer) - 1) {
    morseBuffer[bufferIndex++] = morseChar;
    morseBuffer[bufferIndex] = '\0'; // Null-terminate
    Serial.print(morseChar);
  }
}

// モールス符号バッファをリセット
void resetMorseBuffer() {
  memset(morseBuffer, 0, sizeof(morseBuffer));
  bufferIndex = 0;
}

// 認識した数字を格納する配列をリセット
void resetGetNumBuffer() {
    memset(getNum, 0, sizeof(getNum)); // 配列全体を0でクリア
    getNumIndex = 0; // インデックスもリセット
}

// 認識した数字の配列の中身をシリアルモニタに表示
void displayGetNumBuffer() {
    Serial.print("\n--- Received Numbers: [");
    for (int i = 0; i < getNumIndex; i++) {
        Serial.print(getNum[i]);
        if (i < getNumIndex - 1) {
            Serial.print(", ");
        }
    }
    Serial.println("] ---");
}

// モールス符号バッファにスペースを追加 (文字間の区切り)
void addSpaceToBuffer() {
  if (bufferIndex > 0 && morseBuffer[bufferIndex - 1] != ' ') { // 連続するスペースを避ける
    if (bufferIndex < sizeof(morseBuffer) - 1) {
      morseBuffer[bufferIndex++] = ' ';
      morseBuffer[bufferIndex] = '\0';
    }
  }
}

// バッファの内容を即座に処理する
void processBufferImmediately() {
  if (strcmp(morseBuffer, HEADER_STR) == 0) {
    Serial.println("\n--- HEADER RECEIVED ---");
    resetGetNumBuffer(); // ヘッダーを受信したらgetNum配列をリセット
    // ここではヘッダーの単色出力は行わない
  } else if (strcmp(morseBuffer, FOOTER_STR) == 0) {
    Serial.println("\n--- FOOTER RECEIVED ---");
    // フッター受信後、ヘッダー色を出力
    sendColor(COLOR_BLUE); // ヘッダーの単色出力 (青)
    Serial.println("Outputting Header Color (BLUE) on Footer reception.");
    
    displayGetNumBuffer(); // getNum配列の中身を表示
    outputColorsFromGetNum(); // getNum配列の内容を元にRGB LEDを光らせる
    
    sendColor(COLOR_GREEN); // フッターの単色出力 (緑)
    Serial.println("Outputting Footer Color (GREEN).");
    resetGetNumBuffer(); // 表示後、getNum配列をリセット
  } else {
    bool found = false;
    for (int i = 0; i <= 7; i++) {
      const char* currentMorseCode;
      switch (i) {
        case 0: currentMorseCode = MORSE_CODE_0_STR; break;
        case 1: currentMorseCode = MORSE_CODE_1_STR; break;
        case 2: currentMorseCode = MORSE_CODE_2_STR; break;
        case 3: currentMorseCode = MORSE_CODE_3_STR; break;
        case 4: currentMorseCode = MORSE_CODE_4_STR; break;
        case 5: currentMorseCode = MORSE_CODE_5_STR; break;
        case 6: currentMorseCode = MORSE_CODE_6_STR; break;
        case 7: currentMorseCode = MORSE_CODE_7_STR; break;
        default: currentMorseCode = ""; break;
      }
      if (strcmp(morseBuffer, currentMorseCode) == 0) {
        Serial.print("\nReceived Number: ");
        Serial.println(i);
        // 数字が認識されたらgetNum配列に格納
        if (getNumIndex < sizeof(getNum) / sizeof(getNum[0])) {
            getNum[getNumIndex++] = i;
        } else {
            Serial.println("WARNING: getNum buffer full!");
        }
        found = true;
        break;
      }
    }
    if (!found) {
      // 未知のモールス符号もその都度出力
      Serial.print("\nUnknown Morse Code (Segment): ");
      Serial.println(morseBuffer);
    }
  }
}

// --- RGB LED出力関連の関数 ---

// 全てのLEDを消灯する関数
void turnOffLights() {
  analogWrite(R_PIN, 0);
  analogWrite(G_PIN, 0);
  analogWrite(B_PIN, 0);
}

// 指定された色コードに基づいてLEDを発光させる関数
void sendColor(int code) {
  int r, g, b;
  // 色コードに対応するRGB値を設定 (コモンカソードLEDを想定)
  switch (code) {
    case COLOR_RED:        r = 255; g = 0;   b = 0;   break; // 赤
    case COLOR_BLUE:       r = 0;   g = 0;   b = 255; break; // 青
    case COLOR_GREEN:      r = 0;   g = 255; b = 0;   break; // 緑
    case COLOR_CYAN:       r = 0;   g = 255; b = 255; break; // シアン
    case COLOR_LOWBLUE:    r = 0;   g = 0;   b = 128; break; // 薄い青
    case COLOR_LOWGREEN:   r = 0;   g = 128; b = 0;   break; // 薄い緑
    case COLOR_LOWCYAN:    r = 0;   g = 128; b = 128; break; // 薄いシアン
    default: r = 0; g = 0; b = 0; break; // 未定義の色は消灯
  }
  // 設定されたRGB値でLEDを点灯
  analogWrite(R_PIN, r);
  analogWrite(G_PIN, g);
  analogWrite(B_PIN, b);
  delay(LIGHT_ON_DURATION); // 指定時間点灯
  turnOffLights();          // 一度消灯
  delay(INTERVAL_DURATION); // 色間のインターバル
}

// 受信したgetNum配列の内容を元にRGB LEDを光らせる関数
void outputColorsFromGetNum() {
  Serial.println("\n--- Outputting Colors based on Received Data ---");

  // 受信したデータ配列を元に色を出力
  for (int i = 0; i < getNumIndex; i++) {
    int data_val = getNum[i];
    Serial.print("Outputting Data Pattern: ");
    Serial.println(data_val);

    // データは2色ペアで送信 (元のカラー送信プログラムのロジックに従う)
    int a = colorPairs[data_val][0];
    int b = colorPairs[data_val][1];

    Serial.print("  Color 1: ");
    Serial.println(a);
    sendColor(a); // 1色目

    Serial.print("  Color 2: ");
    Serial.println(b);
    sendColor(b); // 2色目

    // データペアの区切り色送信 (最後のデータペアの後には不要かもしれないが、元のロジックに従う)
    if (i < getNumIndex - 1) { // 最後のデータ以外にはセパレーターを挟む
      Serial.print("Outputting Separator: ");
      Serial.println(COLOR_LOWCYAN);
      sendColor(COLOR_LOWCYAN);
    }
  }

  Serial.println("--- Color Output Complete ---");
}