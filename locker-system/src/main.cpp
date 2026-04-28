#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <Keypad_I2C.h>

// =========================
// PINS
// =========================
#define SDA_PIN     2
#define SCL_PIN     16

#define TRIG_PIN    14
#define ECHO_PIN    15

#define RELAY_PIN   13   // working pin

// =========================
// KEYPAD
// =========================
#define PCF_ADDR 0x20

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {0,1,2,3};
byte colPins[COLS] = {4,5,6};

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, PCF_ADDR);

// =========================
// SETTINGS
// =========================
const String ownerCode  = "1234";
const String parcelCode = "5678";
const int threshold = 13;

// =========================
// STATE
// =========================
bool systemAwake = false;
bool boxUnlocked = false;
String inputCode = "";

enum Mode {
  NONE,
  OWNER,
  DELIVERY
};

Mode currentMode = NONE;

// =========================
// RELAY (ACTIVE LOW)
// =========================
void lockBox() {
  digitalWrite(RELAY_PIN, LOW);   // OFF (LOCKED)
  boxUnlocked = false;
  currentMode = NONE;
}

void unlockBox() {
  digitalWrite(RELAY_PIN, HIGH);  // ON (UNLOCKED)
  boxUnlocked = true;
}

// =========================
// ULTRASONIC
// =========================
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  return duration * 0.034 / 2;
}

// =========================
// SETUP
// =========================
void setup() {
  // 🔥 correct boot behavior
  digitalWrite(RELAY_PIN, LOW);  // start LOCKED
  pinMode(RELAY_PIN, OUTPUT);

  pinMode(4, OUTPUT);   // kill flash LED
  digitalWrite(4, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

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
      systemAwake = false;
      inputCode = "";
      lockBox();
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

        if (inputCode == ownerCode) {
          unlockBox();
          currentMode = OWNER;
        }
        else if (inputCode == parcelCode) {
          unlockBox();
          currentMode = DELIVERY;
        }
        else {
          systemAwake = false;
          lockBox();
        }

        inputCode = "";
      }
    }
  }

  if (boxUnlocked) {
    long d = readDistanceCm();

    if (currentMode == DELIVERY) {
      // parcel placed → CLOSE
      if (d != -1 && d < threshold) {
        lockBox();
        systemAwake = false;
      }
    }
    else if (currentMode == OWNER) {
      // parcel removed → CLOSE
      if (d == -1 || d >= threshold) {
        lockBox();
        systemAwake = false;
      }
    }
  }

  delay(50);
}
