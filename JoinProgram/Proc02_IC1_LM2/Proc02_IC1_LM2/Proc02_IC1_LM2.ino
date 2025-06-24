#include <Arduino.h>

// --- 距離センサーからの数値変換部分 ---
int analogPin = A0;
int sensorValue = 0;
// 距離と対応する閾値。配列をconst int[]にすることで、ROMに配置され効率的です。
const int th[10] = {570, 500, 350, 290, 260, 245, 225, 195, 184, 180}; //15cm,20cm,25cm,30cm,35cm,40cm,45cm,50cm,60cm,70cm
const int datamax = 20; // 送信する数字の最大数を増やす場合、ここも調整
int dataBuffer[datamax]; // センサーから読み取った数値を一時的に格納するバッファ
int dataCount = 0;       // dataBufferに格納された数値の数

// 0から7の数字へ変換（距離を数字にマッピング）
int thchange(int sensorValue) {
  if (sensorValue >= th[0]) { // 570以上 (15cm以下) は開始マーカーとして扱う
    return -1; // 開始
  } else if (sensorValue <= th[9]) { // 180以下 (70cm以上) は終了マーカーとして扱う
    return -2; // 終了
  } else if (sensorValue > th[1] && sensorValue <= th[0]) { // 570より小さく、500以下 -> この条件はth[0] <= sensorValueと衝突するので注意。
                                                          // th[0]を"開始閾値"、th[9]を"終了閾値"と明確に分離します。
                                                          // sensorValueの範囲がth[i+1] < sensorValue <= th[i]となるように調整
                                                          // 修正された条件
    // th[1] = 500, th[2] = 350, ... th[8]=184, th[9]=180
  }

  // 距離に応じた数字の変換ロジック
  if (sensorValue <= th[1] && sensorValue > th[2]) { // 500以下, 350より大
    return 0;
  } else if (sensorValue <= th[2] && sensorValue > th[3]) { // 350以下, 290より大
    return 1;
  } else if (sensorValue <= th[3] && sensorValue > th[4]) { // 290以下, 260より大
    return 2;
  } else if (sensorValue <= th[4] && sensorValue > th[5]) { // 260以下, 245より大
    return 3;
  } else if (sensorValue <= th[5] && sensorValue > th[6]) { // 245以下, 225より大
    return 4;
  } else if (sensorValue <= th[6] && sensorValue > th[7]) { // 225以下, 195より大
    return 5;
  } else if (sensorValue <= th[7] && sensorValue > th[8]) { // 195以下, 184より大
    return 6;
  } else if (sensorValue <= th[8] && sensorValue > th[9]) { // 184以下, 180より大
    return 7;
  } else {
    return 999; // 範囲外のエラー
  }
}

// --- モールス符号送信部分 ---
// グローバル設定
const int UNIT_TIME = 100; // 例: 100ms
const int LASER_PIN = 9;   // レーザーモジュールが接続されているデジタルピン

// モールス符号の定義 (0-7)
const int MORSE_CODE_0[] = {3, 3, 3}; // -----
const int MORSE_CODE_1[] = {3, 3, 1}; // .----
const int MORSE_CODE_2[] = {3, 1, 3}; // ..---
const int MORSE_CODE_3[] = {3, 1, 1}; // ...--
const int MORSE_CODE_4[] = {1, 3, 3}; // ....-
const int MORSE_CODE_5[] = {1, 3, 1}; // .....
const int MORSE_CODE_6[] = {1, 1, 3}; // -....
const int MORSE_CODE_7[] = {1, 1, 1}; // --...

// ヘッダーとフッター (整数配列形式)
const int HEADER[] = {1, 1, -1, 3, 3, 3, -1, 1, 1}; // "..---.."
const int FOOTER[] = {1, -1, 3, -1, 1, -1, 3, -1, 1}; // ".-.-."

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

// 関数プロトタイプ宣言
void sendMorseCode(const int* morseArray, int arrayLength);
void processAndSendMorseCode();

// --- 状態管理変数 ---
bool isReadingSensor = false; // センサーからの読み取り中かどうか
unsigned long lastSensorReadTime = 0; // 最後にセンサーを読んだ時間
const unsigned long SENSOR_READ_INTERVAL = 500; // センサー読み取り間隔 (ミリ秒)

void setup() {
  Serial.begin(9600);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // 初期状態はレーザーOFF
  Serial.println("Arduino Distance-to-Morse Sender Ready.");
  Serial.println("Waiting for sensor input...");
}

void loop() {
  // センサー読み取りの頻度を制御
  if (millis() - lastSensorReadTime < SENSOR_READ_INTERVAL) {
    // delay(SENSOR_READ_INTERVAL - (millis() - lastSensorReadTime)); // 次の読み取りまで待機
    // ↑ このdelayは非推奨。次のループで待機するように、そのままreturn;で良い
    return;
  }
  lastSensorReadTime = millis(); // 読み取り時間を更新

  sensorValue = analogRead(analogPin);
  int detectedZone = thchange(sensorValue);

  // 読み取り開始の検出 (th[0] <= sensorValue の場合)
  if (detectedZone == -1 && !isReadingSensor) {
    Serial.println("読み取り開始を検出 (センサー値: " + String(sensorValue) + ")");
    isReadingSensor = true;
    dataCount = 0; // 新しいデータの読み取り開始なのでバッファをリセット
  }
  // 読み取り中のデータ収集
  else if (isReadingSensor) {
    // 終了マーカーの検出 (th[9] >= sensorValue の場合)
    if (detectedZone == -2) {
      Serial.println("読み取り終了を検出 (センサー値: " + String(sensorValue) + ")");
      isReadingSensor = false; // 読み取り終了状態に
      if (dataCount > 0) { // データが収集されていれば送信
        processAndSendMorseCode();
      } else {
        Serial.println("No data collected before end signal.");
      }
      dataCount = 0; // 送信後バッファをリセット
    }
    // 有効な数値データの収集 (0-7)
    else if (detectedZone >= 0 && detectedZone <= 7) {
      if (dataCount < datamax) {
        dataBuffer[dataCount] = detectedZone;
        Serial.print("データ追加: ");
        Serial.print(detectedZone);
        Serial.print(" (センサー値: ");
        Serial.print(sensorValue);
        Serial.println(")");
        dataCount++;
      } else {
        Serial.println("WARNING: Data buffer full. Skipping new data.");
      }
    }
    // その他の値（999）は無視するか、エラーとして表示
    else if (detectedZone == 999) {
      Serial.println("不明なセンサー値の範囲 (センサー値: " + String(sensorValue) + ").");
    }
  }
  // センサー読み取り待機中
  else {
    Serial.print("待機中... 現在の値: ");
    Serial.println(sensorValue);
  }
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

    if (i < arrayLength - 1 && morseArray[i] > 0 && morseArray[i + 1] > 0) {
      // 次の符号が点または線の場合、符号間隔を空ける
      delay(UNIT_TIME);
    }
  }
  Serial.println(); // 1つのモールス符号送信後、改行
}

// 収集したdataBufferの内容をモールス符号として送信する関数
void processAndSendMorseCode() {
  if (dataCount == 0) {
    Serial.println("No data to send.");
    return;
  }

  // dataBufferの内容をシリアル出力で確認
  Serial.print("Sending Morse code for data: [");
  for (int i = 0; i < dataCount; i++) {
    Serial.print(dataBuffer[i]);
    if (i < dataCount - 1) {
      Serial.print(", ");
    }
  }
  Serial.println("]");

  // ヘッダー送信
  Serial.println("Sending Header...");
  sendMorseCode(HEADER, sizeof(HEADER) / sizeof(int));
  delay(3 * UNIT_TIME); // ヘッダーと最初の数字の間隔 (文字間隔)

  // 各数字のモールス符号を順番に送信
  for (int i = 0; i < dataCount; i++) {
    int numberToSend = dataBuffer[i];
    Serial.print("Sending number: ");
    Serial.println(numberToSend);
    sendMorseCode(MORSE_CODES[numberToSend], MORSE_CODE_LENGTHS[numberToSend]);
    if (i < dataCount - 1) { // 最後の数字でなければ文字間隔を空ける
      delay(3 * UNIT_TIME); // 数字間の間隔 (文字間隔)
    }
  }

  delay(3 * UNIT_TIME); // 最後の数字とフッターの間隔 (文字間隔)

  // フッター送信
  Serial.println("Sending Footer...");
  sendMorseCode(FOOTER, sizeof(FOOTER) / sizeof(int));
  delay(7 * UNIT_TIME); // 送信完了後の待機時間

  Serial.println("Transmission Complete. Waiting for new sensor input...");
}