void setup() {
  pinMode(7, OUTPUT);  digitalWrite(7, HIGH);  // レーザー常時点灯
  pinMode(A0, INPUT);
  Serial.begin(115200);
}
void loop() {
  Serial.println(analogRead(A0));  // 0-1023 を 50 ms ごとに表示
  delay(50);
}