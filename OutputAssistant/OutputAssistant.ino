//Installed on the middle uno in the chain, it listens, complies, and forwards.
#include <SoftwareSerial.h>
int interval = 125;
int alt_interval = 500;
long previousMillis = 0;
long previousMillisAlt = 0;
boolean newData = false;
boolean shouldRun = false;
String cmdcache = "";
byte VERSION = 1;

int DEVICES[] = {6, 7, 8, 9, 10, 11, 12, 13};
const int SPACING = 25;
boolean DIR = true;
const int DEVICE_COUNT = 8;
int MAX_TICKS = 100; //number of ticks in a measure of time
byte TICKS = 1;
int STATECHANGES[DEVICE_COUNT]; //device changeover points

//At times, pump timing will be varied by small random amounts. Pauses will also be inserted.
int STATECHANGE_AUX_STARTS[DEVICE_COUNT];
int STATECHANGE_AUX_PAUSES[DEVICE_COUNT];

//Start-to-pause period variance (affects twice randomly)
byte stochasticity = 3;

const byte numChars = 32;
char receivedChars[numChars];
const char adjustrate_flag = 'r';
const char adjustdensity_flag = 'x';
const char stop_flag = 's';
const char start_flag = 'b';
const char myname = 'a';

SoftwareSerial mySerial(3, 2); // RX, TX

void setup()
{
  MAX_TICKS = (DEVICE_COUNT + 1) * SPACING;
  for (int i = 0; i < DEVICE_COUNT; i = i + 1) {  
     pinMode(DEVICES[i], OUTPUT);
     digitalWrite(DEVICES[i], HIGH);
     STATECHANGES[i] = 1 + (i * SPACING);
   }
   
  Serial.begin(9600);
  mySerial.begin(9600);
  randomSeed(analogRead(0));
}

void loop()
{
  unsigned long currentMillis = millis();
  recvWithStartEndMarkers();
  if (newData == true) {
      newData = false;
      String temp(receivedChars);
      applySerialCommand(temp);
  }
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis; 
    if(shouldRun){
      Iterate();
    }
  }
  if(currentMillis - previousMillisAlt > alt_interval) {
    previousMillisAlt = currentMillis; 
    if(shouldRun){
      DecorateCycle();  
    }
  }
}

void Iterate(){
  if(TICKS >= MAX_TICKS){
    TICKS = 1;
  }
  for(byte i = 0; i < DEVICE_COUNT; i++){
    if(DIR){
      if(TICKS < STATECHANGE_AUX_STARTS[i] | TICKS == STATECHANGE_AUX_PAUSES[i]){
        digitalWrite(DEVICES[i], HIGH);
      }
      else digitalWrite(DEVICES[i], LOW);
    }
    else{
      if(TICKS > STATECHANGES[i]){
        digitalWrite(DEVICES[i], LOW);
      }
      else digitalWrite(DEVICES[i], HIGH);
    }
  }
  TICKS++;
}

void DecorateCycle(){
  for(byte i = 0; i < DEVICE_COUNT; i++){
     int base = STATECHANGES[i];
     int modifier = stochasticity / 4;
     byte _newpause = random(base, base+stochasticity) + modifier;
     byte _newstart = random(base-stochasticity, base) - modifier;
     if(_newstart < 1) _newstart = 1;
     if(_newpause > MAX_TICKS) _newpause = MAX_TICKS;
     STATECHANGE_AUX_STARTS[i] = _newstart;
     STATECHANGE_AUX_PAUSES[i] = _newpause;
  }
}

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
 
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0';
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

//Serial command ex. <ar50>
void applySerialCommand(String serialcommand){
      Serial.println(serialcommand);
      mySerial.print('<' + serialcommand + '>'); //forward it
      if(serialcommand[0] != myname){
        return;
      } else {
        serialcommand.remove(0,1);
      }
      if(serialcommand[0] == adjustrate_flag){
        serialcommand.remove(0,1);
        int newrate = serialcommand.toInt();
        if(newrate < 20) newrate = 50; //avoid burning out the relays
        if(newrate == 0 && shouldRun){
          Stop();
        }
        else if(!shouldRun && newrate > 0){
          Start();
        }
        interval = newrate;
        alt_interval = newrate * 20;
      }
      else if(serialcommand[0] == adjustdensity_flag){
        serialcommand.remove(0,1);
        int _stoch = serialcommand.toInt();
        if(_stoch > 60) _stoch = 60;
        if(_stoch < 2) _stoch = 2;
        stochasticity = _stoch;
      }
      else if(serialcommand[0] == stop_flag){
        Stop();
      }
      else if(serialcommand[0] == start_flag){
        Start();
      }
}

void Stop(){
   if(shouldRun){
    shouldRun = false;
   }
   for (int i = 0; i < DEVICE_COUNT; i = i + 1) {    
     digitalWrite(DEVICES[i], HIGH);
   }
}

void Start(){
   if(!shouldRun){
    shouldRun = true;
   }
}
