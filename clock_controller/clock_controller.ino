#include <DHT11.h> // temp sensor library
#include <I2C_RTC.h> // rtc library
#include <Wire.h> // I2C library

// shift register w arduino docs https://docs.arduino.cc/language-reference/en/functions/advanced-io/shiftOut/
// pin that enables output from the internal storage of the register
// RCLK on 74HC595 shift register
#define LATCH_PIN 7

// pin to cycle the data through the register
// SRCLK on 74HV595 shift register
#define CLOCK_PIN 8

// serial data input pin
// SER on 74HV595 shift register
#define DATA_PIN 6

// DHT temperature sensor signal pin, initialize DHT sensor
// example code https://github.com/adafruit/DHT-sensor-library/blob/master/examples/DHTtester/DHTtester.ino
#define DHT_PIN 5
DHT11 dht11(DHT_PIN);

// rtc init 
static DS3231 RTC;
int hour = 0;
int hour12 = 0;
int minute = 0;
int second = 0;

// pushbutton pin
#define BUTTON_PIN 4

// stores the 7 segment representations of 0-9 on this circuit's setup of shift registers
// stored in order from 0, 1, 2, 3...
byte numbers[10] = {0xD7, 0x11, 0xCD, 0x5D, 0x1B, 0x5E, 0xDE, 0x15, 0xDF, 0x1F}; 

// stores the position of each digit for each display 
byte digitPosition[12] {
  0xEF, 0xDF, 0xDF, 0x7F,
  0xFE, 0xFD, 0xFB, 0xF7,
  0xBF, 0xDF, 0xEF, 0x7F
};


int temperature = 0;
int lastTempMinute = -1;

bool displayOff = false;

// button debounce helpers
unsigned long lastButtonChangeMs = 0;
bool lastButtonReading = HIGH;
bool debouncedButton = HIGH;

void setup() {
  // put your setup code here, to run once:
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(DHT_PIN, INPUT);

  // INPUT_PULLUP means turn on the internal pull up resistor in thispin so that it is HIGH by default
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // dht.begin();
  Wire.begin();

  RTC.setHourMode(false); // 24hr time mode, used to dim after 10pm until 8am
  // set the time here
  // RTC.setHours(5);
  // RTC.setMinutes(49);
  // RTC.setSeconds(0);
  RTC.begin();
  
  // get initial temp
  temperature = getTemp();
}

void loop() {
  // button toggle check
  bool reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (reading != lastButtonReading) {
    lastButtonChangeMs = now;
    lastButtonReading = reading;
  }

  // debounce button
  if ((now - lastButtonChangeMs) > 30) {
    if (reading != debouncedButton) {
      debouncedButton = reading;
      if (debouncedButton == LOW) {
        displayOff = !displayOff;
      }
    }
  }

  // get the time
  hour = RTC.getHours();
  hour12 = convertHour(hour);
  minute = RTC.getMinutes();
  second = RTC.getSeconds();

  // every 1 minute, update the last 2 digits of display 3 with the temperature
  if (minute != lastTempMinute) {
    lastTempMinute = minute;
    temperature = getTemp();
  }

  // figure out what to display
  // get individual digits from time and temp
  byte hTens = (hour12 >= 10) ? numbers[hour12 / 10] : numbers[0];
  byte hOnes = numbers[hour12 % 10];

  byte mTens = (minute >= 10) ? numbers[minute / 10] : numbers[0];
  byte mOnes = numbers[minute % 10];

  byte sTens = (second >= 10) ? numbers[second / 10] : numbers[0];
  byte sOnes = numbers[second % 10];

  // temp wont ever have leading zero
  byte tempTens = numbers[temperature / 10];
  byte tempOnes = numbers[temperature % 10];

  byte outputDigits[8] = {hTens, hOnes, mTens, mOnes, sTens, sOnes, tempTens, tempOnes};

  if (!displayOff) {
    // check if we have to dim the display due to time
    // if (dimDisplay(hour)) {
    //   writeDataSlow(outputDigits);
    // } else {
    //   writeDataOptimal(outputDigits);
    // }
    writeDataOptimal(outputDigits);

  } else {
    clearScreen();
  }
}

static int getTemp() {
  int temp = 0;
  float t = dht11.readTemperature();
  if (!isnan(t)) temp = (int)round((t * 1.8) + 32.0);
  return constrain(temp, 0, 99);
}

static void shiftOutBytes(const byte *buf, size_t n) {
  // shift out N bytes
  for (size_t i = 0; i < n; i++) {
    shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, buf[i]);
  }
}

void clearScreen() {
  byte clearScreen[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  writeDataOptimal(clearScreen);
}

void writeDataOptimal(byte displayNumbers[8]) {
  // shift out one digit per display at once, will shift out 3 digits
  // since there is at most 3 digits per display, only take 3 cycles to display everyting
  int DELAY_TIME1 = 1000;
  int DELAY_TIME2 = 1000;
  int DELAY_TIME3 = 0;

  // pass 1: display 1 digit 1, display 2 digit 1, display 3 digit 2 (off)
  byte output1[5] = {displayNumbers[0], (byte)(digitPosition[0] & digitPosition[4]), displayNumbers[3], 0x00, digitPosition[9]};
  digitalWrite(LATCH_PIN, LOW);
  shiftOutBytes(output1, 5);
  digitalWrite(LATCH_PIN, HIGH);

  delayMicroseconds(DELAY_TIME1);

  // pass 2: display 1 digit 2, display 2 digit 3, display 3 digit 3
  byte output2[5] = {displayNumbers[1], (byte)(digitPosition[1] & digitPosition[6]), displayNumbers[4], displayNumbers[6], digitPosition[10]};
  digitalWrite(LATCH_PIN, LOW);
  shiftOutBytes(output2, 5);
  digitalWrite(LATCH_PIN, HIGH);

  delayMicroseconds(DELAY_TIME2);

  // pass 3: display 1 digit 4, display 2 digit 4, display 3 digit 4
  byte output3[5] = {displayNumbers[2], (byte)(digitPosition[3] & digitPosition[7]), displayNumbers[5], displayNumbers[7], digitPosition[11]};
  digitalWrite(LATCH_PIN, LOW);
  shiftOutBytes(output3, 5);
  digitalWrite(LATCH_PIN, HIGH);

  delayMicroseconds(DELAY_TIME3);
}

void writeDataSlow(byte displayNumbers[8]) {
  // shift out one digit only, 12 iterations to display all possible digits
  byte allDigits[12] = {displayNumbers[0], displayNumbers[1], 0x00, displayNumbers[2], displayNumbers[3], 0x00, displayNumbers[4], displayNumbers[5], 0x00, 0x00, displayNumbers[6], displayNumbers[7]};
  byte output[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  
  for (int i = 0; i < 12; i++) {
    output[0] = (i < 4) ? allDigits[i] : 0x00;
    output[1] = (i < 8) ? digitPosition[i] : 0x00;
    output[2] = (4 <= i && i < 8) ? allDigits[i] : 0x00;
    output[3] = (8 < i && i< 12) ? allDigits[i] : 0x00;
    output[4] = (i >= 8) ? digitPosition[i] : 0x00;

    digitalWrite(LATCH_PIN, LOW);
    shiftOutBytes(output, 5);
    digitalWrite(LATCH_PIN, HIGH);

  }
}

int convertHour(int hour) {
  // converts 24hr time to 12hr time
  if (hour == 0) return 12;
  if (hour > 12) return hour - 12;
  return hour;
}

bool dimDisplay(int hour) {
  // dims the display if the hour is greater than 22 (10pm) and less than 08 (8pm)
  return (hour >= 22 || hour < 8);
}