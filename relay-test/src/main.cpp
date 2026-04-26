#include <Arduino.h>

#define RELAY_PIN 3

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("START");

  pinMode(RELAY_PIN, OUTPUT);

  // OFF at start (important for low-trigger relay)
  digitalWrite(RELAY_PIN, HIGH);
}

void loop() {
  Serial.println("RELAY ON");
  digitalWrite(RELAY_PIN, LOW);   // ON
  delay(2000);

  Serial.println("RELAY OFF");
  digitalWrite(RELAY_PIN, HIGH);  // OFF
  delay(2000);
}