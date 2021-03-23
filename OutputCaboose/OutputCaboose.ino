//Installed on the last uno in the chain, it just listens and complies.

const long interval = 25;
const long alt_interval = 500;
long previousMillis = 0;
long previousMillisAlt = 0;
boolean newData = false;
boolean shouldRun = false;
String cmdcache = "";
byte VERSION = 1;

int GROUP1[] = {8, 9, 10, 11};

const byte numChars = 32;
char receivedChars[numChars];
const char adjustrate_flag = 'r';
const char adjuststoch_flag = 'x';
const char stop_flag = 's';
const char start_flag = 'b';
const char myname = 'a';

void setup()
{
  Serial.begin(9600);
  Serial.println("lets get started");
 
  for (int i = 0; i < 4; i = i + 1) {    
     pinMode(GROUP1[i], OUTPUT);
     digitalWrite(GROUP1[i], HIGH);
   }

  Serial.begin(9600);
  randomSeed(analogRead(0));
  Serial.println("starting...");
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
    Iterate();
  }
  if(currentMillis - previousMillisAlt > alt_interval) {
    previousMillisAlt = currentMillis; 
    UpdateActivationOrder();
  }
}

void UpdateActivationOrder(){
  //TODO repopulate run array
}

void Iterate(){
  //TODO move to next frame of action
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
        Serial.println(newrate, DEC);
      }
      else if(serialcommand[0] == adjuststoch_flag){
        serialcommand.remove(0,1);
        int stochasticity = serialcommand.toInt();
        Serial.println(stochasticity, DEC);
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
}

void Start(){
  Serial.println("Starting");
   if(!shouldRun){
    shouldRun = true;
   }
}
