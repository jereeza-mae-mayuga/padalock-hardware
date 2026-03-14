#include <Arduino.h>

#define TRIG 14
#define ECHO 15
#define RELAY_PIN 13   // change if needed

long readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);

  if (duration == 0) return -1;

  return duration * 0.034 / 2;
}

void setup() {
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(TRIG, LOW);
  digitalWrite(RELAY_PIN, LOW); // change to HIGH if active LOW relay

  Serial.begin(115200);
  delay(1000);
}

void loop() {
  long d = readDistance();

  Serial.print("Distance: ");
  Serial.println(d);

  if (d > 0 && d < 15) {
    digitalWrite(RELAY_PIN, HIGH);   // change if active LOW
  } else {
    digitalWrite(RELAY_PIN, LOW);    // change if active LOW
  }

  delay(300);
}