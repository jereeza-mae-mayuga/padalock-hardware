#include <Arduino.h>

#define RELAY_PIN 2

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  // ON (for low-trigger relay)
  digitalWrite(RELAY_PIN, LOW);
  delay(2000);

  // OFF
  digitalWrite(RELAY_PIN, HIGH);
  delay(2000);
}