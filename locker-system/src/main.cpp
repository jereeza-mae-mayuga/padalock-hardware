#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

// =========================
// WIFI
// =========================
const char* ssid = "IoEnnovate2026_3";
const char* password = "Sdca@wifi";
const char* serverURL = "http://192.168.0.103:3000/api/iot";

// =========================
// LOCKER
// =========================
const String lockerCode = "LOCKER_1777220125689";

// =========================
// PINS
// =========================
#define SDA_PIN 2
#define SCL_PIN 16
#define RELAY_PIN 13
#define TRIG_PIN 14
#define ECHO_PIN 15

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
// STATE
// =========================
const int threshold = 13;
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
// RELAY
// =========================
void lockBox() {
  digitalWrite(RELAY_PIN, LOW);
  boxUnlocked = false;
}

void unlockBox() {
  digitalWrite(RELAY_PIN, HIGH);
  boxUnlocked = true;
}

// =========================
// WIFI CONNECT
// =========================
void maintainWiFi() {
  static unsigned long lastTry = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastTry > 5000) {
      Serial.println("Reconnecting WiFi...");
      WiFi.begin(ssid, password);
      lastTry = millis();
    }
  }
}

// =========================
// VERIFY FROM BACKEND
// =========================
String verifyCode(String code) {
  if (WiFi.status() != WL_CONNECTED) return "";

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, serverURL)) return "";

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  String body = "{\"code\":\"" + code + "\",\"lockerCode\":\"" + lockerCode + "\"}";
  int httpCode = http.POST(body);

  String response = "";
  if (httpCode > 0) {
    response = http.getString();
  }

  http.end();
  return response;
}

// =========================
// SEND LOG
// =========================
void sendLog(String status) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"status\":\"" + status + "\",\"lockerCode\":\"" + lockerCode + "\"}";
  http.POST(body);

  http.end();

  Serial.println("LOG SENT: " + status);
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
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  keypad.begin();

  WiFi.begin(ssid, password);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

// =========================
// LOOP
// =========================
void loop() {

  maintainWiFi();

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

    if (key >= '0' && key <= '9') {
      inputCode += key;

      if (inputCode.length() == 4) {

        String result = verifyCode(inputCode);

        if (result.indexOf("\"mode\":\"OWNER\"") != -1) {

          sendLog("PIN_VALID");
          unlockBox();
          sendLog("LOCK_OPEN");

          currentMode = OWNER;
        }

        else if (result.indexOf("\"mode\":\"DELIVERY\"") != -1) {

          sendLog("DELIVERY_VALID");
          unlockBox();
          sendLog("LOCK_OPEN");

          currentMode = DELIVERY;
        }

        else {
          sendLog("INVALID_PIN");
          lockBox();
        }

        inputCode = "";
        systemAwake = false;
      }
    }
  }

  // =========================
  // SENSOR LOGIC
  // =========================
  if (boxUnlocked) {

    static unsigned long detectStartTime = 0;
    static bool detecting = false;

    long d = readDistanceCm();

    if (d == -1) {
      detecting = false;
      detectStartTime = 0;
    } else {

      // DELIVERY
      if (currentMode == DELIVERY) {

        if (d > 0 && d < threshold) {

          if (!detecting) {
            detecting = true;
            detectStartTime = millis();
          }

          if (millis() - detectStartTime >= 3000) {

            sendLog("PARCEL_DETECTED");

            lockBox();
            sendLog("LOCK_CLOSED");

            currentMode = NONE;
            detecting = false;
          }

        } else {
          detecting = false;
          detectStartTime = 0;
        }
      }

      // OWNER
      else if (currentMode == OWNER) {

        if (d > threshold) {

          if (!detecting) {
            detecting = true;
            detectStartTime = millis();
          }

          if (millis() - detectStartTime >= 3000) {

            sendLog("PARCEL_REMOVED");

            lockBox();
            sendLog("LOCK_CLOSED");

            currentMode = NONE;
            detecting = false;
          }

        } else {
          detecting = false;
          detectStartTime = 0;
        }
      }
    }
  }

  delay(50);
}