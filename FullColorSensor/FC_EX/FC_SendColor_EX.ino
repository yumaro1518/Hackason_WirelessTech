#include <Arduino.h> // 

// RGB LEDが接続されているピンの定義
const int R_PIN = 9;
const int G_PIN = 3;
const int B_PIN = 11;

// 色コードの定義 (受信側と合わせる)
const int COLOR_RED        = 0;
const int COLOR_LOWBLUE       = 1;
const int COLOR_LOWGREEN      = 2;
const int COLOR_CYAN       = 3;
const int COLOR_BLUE    = 4; //ヘッダー
const int COLOR_GREEN   = 5;//フッター
const int COLOR_LOWCYAN    = 6;// 区切り色


// 新しい色コードの定義 (受信側と合わせる)

// ヘッダー、フッター、セパレーターの定義 (単色に変更)
const int HEADER_CODE = COLOR_BLUE;  // ヘッダーは薄い青
const int FOOTER_CODE = COLOR_GREEN;   // フッターは薄い緑
const int SEPARATOR_CODE = COLOR_LOWCYAN;       // （区切り）

// データに変換されるカラーペア (変更なし)
const int colorPairs[8][2] = {
  {0, 1}, {0, 2}, {0, 3}, // 0: 赤+青, 1: 赤+緑, 2: 赤+シアン
  {1, 0}, {1, 2}, {1, 3}, // 3: 青+赤, 4: 青+緑, 5: 青+シアン
  {2, 0}, {2, 1}          // 6: 緑+赤, 7: 緑+青
};

// 送信するデータ配列の定義
int data[] = {5, 7, 4, 3, 7, 1, 0, 2};
//薄い青，赤，緑，薄いシアン，青．緑，薄いシアン，薄い緑
// 発光時間を調整 (ミリ秒)
const int LIGHT_ON_DURATION = 1500; // 各色1.5秒点灯
const int INTERVAL_DURATION = 500;  // 色間の消灯時間

void setup() {
  // ピンを出力モードに設定
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  Serial.begin(9600); // デバッグ用にシリアル通信を開始
  Serial.println("Transmitter Ready.");
}

// 全てのLEDを消灯する関数
void turnOffLights() {
  analogWrite(R_PIN, 0);
  analogWrite(G_PIN, 0);
  analogWrite(B_PIN, 0);
}

// 指定された色コードに基づいてLEDを発光させる関数
void sendColor(int code) {
  int r, g, b;
  // 色コードに対応するRGB値を設定
  switch (code) {
    case COLOR_RED:        r = 255; g = 0;   b = 0;   break; // 赤
    case COLOR_BLUE:       r = 0;   g = 0;   b = 255; break; // 青
    case COLOR_GREEN:      r = 0;   g = 255; b = 0;   break; // 緑
    case COLOR_CYAN:       r = 0;   g = 255; b = 255; break; // シアン
    case COLOR_LOWBLUE:    r = 0;   g = 0;   b = 128; break; // 薄い青
    case COLOR_LOWGREEN: r = 0;   g = 128; b = 0;   break; // 薄い緑
    case COLOR_LOWCYAN:  r = 0;   g = 128; b = 128; break; // 薄いシアン 
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

void loop() {
  Serial.println("--- Transmission Start ---");

  // ヘッダー送信 (単色)
  Serial.print("Sending Header: ");
  Serial.println(HEADER_CODE);
  sendColor(HEADER_CODE);

  // データ送信ループ
  for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    Serial.print("Sending Data Pattern: ");
    Serial.println(data[i]);
    // データは2色ペアで送信
    int a = colorPairs[data[i]][0];
    int b = colorPairs[data[i]][1];

    Serial.print("  Color 1: ");
    Serial.println(a);
    sendColor(a); // 1色目

    Serial.print("  Color 2: ");
    Serial.println(b);
    sendColor(b); // 2色目

    // データペアの区切り色送信
    Serial.print("Sending Separator: ");
    Serial.println(SEPARATOR_CODE);
    sendColor(SEPARATOR_CODE);
  }

  // フッター送信 (単色)
  Serial.print("Sending Footer: ");
  Serial.println(FOOTER_CODE);
  sendColor(FOOTER_CODE);

  Serial.println("--- Transmission Complete ---");
  delay(5000); // 全送信終了後、しばらく待機
  exit(0);
}