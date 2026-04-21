#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

// =========================
// WIFI + BACKEND
// =========================
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const char* serverURL = "http://192.168.1.29:3000/api/iot/update";

// =========================
// PIN SETUP
// =========================
#define SDA_PIN     13
#define SCL_PIN     16
#define TRIG_PIN    14
#define ECHO_PIN    15
#define RELAY_PIN   1

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
const String validCode = "1234";
bool relayActiveHigh = false;

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
// WIFI CONNECT
// =========================
void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

// =========================
// SEND TO BACKEND
// =========================
void sendStatus(String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"status\":\"" + status + "\"}";
    http.POST(json);

    http.end();
  }
}

// =========================
// LOCK / UNLOCK
// =========================
void lockBox() {
  digitalWrite(RELAY_PIN, relayActiveHigh ? LOW : HIGH);
  boxUnlocked = false;
  sendStatus("LOCKED");
}

void unlockBox() {
  digitalWrite(RELAY_PIN, relayActiveHigh ? HIGH : LOW);
  boxUnlocked = true;
  sendStatus("UNLOCKED");
}

void resetSystem() {
  systemAwake = false;
  inputCode = "";
  detectStart = 0;
  lockBox();
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
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);
  lockBox();

  Wire.begin(SDA_PIN, SCL_PIN);
  keypad.begin();

  connectWiFi();
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
        if (inputCode == validCode) {
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
        sendStatus("DELIVERED");  
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