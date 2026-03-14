#include <Arduino.h>

#define TRIG 14
#define ECHO 15

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
  digitalWrite(TRIG, LOW);

  Serial.begin(115200);
  delay(1000);
  Serial.println("Ultrasonic test start");
}

void loop() {
  long d = readDistance();

  Serial.print("Distance: ");
  Serial.println(d);

  delay(500);
}