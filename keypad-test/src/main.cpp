#include <Arduino.h>
#include <Keypad.h>

#define RELAY_PIN 1

bool relayActiveHigh = false;
// =========================
// KEYPAD
// =========================
const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {12,13,14,15};
byte colPins[COLS] = {2,16,3};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// =========================
// ACCESS CODES
// =========================
String code1 = "1234";
String code2 = "5678";

String input = "";
bool systemAwake = false;

// =========================
void lockBox(){
  digitalWrite(RELAY_PIN, relayActiveHigh ? LOW : HIGH);
}

void unlockBox(){
  digitalWrite(RELAY_PIN, relayActiveHigh ? HIGH : LOW);
}

// =========================
void resetSystem(){
  systemAwake = false;
  input = "";
  lockBox();
}

// =========================
void setup(){

  pinMode(RELAY_PIN, OUTPUT);
  lockBox();

}

// =========================
void loop(){

  char key = keypad.getKey();
  if(!key) return;

  if(key == '*'){
    resetSystem();
    return;
  }

  if(!systemAwake){

    if(key == '#'){
      systemAwake = true;
      input = "";
    }

    return;
  }

  if(key >= '0' && key <= '9'){

    input += key;

    if(input.length() == 4){

      if(input == code1 || input == code2){

        unlockBox();
        delay(5000);
        lockBox();

      }

      resetSystem();
    }
  }

}