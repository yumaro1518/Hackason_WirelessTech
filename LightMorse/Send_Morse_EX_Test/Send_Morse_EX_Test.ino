// arduino_r4_wifi_morse_single_send.ino
// 一回だけ送信するプログラム: Arduino R4 WiFi
// 決められた文字列（例: "01234567"）全体にヘッダーとフッターを付与し、
// モールス符号としてレーザー送信します。送信は一度だけ行われます。

// --- グローバル設定 ---
// モールス符号の基本単位時間 (ミリ秒)
// この値を調整することで、モールス符号の速度を変更できます。
const int UNIT_TIME = 80; // 例: 100ms

// --- 送信側設定 ---
const int LASER_PIN = 9; // レーザーモジュールが接続されているデジタルピン

// 送信する固定の数値シーケンス
const String DATA_TO_SEND = "3050544142504432773056"; // ここで送信したい数字の並びを設定します

// モールス符号の定義 (0-7)
// ドットは1、ダッシュは3
// -1: 文字内符号間隔 (ドットとダッシュの間) -> UNIT_TIME
// -2: 文字間隔 (文字と文字の間)         -> 3 * UNIT_TIME
//const int MORSE_CODE_0[] = {3, 3, 3, 3, 3}; // -----
//const int MORSE_CODE_1[] = {1, 3, 3, 3, 3}; // .----
//const int MORSE_CODE_2[] = {1, 1, 3, 3, 3}; // ..---
//const int MORSE_CODE_3[] = {1, 1, 1, 3, 3}; // ...--
//const int MORSE_CODE_4[] = {1, 1, 1, 1, 3}; // ....-
//const int MORSE_CODE_5[] = {1, 1, 1, 1, 1}; // .....
//const int MORSE_CODE_6[] = {3, 1, 1, 1, 1}; // -....
//const int MORSE_CODE_7[] = {3, 3, 1, 1, 1}; // --...

const int MORSE_CODE_0[] = {3, 3, 3}; // -----
const int MORSE_CODE_1[] = {3, 3, 1}; // .----
const int MORSE_CODE_2[] = {3, 1, 3}; // ..---
const int MORSE_CODE_3[] = {3, 1, 1}; // ...--
const int MORSE_CODE_4[] = {1, 3, 3}; // ....-
const int MORSE_CODE_5[] = {1, 3, 1}; // .....
const int MORSE_CODE_6[] = {1, 1, 3}; // -....
const int MORSE_CODE_7[] = {1, 1, 1}; // --...

// ヘッダーとフッター (整数配列形式)
// モールス符号のITU-R勧告ではSOSは"...---..."です。
// ご提示いただいたヘッダーはSOSではなく、別の符号のようですので、元の定義を尊重します。
const int HEADER[] = {1, 1, -1, 3, 3, 3, -1, 1, 1}; // ご提示のヘッダー "..---.."
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

// --- 関数プロトタイプ宣言 ---
void sendMorseCode(const int* morseArray, int arrayLength);

void setup() {
  Serial.begin(9600);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // 初期状態はレーザーOFF
  Serial.println("Arduino Morse Code SINGLE SENDER Ready.");

  // ここで一度だけ送信処理を実行します
  Serial.print("Sending Morse code for: ");
  Serial.println(DATA_TO_SEND);

  // ヘッダー送信
  sendMorseCode(HEADER, sizeof(HEADER) / sizeof(int));
  delay(3 * UNIT_TIME); // ヘッダーと最初の数字の間隔 (文字間隔)

  // 各数字のモールス符号を順番に送信
  for (int i = 0; i < DATA_TO_SEND.length(); i++) {
    int numberToSend = DATA_TO_SEND.charAt(i) - '0';
    sendMorseCode(MORSE_CODES[numberToSend], MORSE_CODE_LENGTHS[numberToSend]);
    if (i < DATA_TO_SEND.length() - 1) { // 最後の数字でなければ文字間隔を空ける
      delay(3 * UNIT_TIME); // 数字間の間隔 (文字間隔)
    }
  }

  delay(3 * UNIT_TIME); // 最後の数字とフッターの間隔 (文字間隔)

  // フッター送信
  sendMorseCode(FOOTER, sizeof(FOOTER) / sizeof(int));
  delay(7 * UNIT_TIME); // 送信完了後の待機時間

  Serial.println("Transmission Complete. The program will now idle.");
}

void loop() {
  // setup() で一度だけ送信が完了しているので、loop() では何もせず待機します。
  // 必要であれば、レーザーが確実にOFFになっていることを確認するコードなどを追加できます。
  digitalWrite(LASER_PIN, LOW); // 念のためレーザーをOFFに保つ
  delay(100); // 無駄なループを防ぐための短い遅延
}

// モールス符号を送信する関数 (変更なし)
void sendMorseCode(const int* morseArray, int arrayLength) {
  for (int i = 0; i < arrayLength; i++) {
    int code = morseArray[i];

    if (code == 1) { // ドット (dit)
      digitalWrite(LASER_PIN, HIGH);
      Serial.print("."); // デバッグ用
      delay(UNIT_TIME);
      digitalWrite(LASER_PIN, LOW);
      Serial.print(" "); // デバッグ用
    } else if (code == 3) { // ダッシュ (dah)
      digitalWrite(LASER_PIN, HIGH);
      Serial.print("-"); // デバッグ用
      delay(3 * UNIT_TIME);
      digitalWrite(LASER_PIN, LOW);
      Serial.print(" "); // デバッグ用
    } else if (code == -1) { // 文字内符号間隔
      digitalWrite(LASER_PIN, LOW); // レーザーOFFを維持
      delay(UNIT_TIME);
      Serial.print(" "); // デバッグ用
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