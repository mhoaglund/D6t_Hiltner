#include <Favg.h>
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

Favg tracker(10,16,1,true);

const int FE_pin = 6;
const int POT_pin = A1;
const int SW_PIN = 5;
const int SENS_POT = A6;
const int DECAY_POT = A5;

uint8_t rbuf[N_READ];

const int deviation = 5;
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

  pinMode(FE_pin, INPUT);
    Serial.begin(9600);  // Serial baudrate = 115200bps
    Serial1.begin(9600); // Serial bus for the 2 arduinos. D13, D14
    Wire.begin();  // i2c master
    pixels.begin();
    pixels.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)
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
    //Serial.print("PTAT:");
    //Serial.println(itemp / 10.0, 1);

    // loop temperature pixels of each thrmopiles measurements
    for (i = 0, j = 2; i < N_PIXEL; i++, j += 2) {
        itemp = conv8us_s16_le(rbuf, j);
        registerPixel(itemp, i);
    }
    tracker.increment(current_readings);
    //interpret();
    setOutput();
    delay(50);
    //TODO instruct co-controllers
}

void registerPixel(int16_t px, int pos){
  current_readings[pos] = int(px);
}

void setOutput(){
  for (byte i = 0; i < N_PIXEL; i++) {
      int16_t latest_reading = current_readings[i];
      int16_t this_reading = tracker.frameAverage[i];
        int avgdiff = latest_reading - this_reading;
        Serial.print(" ");
        Serial.print(avgdiff, DEC);
        if(frontEndActive){
            if(avgdiff > 150) avgdiff = 150;
            if(avgdiff < 0) avgdiff = 0;
            pixels.setPixelColor(npx_readout_channels[i], pixels.Color(150 - (avgdiff * 2), 0, avgdiff));
        }
    }
    Serial.println(" ");
    if(frontEndActive){
      pixels.show();
    }
  }

// ex. <a:r:100> to tell addr a to set rate to 100
void instructCoController(char command, int addr, int payload){
    Serial1.print('<');
    Serial1.print(addr);
    Serial1.print(':');
    Serial1.print(command);
    Serial1.print(payload);
    Serial1.print('>');
}

void instructCoController(char command, int addr, String payload){
    Serial1.print('<');
    Serial1.print(addr);
    Serial1.print(':');
    Serial1.print(command);
    Serial1.print(payload);
    Serial1.print('>');
}

// vi: ft=arduino:fdm=marker:et:sw=4:tw=80
