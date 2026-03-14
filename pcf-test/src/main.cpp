#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <Keypad_I2C.h>

#define I2CADDR 0x20

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {0, 1, 2, 3};   // P0 P1 P2 P3
byte colPins[COLS] = {4, 5, 6};      // P4 P5 P6

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR);

void setup() {

  Serial.begin(115200);
  delay(1000);

  Wire.begin(13, 16);   // SDA, SCL

  keypad.begin();

  Serial.println("Keypad PCF8574 test start");
}

void loop() {

  char key = keypad.getKey();

  if (key) {
    Serial.print("Pressed: ");
    Serial.println(key);
  }

}