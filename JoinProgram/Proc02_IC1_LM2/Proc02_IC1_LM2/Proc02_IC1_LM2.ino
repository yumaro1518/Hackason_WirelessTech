#include <Arduino.h>

// --- 距離センサーからの数値変換部分 ---
int analogPin = A0;
int sensorValue = 0;
// 距離と対応する閾値。
// th[0]: 読み取り開始閾値（これ以上で開始）
// th[9]: 読み取り終了閾値（これ以下で終了）
// th[1]～th[8]：数字の範囲を定義
const int th[10] = {550, 525, 480, 415, 350, 290, 225, 201, 175, 170}; //例:15cm(高ADC),20cm,25cm,30cm,35cm,40cm,45cm,50cm,60cm,70cm(低ADC)
const int datamax = 20; // センサーから読み取って格納する数字の最大数
int dataBuffer[datamax]; // センサーから読み取った数値を一時的に格納するバッファ
int dataCount = 0;       // dataBufferに格納された数値の数

// センサー値を0から7の数字、または特殊な開始/終了コードへ変換
// 以前のthchange関数のロジックをより明確に修正
int thchange(int sensorValue) {
  if (sensorValue >= th[0]) { // th[0] (550) 以上は「読み取り開始」シグナル
    return -1; // 開始
  } else if (sensorValue <= th[9]) { // th[9] (170) 以下は「読み取り終了」シグナル
    return -2; // 終了
  }
  // それ以外の範囲で0～7の数字にマッピング
  else if (sensorValue <= th[1] && sensorValue > th[2]) { // 525以下, 480より大
    return 0;
  } else if (sensorValue <= th[2] && sensorValue > th[3]) { // 480以下, 415より大
    return 1;
  } else if (sensorValue <= th[3] && sensorValue > th[4]) { // 415以下, 350より大
    return 2;
  } else if (sensorValue <= th[4] && sensorValue > th[5]) { // 350以下, 290より大
    return 3;
  } else if (sensorValue <= th[5] && sensorValue > th[6]) { // 290以下, 225より大
    return 4;
  } else if (sensorValue <= th[6] && sensorValue > th[7]) { // 225以下, 201より大
    return 5;
  } else if (sensorValue <= th[7] && sensorValue > th[8]) { // 201以下, 175より大
    return 6;
  } else if (sensorValue <= th[8] && sensorValue > th[9]) { // 175以下, 170より大
    return 7;
  } else {
    // どの定義済み範囲にも当てはまらない場合
    // この999はエラー値として扱われ、データバッファには追加されません。
    return 999;
  }
}

// --- モールス符号送信部分 ---
// グローバル設定
const int UNIT_TIME = 100; // モールス符号の基本単位時間 (ミリ秒)
const int LASER_PIN = 9;   // レーザーモジュールが接続されているデジタルピン

// モールス符号の定義 (0-7) - ユーザー提供の3要素パターン
// ドットは1、ダッシュは3で表現
// -1: 文字内符号間隔 (ドットとダッシュの間) -> UNIT_TIME
// -2: 文字間隔 (文字と文字の間)         -> 3 * UNIT_TIME
const int MORSE_CODE_0[] = {3, 3, 3}; // ---
const int MORSE_CODE_1[] = {3, 3, 1}; // --.
const int MORSE_CODE_2[] = {3, 1, 3}; // -.-
const int MORSE_CODE_3[] = {3, 1, 1}; // -..
const int MORSE_CODE_4[] = {1, 3, 3}; // .--
const int MORSE_CODE_5[] = {1, 3, 1}; // .-.
const int MORSE_CODE_6[] = {1, 1, 3}; // ..-
const int MORSE_CODE_7[] = {1, 1, 1}; // ...

// ヘッダーとフッター (整数配列形式) - ユーザー提供の定義
const int HEADER[] = {1, 1, -1, 3, 3, 3, -1, 1, 1}; // "..---.." (SOSではない)
const int FOOTER[] = {1, -1, 3, -1, 1, -1, 3, -1, 1}; // ".-.-." (AR)

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
// SENSOR_READ_INTERVAL: センサーの読み取り間隔。これより短い期間でセンサー値を連続して読み取らないようにします。
// 元のコードのdelay(3700)を模倣するため、かなり長めに設定。
// リアルタイム性が求められる場合はもっと短く調整してください。
const unsigned long SENSOR_READ_INTERVAL = 1000; // 1秒ごとにセンサー値をチェック

void setup() {
  Serial.begin(9600);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // 初期状態はレーザーOFF
  Serial.println("Arduino Distance-to-Morse Sender Ready.");
  Serial.println("Waiting for sensor input (ADC > " + String(th[0]) + " to start, ADC < " + String(th[9]) + " to end)...");
}

void loop() {
  // センサー読み取りの頻度を制御
  if (millis() - lastSensorReadTime < SENSOR_READ_INTERVAL) {
    return; // まだ読み取り間隔に達していないので待機
  }
  lastSensorReadTime = millis(); // 読み取り時間を更新

  sensorValue = analogRead(analogPin); // センサー値を読み取り
  int detectedZone = thchange(sensorValue); // センサー値を数字または特殊コードに変換

  // 読み取り開始の検出 (th[0] <= sensorValue の場合、thchangeは-1を返す)
  if (detectedZone == -1 && !isReadingSensor) {
    Serial.println("--- 読み取り開始を検出 --- (センサー値: " + String(sensorValue) + ")");
    isReadingSensor = true;
    dataCount = 0; // 新しいデータの読み取り開始なのでバッファをリセット
    // 開始シグナル検出後、すぐにデータ読み取りを始めるための短い遅延
    // 元のコードにあったdelay(5100)は非常に長いため、ここで一度待機させる意味合い
    delay(1000); // 例えば1秒待機してからデータ収集開始 (調整してください)
  }
  // 読み取り中のデータ収集
  else if (isReadingSensor) {
    // 終了マーカーの検出 (th[9] >= sensorValue の場合、thchangeは-2を返す)
    if (detectedZone == -2) {
      Serial.println("--- 読み取り終了を検出 --- (センサー値: " + String(sensorValue) + ")");
      isReadingSensor = false; // 読み取り終了状態に
      if (dataCount > 0) { // データが収集されていればモールス符号として送信
        processAndSendMorseCode();
      } else {
        Serial.println("Warning: No valid data collected before end signal.");
      }
      dataCount = 0; // 送信後、またはデータがなくてもバッファをリセット
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
      Serial.println("不明なセンサー値の範囲 (センサー値: " + String(sensorValue) + "). 無視します。");
    }
  }
  // センサー読み取り待機中（isReadingSensorがfalseで、開始シグナルも検出されていない場合）
  else {
    Serial.print("待機中... 現在のセンサー値: ");
    Serial.println(sensorValue);
  }
}

// モールス符号のドットまたはダッシュをレーザーで送信する関数
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
    } else if (code == -1) { // 文字内符号間隔 (レーザーOFFのまま)
      digitalWrite(LASER_PIN, LOW);
      delay(UNIT_TIME);
      Serial.print(" "); // デバッグ用
    } else if (code == -2) { // 文字間隔 (レーザーOFFのまま)
      digitalWrite(LASER_PIN, LOW);
      delay(3 * UNIT_TIME);
      Serial.print("   "); // デバッグ用
    }

    // 符号と符号の間に短い間隔を空ける (次の符号が点または線の場合のみ)
    if (i < arrayLength - 1 && morseArray[i] > 0 && morseArray[i + 1] > 0) {
      delay(UNIT_TIME);
    }
  }
  Serial.println(); // 1つのモールス符号送信後、改行
}

// 収集したdataBufferの内容をモールス符号として送信する関数
void processAndSendMorseCode() {
  if (dataCount == 0) {
    Serial.println("No data to send from buffer.");
    return;
  }

  // dataBufferの内容をシリアル出力で確認
  Serial.print("Sending Morse code for sensor data: [");
  for (int i = 0; i < dataCount; i++) {
    Serial.print(dataBuffer[i]);
    if (i < dataCount - 1) {
      Serial.print(", ");
    }
  }
  Serial.println("]");

  // ヘッダー送信
  Serial.println("--- Sending Header ---");
  sendMorseCode(HEADER, sizeof(HEADER) / sizeof(int));
  delay(3 * UNIT_TIME); // ヘッダーと最初のデータの間隔

  // 各数字のモールス符号を順番に送信
  for (int i = 0; i < dataCount; i++) {
    int numberToSend = dataBuffer[i];
    Serial.print("Sending number: ");
    Serial.println(numberToSend);
    sendMorseCode(MORSE_CODES[numberToSend], MORSE_CODE_LENGTHS[numberToSend]);
    if (i < dataCount - 1) { // 最後の数字でなければ文字間隔を空ける
      delay(3 * UNIT_TIME); // 数字間の間隔
    }
  }

  delay(3 * UNIT_TIME); // 最後のデータとフッターの間隔

  // フッター送信
  Serial.println("--- Sending Footer ---");
  sendMorseCode(FOOTER, sizeof(FOOTER) / sizeof(int));
  delay(7 * UNIT_TIME); // 送信完了後の待機時間

  Serial.println("Transmission Complete. Waiting for new sensor input...");
}