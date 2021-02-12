#include <Adafruit_NeoPixel.h>

/*
 * MIT License
 * Copyright (c) 2019, 2018 - present OMRON Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* includes */
#include <Wire.h>

/* defines */
#define D6T_ADDR 0x0A  // for I2C 7bit address
#define D6T_CMD 0x4C  // for D6T-44L-06/06H, D6T-8L-09/09H, for D6T-1A-01/02

#define N_ROW 4
#define N_PIXEL (4 * 4)

#define N_READ ((N_PIXEL + 1) * 2 + 1)

#define NPXPIN        7
#define NUMPIXELS 64

Adafruit_NeoPixel pixels(NUMPIXELS, NPXPIN, NEO_GRB + NEO_KHZ800);
//Mux control pins
const int s0 = 0;
const int s1 = 1;
const int s2 = 2;
const int s3 = 3;

//Mux in "SIG" pin
const int SIG_pin = A0;
const int FE_pin = 6;
const int POT_pin = A1;

uint8_t rbuf[N_READ];

const int deviation = 5;
int fieldAvg = 0;
bool frontEndActive = true; //false for actual operation
int current_readings[16];
int npx_readout_channels[] = {2,3,4,5,10,11,12,13,18,19,20,21,26,27,28,29};
int current_switch_settings[16];



uint8_t calc_crc(uint8_t data) {
    int index;
    uint8_t temp;
    for (index = 0; index < 8; index++) {
        temp = data;
        data <<= 1;
        if (temp & 0x80) {data ^= 0x07;}
    }
    return data;
}

/** <!-- D6T_checkPEC {{{ 1--> D6T PEC(Packet Error Check) calculation.
 * calculate the data sequence,
 * from an I2C Read client address (8bit) to thermal data end.
 */
bool D6T_checkPEC(uint8_t buf[], int n) {
    int i;
    uint8_t crc = calc_crc((D6T_ADDR << 1) | 1);  // I2C Read address (8bit)
    for (i = 0; i < n; i++) {
        crc = calc_crc(buf[i] ^ crc);
    }
    bool ret = crc != buf[n];
    if (ret) {
        Serial.print("PEC check failed:");
        Serial.print(crc, HEX);
        Serial.print("(cal) vs ");
        Serial.print(buf[n], HEX);
        Serial.println("(get)");
    }
    return ret;
}

/** <!-- conv8us_s16_le {{{1 --> convert a 16bit data from the byte stream. Passing in a buffer (sensor packet) and an index.
 */
int16_t conv8us_s16_le(uint8_t* buf, int n) {
    int ret;
    ret = buf[n];
    ret += buf[n + 1] << 8;
    return (int16_t)ret;   // and convert negative.
}

/** <!-- setup {{{1 -->
 * 1. initialize a Serial port for output.
 * 2. initialize an I2C peripheral.
 */
void setup() {
   pinMode(s0, OUTPUT); 
  pinMode(s1, OUTPUT); 
  pinMode(s2, OUTPUT); 
  pinMode(s3, OUTPUT); 
  pinMode(FE_pin, INPUT);

  digitalWrite(s0, LOW);
  digitalWrite(s1, LOW);
  digitalWrite(s2, LOW);
  digitalWrite(s3, LOW);

    pinMode(SIG_pin, INPUT);
    Serial.begin(115200);  // Serial baudrate = 115200bps
    Serial1.begin(115200); // Serial bus for the 2 arduinos. D13, D14
    Wire.begin();  // i2c master
    pixels.begin();
}


/** <!-- loop - Thermal sensor {{{1 -->
 * 1. read sensor.
 * 2. output results, format is: [degC]
 */
void loop() {
    int reading = digitalRead(FE_pin);
//    if(reading == HIGH){
//      frontEndActive = true;
//    } else {
//      frontEndActive = false;
//    }
    if(frontEndActive){
      //readSwitches();
      //int sens = analogRead(POT_pin);
      //TODO set sensitivity from pot
    }
  
    int i, j;

    memset(rbuf, 0, N_READ);
    // Wire buffers are enough to read D6T-16L data (33bytes) with
    // MKR-WiFi1010 and Feather ESP32,
    // these have 256 and 128 buffers in their libraries.
    Wire.beginTransmission(D6T_ADDR);  // I2C client address
    Wire.write(D6T_CMD);               // D6T register
    Wire.endTransmission();            // I2C repeated start for read
    delay(1);
    Wire.requestFrom(D6T_ADDR, N_READ);
    i = 0;
    while (Wire.available()) {
        rbuf[i++] = Wire.read();
    }

    if (D6T_checkPEC(rbuf, N_READ - 1)) {
        return;
    }

    // 1st data is PTAT measurement (: Proportional To Absolute Temperature)
    int16_t itemp = conv8us_s16_le(rbuf, 0);
    Serial.print("PTAT:");
    Serial.println(itemp / 10.0, 1);

    // loop temperature pixels of each thrmopiles measurements
    for (i = 0, j = 2; i < N_PIXEL; i++, j += 2) {
        itemp = conv8us_s16_le(rbuf, j);
        registerPixel(itemp, i);
        Serial.print(itemp / 10.0, 1);  // print PTAT & Temperature
        //TODO: register pixel
        if ((i % N_ROW) == N_ROW - 1) {
            Serial.println(" [degC]");  // wrap text at ROW end.
        } else {
            Serial.print(",");   // print delimiter
        }
    }
    updateAverage();
    setPixelDisplay();
    delay(25);
}

//Intake method: a pixel has just arrived. 
void registerPixel(int16_t px, int pos){
  current_readings[pos] = px;
//  Set Neopixel for this one
//  Whatever else
}

//Called at the end of a frame. Recompute average of field.
void updateAverage(){
  fieldAvg = _arrAvg(current_readings, 16);
  Serial.print("AVG:   ");
  Serial.println(fieldAvg, DEC);
}

//Set all display pixels for the sensor readout.
void setPixelDisplay(){
  for (byte i = 0; i < N_PIXEL; i++) {
      int16_t this_reading = current_readings[i];
        //setPixel(this_reading, i);
        int avgdiff = this_reading - fieldAvg;
        if(avgdiff > deviation && this_reading > fieldAvg){
          pixels.setPixelColor(npx_readout_channels[i], pixels.Color(0, 75, 75));
        } else {
          pixels.setPixelColor(npx_readout_channels[i], pixels.Color(150, 0, 0));     
        }
    }
    pixels.show();
  }

int _arrAvg(int _avgarr[], int _arrsize){
  int total = 0;
  for (int ind = 0; ind < _arrsize; ind++) {
    total += _avgarr[ind];
  }
  return total/_arrsize;
}

//Read the 16 pixel settings
void readSwitches(){
  for(int i = 0; i < 16; i ++){
//    Serial.print("Value at channel ");
//    Serial.print(i);
//    Serial.print("is : ");
//    Serial.println(readMux(i));
    current_switch_settings[i] = readMux(i);
  }
}

int readMux(int channel){
  int controlPin[] = {s0, s1, s2, s3};

  int muxChannel[16][4]={
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };

  //loop through the 4 sig
  for(int i = 0; i < 4; i ++){
    digitalWrite(controlPin[i], muxChannel[channel][i]);
  }

  //read the value at the SIG pin
  int val = analogRead(SIG_pin);

  //return the value
  return val;
}
// vi: ft=arduino:fdm=marker:et:sw=4:tw=80