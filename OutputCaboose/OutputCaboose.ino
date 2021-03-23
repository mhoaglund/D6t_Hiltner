//Installed on the last uno in the chain, it just listens and complies.
//#include <SoftwareSerial.h>
int interval = 125;
int alt_interval = 500;
long previousMillis = 0;
long previousMillisAlt = 0;
boolean newData = false;
boolean shouldRun = false;
String cmdcache = "";
byte VERSION = 1;

int DEVICES[] = {8, 9, 10, 11};
boolean DIR = true;
int DEVICE_COUNT = 4;
const byte MAX_TICKS = 25; //number of ticks in a measure of time
byte TICKS = 1;
const byte STATECHANGES[] = {1,7,14,19}; //device changeover points

//At times, pump timing will be varied by small random amounts. Pauses will also be inserted.
byte STATECHANGE_AUX_STARTS[] = {0,0,0,0};
byte STATECHANGE_AUX_PAUSES[] = {0,0,0,0};
byte STATECHANGE_AUX_RAND_PAUSES[] = {0,0,0,0};

byte stochasticity = 4;

const byte numChars = 32;
char receivedChars[numChars];
const char adjustrate_flag = 'r';
const char adjuststoch_flag = 'x';
const char stop_flag = 's';
const char start_flag = 'b';
const char myname = 'b';

//SoftwareSerial mySerial(2, 3); // RX, TX

void setup()
{
  Serial.begin(9600);
  Serial.println("lets get started");
 
  for (int i = 0; i < DEVICE_COUNT; i = i + 1) {    
     pinMode(DEVICES[i], OUTPUT);
     digitalWrite(DEVICES[i], HIGH);
   }

  Serial.begin(9600);
  randomSeed(analogRead(0));
  //mySerial.begin(9600);
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
    //TODO stochastic redecorate? or will that always be on another loop?
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
  Serial.println("Decorating...");
  for(byte i = 0; i < DEVICE_COUNT; i++){
     int base = STATECHANGES[i];
     byte _newpause = random(base, base+stochasticity);
     byte _randpause = random(1, MAX_TICKS);
     byte _newstart = random(base-stochasticity, base);
     if(_newstart < 1) _newstart = 1;
     if(_newpause > MAX_TICKS) _newpause = MAX_TICKS;
     STATECHANGE_AUX_STARTS[i] = _newstart;
     STATECHANGE_AUX_PAUSES[i] = _newpause;
     STATECHANGE_AUX_RAND_PAUSES[i] = _randpause;
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
      if(serialcommand[0] != myname){
        return;
      } else {
        serialcommand.remove(0,1);
      }
      if(serialcommand[0] == adjustrate_flag){
        //TODO adjust rate of cycling
        serialcommand.remove(0,1);
        int newrate = serialcommand.toInt();
        if(newrate < 50) newrate = 50; //avoid burning out the relays
        Serial.println(newrate, DEC);
        if(newrate == 0 && shouldRun){
          Stop();
        }
        else if(!shouldRun && newrate > 0){
          Start();
        }
        interval = newrate;
      }
      else if(serialcommand[0] == adjuststoch_flag){
        serialcommand.remove(0,1);
        int stoch = serialcommand.toInt();
        Serial.println(stoch, DEC);
        alt_interval = stoch;
      }
      else if(serialcommand[0] == stop_flag){
        Stop();
      }
      else if(serialcommand[0] == start_flag){
        Start();
      }
}

void Stop(){
  Serial.println("Stopping");
   if(shouldRun){
    shouldRun = false;
   }
   for (int i = 0; i < DEVICE_COUNT; i = i + 1) {    
     digitalWrite(DEVICES[i], HIGH);
   }
}

void Start(){
  Serial.println("Starting");
   if(!shouldRun){
    shouldRun = true;
   }
}
