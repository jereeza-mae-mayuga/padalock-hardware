#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("ESP32 CAM TEST START");
}

void loop() {
  Serial.println("RUNNING");
  delay(1000);
}