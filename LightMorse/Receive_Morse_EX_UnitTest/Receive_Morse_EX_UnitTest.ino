// arduino_r4_wifi_morse_receiver.ino
// 受信側プログラム: Arduino R4 WiFi
// フォトトランジスタで光モールス符号を受信し、デコードしてシリアル出力します。
// モールス符号を一つでも受信したら、その時点で数値を出力
// ヘッダーとフッターを受信したら、その時点でHeader detectedやFooter detectedを出力
// 認識した数値をint getNum[]配列に格納し、フッター受信後にその中身を表示

// --- グローバル設定 ---
// モールス符号の基本単位時間 (ミリ秒)
// 送信側と受信側で同じ値を設定してください。
const int UNIT_TIME = 100; // 例: 100ms

// --- 受信側設定 ---
const int PHOTOTRANSISTOR_PIN = A0; // フォトトランジスタが接続されているアナログピン
const int LIGHT_THRESHOLD = 500;    // 光の閾値 (要調整: 環境光やレーザーの強度による)
const int DEBOUNCE_DELAY = 10;      // 立ち上がり/立ち下がり時のノイズ・チャタリング対策のためのデバウンス時間 (ミリ秒)
const int TIMEOUT_DURATION = 7 * UNIT_TIME; // 語間隔 (長すぎる空白を検出してメッセージ終了と判断するため)

// モールス符号解読用バッファ
char morseBuffer[80]; // 最大80符号分のドット/ダッシュを格納 (ヘッダー+データ+フッター用)
int bufferIndex = 0;

// 認識した数字を格納する配列とインデックス
// 最大で送信する数字の数に合わせてサイズを設定します。
// 例: ヘッダー + 8文字 + フッター の場合、8文字分のスペースが必要。
// 便宜上、最大20文字分の数字が格納できるとしています。
int getNum[20];
int getNumIndex = 0;

// モールス符号の定義 (ドット/ダッシュ文字列)
const char* MORSE_CODE_0_STR = "-----";
const char* MORSE_CODE_1_STR = ".----";
const char* MORSE_CODE_2_STR = "..---";
const char* MORSE_CODE_3_STR = "...--";
const char* MORSE_CODE_4_STR = "....-";
const char* MORSE_CODE_5_STR = ".....";
const char* MORSE_CODE_6_STR = "-....";
const char* MORSE_CODE_7_STR = "--...";

// ヘッダーとフッター (文字列)
// 送信側と文字列が一致している必要があります。
const char* HEADER_STR = "..---.."; // ご提示のヘッダー "..---.."
const char* FOOTER_STR = ".-.-.";     // AR:  .-.-.

// --- 受信処理用グローバル変数 ---
static unsigned long lastLightStateChangeTime = 0; // 光の状態が変化した最後の時間
static unsigned long lastLightOnTime = 0;          // 最後に光がONになった時間
static unsigned long lastLightOffTime = 0;         // 最後に光がOFFになった時間
static bool currentLightState = false;             // 現在の光の状態 (true:ON, false:OFF)

// --- 関数プロトタイプ宣言 ---
void addToMorseBuffer(char morseChar);
void resetMorseBuffer();
void addSpaceToBuffer();
void processBufferImmediately();
void resetGetNumBuffer(); // 新しく追加する関数
void displayGetNumBuffer(); // 新しく追加する関数

void setup() {
  Serial.begin(9600);
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
    // 状態が変化したか、またはデバouンス期間が終了した場合
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

// 認識した数字の配列の中身を表示
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
  } else if (strcmp(morseBuffer, FOOTER_STR) == 0) {
    Serial.println("\n--- FOOTER RECEIVED ---");
    displayGetNumBuffer(); // フッターを受信したらgetNum配列の中身を表示
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