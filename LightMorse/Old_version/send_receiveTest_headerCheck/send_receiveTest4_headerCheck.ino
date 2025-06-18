/*****************************************************************
  hdr_tx_rx_combo_auto.ino
*****************************************************************/
#include <Arduino.h>

const uint8_t LASER_PIN = 7;
const uint8_t PD_PIN    = A0;
const uint8_t LED_DBG   = LED_BUILTIN;

const uint16_t UNIT   = 60;
const uint16_t TOL    = 50;
const uint16_t THRESH = 180;
const uint16_t GAPCHR = 3*UNIT;
const uint16_t GAPFRM = 7*UNIT;
const uint32_t AUTO_INT = 5000;     // 5 s 自動送信間隔

const char *HEADER = "-.-.-";

/* ---- TX helpers ---- */
inline void laserOn(bool on){ digitalWrite(LASER_PIN,on); }
inline void dbgLED(bool on){ digitalWrite(LED_DBG,on); }

inline void dot(){  laserOn(true); dbgLED(true); delay(UNIT);
                    laserOn(false); dbgLED(false); delay(UNIT); }
inline void dash(){ laserOn(true); dbgLED(true); delay(3*UNIT);
                    laserOn(false); dbgLED(false); delay(UNIT); }
void sendHeader(){
  for(const char *p=HEADER; *p; ++p) (*p=='.')?dot():dash();
  delay(GAPFRM);
}

/* ---- RX helpers ---- */
inline bool isHi(){ return analogRead(PD_PIN) > THRESH; }
char classify(uint32_t d){
  if(abs((int)d-(int)UNIT)     <= TOL)   return '.';
  if(abs((int)d-(int)3*UNIT)   <= 3*TOL) return '-';
  return '?';
}
void hdrSniffer(){
  static char buf[8]; static uint8_t len=0;
  static bool lastHi=isHi(); static uint32_t tEdge=millis();
  bool hi=isHi(); uint32_t now=millis();
  if(hi!=lastHi){
    uint32_t dur=now-tEdge; tEdge=now;
    if(lastHi){
      char s=classify(dur); if(s!='?') { if(len<7)buf[len++]=s; buf[len]='\0'; }
    }else if(dur>=GAPCHR){ if(len<7)buf[len++]=' '; }
    if(strcmp(buf,HEADER)==0){ Serial.println(F("[HEADER DETECTED]")); len=0; }
    lastHi=hi;
  }
}

void setup(){
  pinMode(LASER_PIN,OUTPUT); laserOn(false);
  pinMode(LED_DBG, OUTPUT);  dbgLED(false);
  pinMode(PD_PIN,  INPUT);
  Serial.begin(9600);
  Serial.println(F("Auto-TX Header every 5 s.  Press h/H to repeat."));
}

void loop(){
  /* --- auto-TX every AUTO_INT ms --- */
  static uint32_t tAuto=millis();
  if(millis()-tAuto>=AUTO_INT){ tAuto+=AUTO_INT; sendHeader(); }

  /* --- manual TX via Serial --- */
  if(Serial.available()){
    char ch=Serial.read();
    if(ch=='h'||ch=='H') sendHeader();
  }

  /* --- header sniff --- */
  hdrSniffer();
}