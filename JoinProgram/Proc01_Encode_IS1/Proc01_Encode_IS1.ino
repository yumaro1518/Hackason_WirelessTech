#include <SPI.h>
#include "Arduino_LED_Matrix.h"
ArduinoLEDMatrix matrix;

// ピン定義。
#define PIN_SPI_MOSI 11
#define PIN_SPI_MISO 12
#define PIN_SPI_SCK 13
#define PIN_SPI_SS 10
#define PIN_BUSY 9

const char glyph[8][8] PROGMEM = {
  { 'A','B','C','D','E','F','G','H' },
  { 'I','J','K','L','M','N','O','P' },
  { 'Q','R','S','T','U','V','W','X' },
  { 'Y','Z','a','b','c','d','e','f' },
  { 'g','h','i','j','k','l','m','n' },
  { 'o','p','q','r','s','t','u','v' },
  { 'w','x','y','z','!','?','.',',' },
  { ':',';','-','_','@','$','%',' ' }   
};


// findPos関数指定文字 c の (row,col) を探す。見つかったら true を返し、参照引数 r,c に代入。
bool findPos(char c, int &r, int &cidx)
{
  for (r = 0; r < 8; r++) {
    for (cidx = 0; cidx < 8; cidx++) {
      if (pgm_read_byte(&glyph[r][cidx]) == c) 
      return true;       //復号テーブル内に該当の文字がある場合はtrueを返す
    }
  }
  return false;          // テーブルに存在しない場合にはfalseを返す
}

void setup()
{
  delay(1000);
  pinMode(PIN_SPI_MOSI, OUTPUT);
  pinMode(PIN_SPI_MISO, INPUT);
  pinMode(PIN_SPI_SCK, OUTPUT);
  pinMode(PIN_SPI_SS, OUTPUT);
  pinMode(PIN_BUSY, INPUT);
  SPI.begin();

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE3));
  // SPI.setDataMode(SPI_MODE3);
  // SPI.setBitOrder(MSBFIRST);
  Serial.begin(9600);
  digitalWrite(PIN_SPI_SS, HIGH);
 
  L6470_resetdevice(); //L6470リセット
  L6470_setup();  //L6470を設定
  L6470_goto(0);
  L6470_busydelay(1000);
  

  
  // L6470_move(1,1600);//指定方向に指定数ステップする 
  // L6470_busydelay(5000); //busyフラグがHIGHになってから、指定ミリ秒待つ。
  // L6470_run(0,10000);//指定方向に連続回転
  // delay(6000);
  // L6470_softstop();//回転停止、保持トルクあり
  // L6470_busydelay(5000);
  // L6470_goto(0x6789);//指定座標に最短でいける回転方向で移動
  // L6470_busydelay(5000);
  // L6470_run(0,0x4567);
  // delay(6000);
  // L6470_hardhiz();//回転急停止、保持トルクなし
  while(!Serial){}

  Serial.println(F("使用可能は文字列はA-Z,a-z,!?,.:;_@$% です"));
  Serial.println(F("入力して改行するその文字列に対応した行と列のペアを出力します"));

}

// インデックス → ステップ変換
int indexToStep(int label) {


    int mm[] = {200, 250, 300, 350, 400, 450, 500, 600, 700};
    if (label >= 0 && label <= 8) {
        return (int)(mm[label] * 200.0 * 8 / 60.0);  // = mm *1/8すってぷ* 3.333
    }

    // デフォルトは原点（0）
    return 0;
}

// 移動処理
void moveTo(int label) {
    int pos = indexToStep(label);
    L6470_goto(pos);
    L6470_busydelay(1000);  // 必要に応じて
}

void loop(){
    static String buf;

  if(!Serial.available())
  return;

  char ch = Serial.read();
  if (ch == '\n' || ch == '\r') {
    if (buf.length() == 0) 
    return;           
   bool flag = true;

    L6470_goto(150*8);
    L6470_busydelay(1000);

    for (uint16_t i = 0; i < buf.length(); i++) {
      int row, col;
      if (findPos(buf[i], row, col)) {
        Serial.print(row);
        moveTo(row);
        Serial.print(',');
        Serial.print(col);
        moveTo(col);
        if (i < buf.length() - 1) Serial.print(',');
      } else {
        Serial.println();
        Serial.print(F("対応していない文字「"));
        Serial.print(buf[i]);
        Serial.println(F("」が含まれています。もう一度入力してください。"));
        flag = false;
        break;
      }
    }
      L6470_goto(2500*8);
      L6470_busydelay(1000);
    if (flag == true) Serial.println();             // 正常終了で改行
    buf = "";
  }else {
    buf += ch;/* 改行以外の文字はバッファに追加 */
  }
}


void L6470_setup(){
L6470_setparam_acc(0x40); //[R, WS] 加速度default 0x08A (12bit) (14.55*val+14.55[step/s^2])
L6470_setparam_dec(0x40); //[R, WS] 減速度default 0x08A (12bit) (14.55*val+14.55[step/s^2])
L6470_setparam_maxspeed(0x40); //[R, WR]最大速度default 0x041 (10bit) (15.25*val+15.25[step/s])
L6470_setparam_minspeed(0x01); //[R, WS]最小速度default 0x000 (1+12bit) (0.238*val[step/s])
L6470_setparam_fsspd(0x3ff); //[R, WR]μステップからフルステップへの切替点速度default 0x027 (10bit) (15.25*val+7.63[step/s])
L6470_setparam_kvalhold(0x50); //[R, WR]停止時励磁電圧default 0x29 (8bit) (Vs[V]*val/256)
L6470_setparam_kvalrun(0x50); //[R, WR]定速回転時励磁電圧default 0x29 (8bit) (Vs[V]*val/256)
L6470_setparam_kvalacc(0x50); //[R, WR]加速時励磁電圧default 0x29 (8bit) (Vs[V]*val/256)
L6470_setparam_kvaldec(0x50); //[R, WR]減速時励磁電圧default 0x29 (8bit) (Vs[V]*val/256)

L6470_setparam_stepmood(0x03); //ステップモードdefault 0x07 (1+3+1+3bit)
}

void fulash(){
  Serial.print("0x");
  Serial.print( L6470_getparam_abspos(),HEX);
  Serial.print("  ");
  Serial.print("0x");
  Serial.println( L6470_getparam_speed(),HEX);
}