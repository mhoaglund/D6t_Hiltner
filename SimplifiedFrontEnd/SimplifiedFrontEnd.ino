#include <2JCIE-EV01.h>

#include <Favg.h>
#include <Adafruit_NeoPixel.h>

// TODO: Simplified Version
// What if we use the Sens adjustment to define a "cutoff"?
// Use the PTAT from the D6T and add the cutoff value.
// If a given thermopile is turning in a higher val, it's a presence.

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

const int AVG_RATE = 4;
Favg tracker(10,16,1,AVG_RATE,true); //10 frames deep, 16 values per frame, max ROC of 1, computed every 4 frames

const int FE_pin = 4;
const int POT_pin = A1;
const int SENS_POT = A6;
const int RATE_POT = A5;

const char adjustrate_flag = 'r';
const char adjustdensity_flag = 'x';
const char stop_flag = 's';
const char start_flag = 'b';
const char myname = 'c';

int interval = 50;
int alt_interval = 250;
long previousMillis = 0;
long previousMillisAlt = 0;
long previousFeMillis = 0;

uint8_t rbuf[N_READ];

int sensitivity = 25;
int delaytime = 50;
const int deviation = 5;
bool frontEndActive = true; //false for actual operation
int current_readings[16];
int npx_readout_channels[] = {2,3,4,5,10,11,12,13,18,19,20,21,26,27,28,29};
int npx_sens_channels[] = {0,8,16,24,32,40,48}; //left column
int npx_rate_channels[] = {7,15,23,31,39,47,55}; //right column
int npx_summary_channels[] = {56,57,58,59,60,61,62,63}; //bottom row
int current_switch_settings[16];

const int max_sum = 1000;

//Base values for cocontroller command inputs
int current_delta_sum = 0;
const int cc_loop_rate_base = 250;
const int cc_loop_rate_min = 10;
const int cc_density_base = 5;
const int cc_density_max = 150;
int cc_loop_rate = 125;
int cc_density = 5;

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

int16_t conv8us_s16_le(uint8_t* buf, int n) {
    int ret;
    ret = buf[n];
    ret += buf[n + 1] << 8;
    return (int16_t)ret;   // and convert negative.
}

void setup() {

  pinMode(FE_pin, INPUT);
  pinMode(SENS_POT, INPUT);
  pinMode(RATE_POT, INPUT);
    Serial.begin(9600);  // Serial baudrate = 115200bps
    Serial1.begin(9600); // Serial bus for the 2 arduinos. D13, D14
    Wire.begin();  // i2c master
    pixels.begin();
    pixels.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)
}

void loop() {
    int fe_state = digitalRead(FE_pin);
    if(fe_state == HIGH){
      frontEndActive = true;
    } else {
      frontEndActive = false;
      pixels.clear();
      pixels.show();
    }

    int sens_state = analogRead(SENS_POT); //0-1000 for each
    sensitivity = map(sens_state, 0, 1050, 65, 5); //we have presence if reading = avg + sensitivty
    int rate_state = analogRead(RATE_POT); //Adjust the speed of the whole sensing loop.
    delaytime = map(rate_state, 0, 1050, 60, 1000);
    alt_interval = delaytime * 8;

    unsigned long currentMillis = millis();
    
    if(currentMillis - previousMillis > delaytime) {
      previousMillis = currentMillis; 
      readFromSensor();
      setOutput();
      computeLoopRate(current_delta_sum);
    }

    if(currentMillis - previousFeMillis > interval) {
      previousFeMillis = currentMillis; 
      if(frontEndActive){
        updateFrontEnd();
      }    
    }
    
    if(currentMillis - previousMillisAlt > alt_interval) {
      previousMillisAlt = currentMillis; 
//      Serial.print("S-R: ");
//      Serial.print(sensitivity, DEC);
//      Serial.print(" - ");
//      Serial.println(delaytime, DEC);
//      Serial.print("Delta Sum is: ");
//      Serial.println(current_delta_sum, DEC);
//      Serial.println("Updating CCs...");
//      Serial.print("cc loop rate is: ");
//      Serial.println(cc_loop_rate);
//      Serial.print("cc density is: ");
//      Serial.println(cc_density);
      instructCoController('a', adjustrate_flag, cc_loop_rate);
      instructCoController('b', adjustrate_flag, cc_loop_rate);
      instructCoController('a', adjustdensity_flag, cc_density); //What do we want to do with this density flag? Might need to look end to end.
      instructCoController('b', adjustdensity_flag, cc_density);
    }
}

void computeLoopRate(int _latest){
    int inc = 0;
    if(_latest = 0){
        //No activity!
        inc = -1;
    } else {
        inc = _latest; //1-16
    }
    cc_loop_rate += inc;
    constrain(cc_loop_rate, cc_loop_rate_min, cc_loop_rate_base);
    cc_density = map(cc_loop_rate, cc_loop_rate_min, (cc_loop_rate_base/2), cc_density_max, cc_density_base);
    if(cc_density < cc_density_base) cc_density = cc_density_base;
}

void readFromSensor(){
    int i, j;
    memset(rbuf, 0, N_READ);

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
    int16_t itemp = conv8us_s16_le(rbuf, 0);
    // loop temperature pixels of each thermopiles measurements
    for (i = 0, j = 2; i < N_PIXEL; i++, j += 2) {
        itemp = conv8us_s16_le(rbuf, j);
        current_readings[i] = int(itemp);
    }
    tracker.increment(current_readings);
}

//Set the sensor view and calculate values for cocontroller messages
void setOutput(){
  int delta_sum = 0;
  for (byte i = 0; i < N_PIXEL; i++) {
      int16_t latest_reading = current_readings[i];
      int16_t this_reading = tracker.frameAverage[i];
        int avgdiff = latest_reading - this_reading; //Difference between this thermopile and the running avg
        if(avgdiff > sensitivity){
            //Presence in this thermopile, for now.
            delta_sum += 1;
        }
    }
    //delta sum is between 1 and 16.
    current_delta_sum = delta_sum;
  }

 void updateFrontEnd(){
  for (byte i = 0; i < N_PIXEL; i++) {
      int16_t latest_reading = current_readings[i];
      int16_t this_reading = tracker.frameAverage[i];
        int avgdiff = latest_reading - this_reading; //Difference between this thermopile and the running avg
        if(avgdiff > 150) avgdiff = 150;
        if(avgdiff < 0) avgdiff = 0;
        pixels.setPixelColor(npx_readout_channels[i], pixels.Color(150 - (avgdiff * 2), 0, avgdiff));
    }
    byte sens_level = map(sensitivity, 5, 65, 7, 1);
    for(byte j = 0; j < 7; j++){
      if(j < sens_level){
        pixels.setPixelColor(npx_sens_channels[j], pixels.Color(150,75,0));
      } else pixels.setPixelColor(npx_sens_channels[j], pixels.Color(15,150,150));
    }
    byte rate_level = map(delaytime, 100, 950, 7, 1);
    for(byte k = 0; k < 7; k++){
      if(k < rate_level){
        pixels.setPixelColor(npx_rate_channels[k], pixels.Color(0,255,5));
      } else pixels.setPixelColor(npx_rate_channels[k], pixels.Color(255,0,255));
    }
    byte delta_level = map(current_delta_sum, 0, 16, 0, 8);
    for(byte l = 0; l < 8; l++){
      if(l < delta_level){
        pixels.setPixelColor(npx_summary_channels[l], pixels.Color(125,125,125));
      } else pixels.setPixelColor(npx_summary_channels[l], pixels.Color(0,0,255));
    }
    pixels.show();
 }

// ex. <a:r:100> to tell addr a to set rate to 100
void instructCoController(char addr, char command, int payload){
    Serial1.print('<');
    Serial1.print(addr);
    Serial1.print(command);
    Serial1.print(payload);
    Serial1.print('>');
}

// vi: ft=arduino:fdm=marker:et:sw=4:tw=80
