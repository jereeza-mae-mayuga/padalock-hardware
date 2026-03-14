#include <Arduino.h>

#define RELAY_PIN 13   // change if your relay is on another pin

void setup() {
  pinMode(RELAY_PIN, OUTPUT);

  // start locked / off
  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  digitalWrite(RELAY_PIN, HIGH);   // relay ON
  delay(2000);

  digitalWrite(RELAY_PIN, LOW);    // relay OFF
  delay(2000);
}