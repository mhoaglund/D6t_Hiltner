//Installed on the last uno in the chain, it just listens and complies.
#include <SoftwareSerial.h>
int interval = 1250;
long previousMillis = 0;
boolean newData = false;
boolean shouldRun = true;
String cmdcache = "";
byte VERSION = 1;

int DEVICES[] = {5, 6, 7, 8, 9, 10, 11, 12};
const int DEVICE_COUNT = 8;

byte CHANGE_BITS[DEVICE_COUNT];
int TEMP_BITS[DEVICE_COUNT];

int density = 55;
//rates according to MKRZERO...
const int cc_loop_rate_base = 250;
const int cc_loop_rate_min = 10;

const byte numChars = 32;
char receivedChars[numChars];
const char adjustrate_flag = 'r';
const char adjustdensity_flag = 'x';
const char stop_flag = 's';
const char start_flag = 'b';
const char kick_flag = 'k';
const char myname = 'a';

SoftwareSerial mySerial(3, 2); // RX, TX

void setup()
{
  for (int i = 0; i < DEVICE_COUNT; i = i + 1) {  
     pinMode(DEVICES[i], OUTPUT);
     digitalWrite(DEVICES[i], HIGH);
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
      updateBits();
      Iterate();
    }
  }
}

void Iterate(){
  for(byte i = 0; i < DEVICE_COUNT; i++){
      byte current = CHANGE_BITS[i];
      if(current == 1){
          digitalWrite(DEVICES[i], LOW);
      } else {
          digitalWrite(DEVICES[i], HIGH);
      }
  }
}

//Create some new change bits.
void updateBits(){
    int modbase = map(density, 50,100,2,45); //klugy
    for(byte i = 0; i < DEVICE_COUNT; i++){
        TEMP_BITS[i] = random(0+modbase,density);
    }
    for(byte i = 0; i < DEVICE_COUNT; i++){
        if(TEMP_BITS[i] > 50){
            CHANGE_BITS[i] = 1;
        } else {
            CHANGE_BITS[i] = 0;
        }
    }
    if(density == 100){
        //max density, keep it fresh by turning a channel off
        byte target = random(0, (DEVICE_COUNT-1));
        CHANGE_BITS[target] = 0;
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
        constrain(newrate, cc_loop_rate_min, cc_loop_rate_base);
        Serial.println(newrate, DEC);
        int processed = map(newrate, cc_loop_rate_min, cc_loop_rate_base, 125, 1250);
        Serial.println(processed);
        if(newrate == 0 && shouldRun){
          Stop();
        }
        else if(!shouldRun && newrate > 0){
          Start();
        }
        interval = processed;
      }
      else if(serialcommand[0] == adjustdensity_flag){
        serialcommand.remove(0,1);
        int _stoch = serialcommand.toInt();
        if(_stoch > 150) _stoch = 150;
        if(_stoch < 2) _stoch = 2;
        density = map(_stoch, 2,150, 55,100);
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
