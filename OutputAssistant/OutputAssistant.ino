/*
  Software serial multple serial test

 Receives from the hardware serial, sends to software serial.
 Receives from software serial, sends to hardware serial.

 The circuit:
 * RX is digital pin 2 (connect to TX of other device)
 * TX is digital pin 3 (connect to RX of other device)

 Note:
 Not all pins on the Mega and Mega 2560 support change interrupts,
 so only the following can be used for RX:
 10, 11, 12, 13, 50, 51, 52, 53, 62, 63, 64, 65, 66, 67, 68, 69

 Not all pins on the Leonardo support change interrupts,
 so only the following can be used for RX:
 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).

 created back in the mists of time
 modified 25 May 2012
 by Tom Igoe
 based on Mikal Hart's example

 This example code is in the public domain.

 */
#include <SoftwareSerial.h>

const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;
String cmdcache = "";

const char myaddr = 'a';
const char waitflag = 'w'; //stop output activity
const char rateflag = 'r'; //change rate
const char patternflag = 'p'; //spin up new pattern
const char stochflag = 's'; //how much randomness to add to routine

SoftwareSerial mySerial(2, 3); // RX, TX

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Goodnight moon two!!");

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);
}

void loop() // run over and over
{
  recvWithStartEndMarkers();
  if (newData == true) {
      newData = false;
      String temp(receivedChars);
      applySerialCommand(temp);
  }
}

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
 
    while (mySerial.available() > 0 && newData == false) {
        rc = mySerial.read();
        Serial.write(rc); //forward regardless

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

void applySerialCommand(String serialcommand){
  if(receivedChars[0] != myaddr){
    return;
  }
  if(_mode == rfidflag){
        //cache a command that came in during button interaction
        cmdcache = serialcommand;
        return;
      }
      else if(receivedChars[2] == rgbflag){
        _mode = rgbflag;
        _modecache = rgbflag;
        _shouldFlash = true;
        char* rgbvals = strtok(receivedChars, ".");
        byte i = 0;
        //packet looks like this: <c.125.125.125>
        while (rgbvals != 0){
            int color = atoi(rgbvals);
            if(color > 0){
                cueColor[i] = color;
                i++;                
            }
          rgbvals = strtok(0, ".");
        }
      }
      else if(receivedChars[2] == waitflag){
        _mode = waitflag;
      }
}

