#include <Wire.h>
#include "Adafruit_TCS34725.h"

// --- 送信側 (RGB LED) の設定 ---
const int R_PIN = 9;
const int G_PIN = 3;
const int B_PIN = 11;

// テストしたい色コードを指定(0:赤, 1:青, 2:緑, 3:シアン, 8:薄い青)
const int TEST_COLOR_CODE = 8; // 

const int LIGHT_ON_DURATION = 2000; // 2秒間点灯 (センサーが安定して読み取るため少し長めに)
const int LIGHT_OFF_DURATION = 1000; // 1秒間消灯

// --- 受信側 (TCS34725センサー) の設定 ---
Adafruit_TCS34725 tcs(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X); // 現在の設定

// 色コードの定義（送信側と合わせる）
const int COLOR_RED     = 0;
const int COLOR_BLUE    = 1;
const int COLOR_GREEN   = 2;
const int COLOR_CYAN    = 3;
const int COLOR_LOWBLUE = 8; // 区切り色 (現在は「薄い青」として使用)

// 消灯時のC値（輝度）を考慮した閾値
// このC値以下であれば「消灯時」または「暗すぎる」と判断し、どの色にも割り当てない
const uint16_t SHUTDOWN_C_THRESHOLD = 20;

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  while (!Serial); // シリアルモニタがオープンするまで待機 (Leonardo, Microなど)

  // 送信側 (RGB LED) のピン設定
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  // 受信側 (TCS34725センサー) の初期化
  if (!tcs.begin()) {
    Serial.println("Sensor not found! Check wiring.");
    while (1);
  }
  // TCS34725の内蔵LED制御（Adafruitライブラリは通常自動でオンにしますが、必要であれば）
  // tcs.setInterrupt(false); // TCS34725のLEDを制御するために割り込みを無効にする（不要な場合も）

  Serial.println("--- Color Test System Ready ---");
  Serial.println("Ensure the sensor is well-shielded from ambient light.");
  Serial.println("Adjust TCS34725 gain/integration time for C values in 500-5000 range for best results.");
  Serial.println("-------------------------------");
}

// RGB LEDで指定した色を点灯させる関数
void sendColor(int code) {
  int r, g, b;
  switch (code) {
    case 0: r = 255; g = 0;   b = 0;   break; // 赤
    case 1: r = 0;   g = 0;   b = 255; break; // 青
    case 2: r = 0;   g = 255; b = 0;   break; // 緑
    case 3: r = 0;   g = 255; b = 255; break; // シアン
    case 8: r = 0; g = 0;   b = 128; break; // 薄い青
    default: r = 0; g = 0; b = 0; break; // 未定義は消灯
  }
  analogWrite(R_PIN, r);
  analogWrite(G_PIN, g);
  analogWrite(B_PIN, b);
}

// 全てのLEDを消灯させる関数
void turnOffLights() {
  analogWrite(R_PIN, 0);
  analogWrite(G_PIN, 0);
  analogWrite(B_PIN, 0);
}

// RGB値とC値から色を判定する関数 (C値も引数として受け取る)
int detectColor(uint16_t r, uint16_t g, uint16_t b, uint16_t c) {
  // まず、C値（輝度）が非常に低い場合は、消灯時または暗すぎると判断し、UNKNOWNを返します。
  if (c < SHUTDOWN_C_THRESHOLD) {
    return -1; // 輝度が低すぎる場合はどの色でもないと判断
  }

  // 0 (赤) の閾値: 
  if (r >= 19 && r <= 29 &&
      g >= 5 && g <= 15 &&
      b >= 1 && b <= 11) return COLOR_RED;

  // 1 (青) の閾値: 
  if (r >= 9 && r <= 19 &&
      g >= 41 && g <= 51 &&
      b >= 127 && b <= 137) return COLOR_BLUE;

  // 2 (緑) の閾値: 
  if (r >= 17 && r <= 27 &&
      g >= 89 && g <= 99 &&
      b >= 28 && b <= 38) return COLOR_GREEN;

  // 3 (シアン) の閾値: 
  if (r >= 18 && r <= 28 &&
      g >= 128 && g <= 138 &&
      b >= 154 && b <= 164) return COLOR_CYAN;

  // 8 (薄い青) の閾値:
  if (r >= 8 && r <= 18 &&
      g >= 21 && g <= 31 &&
      b >= 64 && b <= 74) return COLOR_LOWBLUE;

  return -1; // どの色も検出されない場合
}

void loop() {
  // 1. 指定された色をLEDで点灯
  Serial.print("Emitting color code: ");
  Serial.println(TEST_COLOR_CODE);
  sendColor(TEST_COLOR_CODE);
  delay(LIGHT_ON_DURATION); // 安定するまで待機

  // 2. センサーからRGBとC値を読み取る
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // 3. 読み取った値で色を判定
  int detectedCode = detectColor(r, g, b, c);

  // 4. 結果をシリアルモニターに出力
  Serial.print("R: ");
  Serial.print(r);
  Serial.print(", G: ");
  Serial.print(g);
  Serial.print(", B: ");
  Serial.print(b);
  Serial.print(", C: ");
  Serial.print(c);

  Serial.print(" -> Detected Color Code: ");
  if (detectedCode == -1) {
    Serial.println("UNKNOWN (-1)");
  } else {
    Serial.println(detectedCode);
  }

  // 5. LEDを消灯
  Serial.println("Lights off.");
  turnOffLights();
  delay(LIGHT_OFF_DURATION); // 消灯状態を維持
}
