int x[8] = {
   3,1,5,0,7,6,2,4
};


void setup() {
  pinMode(3,OUTPUT);
  pinMode(9,OUTPUT);
  pinMode(11,OUTPUT);
}



void loop() {
  delay(500);
//ヘッダー部分
  analogWrite(3,128);
  analogWrite(9,128);
  analogWrite(11,128);
  delay(5000);
 
  digitalWrite(3,LOW);
  digitalWrite(9,LOW);
  digitalWrite(11,LOW);
  delay(500);


}