#include <Arduino.h>

#define RELAY_PIN 13

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
}

// toggle relay every 1 second
void loop() {
  digitalWrite(RELAY_PIN, HIGH);
  delay(1000);

  digitalWrite(RELAY_PIN, LOW);
  delay(1000);
}