#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <Keypad_I2C.h>

// =========================
// PIN SETUP
// =========================
#define SDA_PIN     13
#define SCL_PIN     16

#define TRIG_PIN    14
#define ECHO_PIN    15

#define RELAY_PIN   1

// =========================
// PCF8574 / KEYPAD
// =========================
#define PCF_ADDR    0x20

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {0, 1, 2, 3};   // P0 P1 P2 P3
byte colPins[COLS] = {4, 5, 6};      // P4 P5 P6

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, PCF_ADDR);

// =========================
// ACCESS CODE
// =========================
const String validCode = "1234";

// =========================
// RELAY SETTINGS
// =========================
bool relayActiveHigh = false;

// =========================
// SENSOR SETTINGS
// =========================
const int parcelThresholdCm = 15;
const unsigned long detectHoldMs = 1500;

// =========================
// STATE
// =========================
bool systemAwake = false;
bool boxUnlocked = false;
String inputCode = "";

unsigned long detectStart = 0;

// =========================
// FUNCTIONS
// =========================
void lockBox() {
  digitalWrite(RELAY_PIN, relayActiveHigh ? LOW : HIGH);
  boxUnlocked = false;
}

void unlockBox() {
  digitalWrite(RELAY_PIN, relayActiveHigh ? HIGH : LOW);
  boxUnlocked = true;
}

void resetSystem() {
  systemAwake = false;
  inputCode = "";
  detectStart = 0;
  lockBox();
}

long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return -1;

  long distance = duration * 0.034 / 2;
  return distance;
}

bool isValidCode(const String& code) {
  return (code == validCode);
}

// =========================
// SETUP
// =========================
void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);
  lockBox();

  Wire.begin(SDA_PIN, SCL_PIN);
  keypad.begin();
}

// =========================
// LOOP
// =========================
void loop() {
  char key = keypad.getKey();

  if (key) {
    if (key == '*') {
      resetSystem();
      return;
    }

    if (!systemAwake) {
      if (key == '#') {
        systemAwake = true;
        inputCode = "";
      }
      return;
    }

    if (!boxUnlocked && key >= '0' && key <= '9') {
      inputCode += key;

      if (inputCode.length() == 4) {
        if (isValidCode(inputCode)) {
          unlockBox();
          detectStart = 0;
        } else {
          resetSystem();
        }
      }
    }
  }

  if (boxUnlocked) {
    long d = readDistanceCm();

    if (d > 0 && d < parcelThresholdCm) {
      if (detectStart == 0) {
        detectStart = millis();
      }

      if (millis() - detectStart >= detectHoldMs) {
        lockBox();
        systemAwake = false;
        inputCode = "";
        detectStart = 0;
      }
    } else {
      detectStart = 0;
    }
  }

  delay(30);
}