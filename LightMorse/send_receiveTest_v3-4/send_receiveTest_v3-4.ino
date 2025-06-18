// 統合版：光モールス符号 送受信プログラム (Arduino R4 WiFi) ->入力された文字列全体に対してヘッダーとフッターを付与するプログラム

// --- グローバル設定 ---
// モールス符号の基本単位時間 (ミリ秒)
// この値を調整することで、モールス符号の速度を変更できます。送受信で同じ値を使用。
const int UNIT_TIME = 100; // 例: 100ms (送信側はこの値でパルスを生成している前提)

// --- 送信側設定 ---
const int LASER_PIN = 9; // レーザーモジュールが接続されているデジタルピン

// モールス符号の定義 (0-7)
// ドットは1、ダッシュは3
// -1: 文字内符号間隔 (ドットとダッシュの間) -> UNIT_TIME
// -2: 文字間隔 (文字と文字の間)         -> 3 * UNIT_TIME
// -3: 語間隔 (単語と単語の間)           -> 7 * UNIT_TIME (今回は使用しない)
const int MORSE_CODE_0[] = {3, 3, 3, 3, 3}; // -----
const int MORSE_CODE_1[] = {1, 3, 3, 3, 3}; // .----
const int MORSE_CODE_2[] = {1, 1, 3, 3, 3}; // ..---
const int MORSE_CODE_3[] = {1, 1, 1, 3, 3}; // ...--
const int MORSE_CODE_4[] = {1, 1, 1, 1, 3}; // ....-
const int MORSE_CODE_5[] = {1, 1, 1, 1, 1}; // .....
const int MORSE_CODE_6[] = {3, 1, 1, 1, 1}; // -....
const int MORSE_CODE_7[] = {3, 3, 1, 1, 1}; // --...

// ヘッダーとフッター
const int HEADER[] = {1, 1, 1, -1, 3, 3, 3, -1, 1, 1, 1}; // SOS: ...---...
const int FOOTER[] = {1, -1, 3, -1, 1, -1, 3, -1, 1};     // AR:  .-.-.

// 数字に対応するモールス符号のポインタ配列
const int* MORSE_CODES[] = {
    MORSE_CODE_0, MORSE_CODE_1, MORSE_CODE_2, MORSE_CODE_3,
    MORSE_CODE_4, MORSE_CODE_5, MORSE_CODE_6, MORSE_CODE_7
};

// 各モールス符号の長さ
const int MORSE_CODE_LENGTHS[] = {
    sizeof(MORSE_CODE_0) / sizeof(int), sizeof(MORSE_CODE_1) / sizeof(int),
    sizeof(MORSE_CODE_2) / sizeof(int), sizeof(MORSE_CODE_3) / sizeof(int),
    sizeof(MORSE_CODE_4) / sizeof(int), sizeof(MORSE_CODE_5) / sizeof(int),
    sizeof(MORSE_CODE_6) / sizeof(int), sizeof(MORSE_CODE_7) / sizeof(int)
};

// --- 受信側設定 ---
const int PHOTOTRANSISTOR_PIN = A0; // フォトトランジスタが接続されているアナログピン
const int LIGHT_THRESHOLD = 500;    // 光の閾値 (要調整: 環境光やレーザーの強度による)
const int DEBOUNCE_DELAY = 10;      // 立ち上がり/立ち下がり時のノイズ・チャタリング対策のためのデバウンス時間 (ミリ秒)
const int TIMEOUT_DURATION = 7 * UNIT_TIME; // 語間隔 (文字間の区切りにも使用)

// モールス符号解読用バッファ
char morseBuffer[20]; // 最大20文字分のドット/ダッシュを格納
int bufferIndex = 0;

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
const char* HEADER_STR = "...---..."; // SOS
const char* FOOTER_STR = ".-.-.";     // AR

// --- 動作モード ---
enum Mode {
  NONE,
  SENDER_MODE,
  RECEIVER_MODE
};
Mode currentMode = NONE;

// --- 関数プロトタイプ宣言 ---
void enterSenderMode();
void enterReceiverMode();
void sendMorseCode(const int* morseArray, int arrayLength);
void addToMorseBuffer(char morseChar);
void resetMorseBuffer();
void addSpaceToBuffer();
void processReceivedMorseCode();

void setup() {
  Serial.begin(9600);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // 初期状態はレーザーOFF
  Serial.println("Arduino Morse Code Integrated Program Ready.");
  Serial.println("Select mode: (S)ender or (R)eceiver");
}

void loop() {
  if (currentMode == NONE) {
    if (Serial.available() > 0) {
      char choice = Serial.read();
      if (choice == 'S' || choice == 's') {
        enterSenderMode();
      } else if (choice == 'R' || choice == 'r') {
        enterReceiverMode();
      } else {
        Serial.println("Invalid choice. Please enter 'S' or 'R'.");
      }
    }
  } else if (currentMode == SENDER_MODE) {
    // 変更点: 文字列として入力を受け取る
    if (Serial.available() > 0) {
      String receivedString = Serial.readStringUntil('\n'); // 改行まで読み込む
      receivedString.trim(); // 前後の空白を削除

      if (receivedString.equalsIgnoreCase("M")) { // モード変更コマンド
        currentMode = NONE;
        Serial.println("\n--- Mode changed. Select mode: (S)ender or (R)eceiver ---");
      } else {
        bool isValidInput = true;
        for (int i = 0; i < receivedString.length(); i++) {
          char c = receivedString.charAt(i);
          if (c < '0' || c > '7') {
            isValidInput = false;
            break;
          }
        }

        if (isValidInput && receivedString.length() > 0) {
          Serial.print("Sending Morse code for: ");
          Serial.println(receivedString);

          // ヘッダー送信
          sendMorseCode(HEADER, sizeof(HEADER) / sizeof(int));
          delay(3 * UNIT_TIME); // ヘッダーと最初の数字の間隔 (文字間隔)

          // 各数字のモールス符号を順番に送信
          for (int i = 0; i < receivedString.length(); i++) {
            int numberToSend = receivedString.charAt(i) - '0';
            sendMorseCode(MORSE_CODES[numberToSend], MORSE_CODE_LENGTHS[numberToSend]);
            if (i < receivedString.length() - 1) { // 最後の数字でなければ文字間隔を空ける
              delay(3 * UNIT_TIME); // 数字間の間隔 (文字間隔)
            }
          }

          delay(3 * UNIT_TIME); // 最後の数字とフッターの間隔 (文字間隔)

          // フッター送信
          sendMorseCode(FOOTER, sizeof(FOOTER) / sizeof(int));
          delay(7 * UNIT_TIME); // 送信完了後の待機時間 (新しいデータの準備)

          Serial.println("Transmission Complete. Enter next number sequence (0-7), or 'M' to change mode:");
        } else {
          Serial.println("Invalid input. Please enter a sequence of numbers (0-7), or 'M' to change mode.");
        }
      }
    }
  } else if (currentMode == RECEIVER_MODE) {
    static unsigned long lastLightStateChangeTime = 0; // 光の状態が変化した最後の時間
    static unsigned long lastLightOnTime = 0;          // 最後に光がONになった時間
    static unsigned long lastLightOffTime = 0;         // 最後に光がOFFになった時間
    static bool currentLightState = false;             // 現在の光の状態 (true:ON, false:OFF)

    int lightValue = analogRead(PHOTOTRANSISTOR_PIN);

    // シリアル入力によるモード切り替え
    if (Serial.available() > 0) {
      char choice = Serial.read();
      if (choice == 'M' || choice == 'm') {
        currentMode = NONE;
        resetMorseBuffer(); // 受信バッファをリセット
        Serial.println("\n--- Mode changed. Select mode: (S)ender or (R)eceiver ---");
        return; // モード変更後、残りのループ処理はスキップ
      }
    }

    // 光の状態検出とデバウンス処理
    bool newLightState;
    if (lightValue > LIGHT_THRESHOLD) {
      newLightState = true;  // 光ON
    } else {
      newLightState = false; // 光OFF
    }

    if (newLightState != currentLightState) {
      // 状態が変化したか、またはデバウンス期間が終了した場合
      if (millis() - lastLightStateChangeTime > DEBOUNCE_DELAY) {
        currentLightState = newLightState;
        lastLightStateChangeTime = millis();

        if (currentLightState) { // 光がONになった (立ち上がりエッジ)
          unsigned long noLightDuration = millis() - lastLightOffTime;

          if (noLightDuration >= TIMEOUT_DURATION) { // 語間隔以上の空白
            if (bufferIndex > 0) {
                processReceivedMorseCode(); // 以前のモールス符号を処理
            }
            resetMorseBuffer(); // バッファをリセット
            Serial.println("\n--- Word Space Detected, Resetting Buffer ---");
          } else if (noLightDuration >= (3 * UNIT_TIME - UNIT_TIME / 2)) { // 文字間隔の判定を少し広めに (約2.5UNIT_TIME以上)
            addSpaceToBuffer();
            Serial.print("   "); // シリアルモニタでの表示
          } else if (noLightDuration >= (UNIT_TIME - UNIT_TIME / 2)) { // 符号内間隔の判定を少し広めに (約0.5UNIT_TIME以上)
            // 符号内間隔は特に文字を追加しないが、シリアルモニタで区切りを表示
            Serial.print(" "); // シリアルモニタでの表示
          }
          lastLightOnTime = millis();

        } else { // 光がOFFになった (立ち下がりエッジ)
          unsigned long lightDuration = millis() - lastLightOnTime;

          // ドット: UNIT_TIME前後 (UNIT_TIME * 0.7 から UNIT_TIME * 2)
          // ダッシュ: 3 * UNIT_TIME前後 (UNIT_TIME * 2.5 から UNIT_TIME * 4)
          if (lightDuration >= (UNIT_TIME * 0.7) && lightDuration < (UNIT_TIME * 2)) { // ドットの許容範囲
            addToMorseBuffer('.');
          } else if (lightDuration >= (UNIT_TIME * 2.5) && lightDuration <= (UNIT_TIME * 4)) { // ダッシュの許容範囲
            addToMorseBuffer('-');
          }
          lastLightOffTime = millis();
        }
      }
    }

    // タイムアウトによるモールス符号の確定
    // 光がOFFの状態で、最後に光がOFFになった時間から十分な時間が経過した場合
    if (!currentLightState && (millis() - lastLightOffTime > TIMEOUT_DURATION)) {
      if (bufferIndex > 0) { // バッファに何かある場合
        processReceivedMorseCode();
        resetMorseBuffer();
        Serial.println("\n--- Timeout Detected, Processing Buffer ---");
      }
    }
    delay(1); // ポーリング間隔
  }
}

// --- 送信側関数 ---
void enterSenderMode() {
  currentMode = SENDER_MODE;
  Serial.println("\n--- Entering SENDER Mode ---");
  Serial.println("Enter a sequence of numbers (0-7), or 'M' to change mode:");
  digitalWrite(LASER_PIN, LOW); // 送信モード開始時もレーザーをオフに
}

// モールス符号を送信する関数 (変更なし)
void sendMorseCode(const int* morseArray, int arrayLength) {
  for (int i = 0; i < arrayLength; i++) {
    int code = morseArray[i];

    if (code == 1) { // ドット (dit)
      digitalWrite(LASER_PIN, HIGH);
      delay(UNIT_TIME);
      digitalWrite(LASER_PIN, LOW);
      Serial.print(".");
    } else if (code == 3) { // ダッシュ (dah)
      digitalWrite(LASER_PIN, HIGH);
      delay(3 * UNIT_TIME);
      digitalWrite(LASER_PIN, LOW);
      Serial.print("-");
    } else if (code == -1) { // 文字内符号間隔
      digitalWrite(LASER_PIN, LOW); // レーザーOFFを維持
      delay(UNIT_TIME);
    } else if (code == -2) { // 文字間隔
      digitalWrite(LASER_PIN, LOW); // レーザーOFFを維持
      delay(3 * UNIT_TIME);
      Serial.print("   "); // デバッグ用
    }

    if (i < arrayLength - 1 && morseArray[i] > 0 && morseArray[i+1] > 0) {
      // 次の符号が点または線の場合、符号間隔を空ける
      delay(UNIT_TIME);
    }
  }
  Serial.println(); // 1つのモールス符号送信後、改行
}

// --- 受信側関数 (変更なし) ---
void enterReceiverMode() {
  currentMode = RECEIVER_MODE;
  Serial.println("\n--- Entering RECEIVER Mode ---");
  Serial.println("Waiting for Morse code... (Press 'M' to change mode)");
  resetMorseBuffer(); // 受信モード開始時にバッファをリセット
}

// モールス符号バッファに文字を追加 (変更なし)
void addToMorseBuffer(char morseChar) {
  if (bufferIndex < sizeof(morseBuffer) - 1) {
    morseBuffer[bufferIndex++] = morseChar;
    morseBuffer[bufferIndex] = '\0'; // Null-terminate
    Serial.print(morseChar);
  }
}

// モールス符号バッファをリセット (変更なし)
void resetMorseBuffer() {
  memset(morseBuffer, 0, sizeof(morseBuffer));
  bufferIndex = 0;
}

// モールス符号バッファにスペースを追加 (文字間の区切り) (変更なし)
void addSpaceToBuffer() {
  if (bufferIndex > 0 && morseBuffer[bufferIndex - 1] != ' ') { // 連続するスペースを避ける
    if (bufferIndex < sizeof(morseBuffer) - 1) {
      morseBuffer[bufferIndex++] = ' ';
      morseBuffer[bufferIndex] = '\0';
    }
  }
}

// 受信したモールス符号を処理し、対応する数字を出力 (変更なし)
void processReceivedMorseCode() {
  if (strcmp(morseBuffer, HEADER_STR) == 0) {
    Serial.println("\n--- HEADER RECEIVED ---");
  } else if (strcmp(morseBuffer, FOOTER_STR) == 0) {
    Serial.println("\n--- FOOTER RECEIVED ---");
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
        found = true;
        break;
      }
    }
    if (!found) {
      Serial.print("\nUnknown Morse Code: ");
      Serial.println(morseBuffer);
    }
  }
}