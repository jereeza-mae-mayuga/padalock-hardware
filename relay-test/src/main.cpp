#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <ESP32QRCodeReader.h>

#define RELAY_PIN 12
#define REED_PIN 14
#define FLASH_LED 4

#define SDA_PIN 13
#define SCL_PIN 2
#define PCF_ADDR 0x20

const char* ssid = "ZTE_2.4G_7QFQX7";
const char* password = "YU2GR4Tx";
const char* LOCKER_CODE = "LOCKER_1779378809171";
const char* API_URL = "http://192.168.1.12:3000/api/iot";

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

Keypad_I2C keypad(
    makeKeymap(keys),
    rowPins,
    colPins,
    ROWS,
    COLS,
    PCF_ADDR
);

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER, FRAMESIZE_CIF);
QRCodeData qrCodeData;

bool unlocked = false;
bool lidOpened = false;
unsigned long lidOpenTime = 0;
bool lidAlertSent = false;
//bool qrUsed = false;

enum Mode
{
    NONE,
    DELIVERY,
    OWNER
};

Mode currentMode = NONE;

int expectedParcels = 0;
int validatedParcels = 0;

bool waitingForParcelCount = false;
bool deliverySession = false;

String trackingInput = "";

// =========================
// VERIFY CODE
// =========================

String verifyCode(String code)
{
    if (WiFi.status() != WL_CONNECTED)
        return "";

    HTTPClient http;

    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");

    String body =
    "{\"code\":\"" + code +
    "\",\"action\":\"VERIFY\"" +
    ",\"lockerCode\":\"" + String(LOCKER_CODE) +
    "\"}";

    int httpCode = http.POST(body);

    if (httpCode == 200)
    {
        String response = http.getString();
        http.end();
        return response;
    }

    http.end();
    return "";
}

void sendAction(String action)
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;

    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");

    String body =
    "{\"action\":\"" + action +
    "\",\"lockerCode\":\"" + String(LOCKER_CODE) +
    "\"}";

    http.POST(body);

    http.end();
}
void blinkValid()
{
    digitalWrite(FLASH_LED, HIGH);
    delay(150);
    digitalWrite(FLASH_LED, LOW);
}

void blinkInvalid()
{
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(FLASH_LED, HIGH);
        delay(100);
        digitalWrite(FLASH_LED, LOW);
        delay(100);
    }
}

// =========================
// LOCK CONTROL
// =========================

void unlockBox()
{
    digitalWrite(RELAY_PIN, HIGH);
    unlocked = true;
    lidOpened = false;
}

void lockBox()
{
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(FLASH_LED, LOW);
    unlocked = false;
    lidOpened = false;
}

// =========================
// SETUP
// =========================

void setup()
{
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(REED_PIN, INPUT_PULLUP);
    pinMode(FLASH_LED, OUTPUT);
    digitalWrite(FLASH_LED, LOW);

    lockBox();

    Wire.begin(SDA_PIN, SCL_PIN);
    keypad.begin();

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    Serial.begin(115200);

    Serial.println("LOCKER:");
    Serial.println(LOCKER_CODE);

    reader.setup();
    reader.beginOnCore(1);
}

// =========================
// LOOP
// =========================

void loop()
{
    // =========================
    // QR SCAN
    // =========================

    if (!unlocked &&
        deliverySession &&
        reader.receiveQrCode(&qrCodeData, 10))
    {
        if (qrCodeData.valid)
        {
            String qrValue = String((char*)qrCodeData.payload);

            String result = verifyCode(qrValue);

            if (result.indexOf("DELIVERY") >= 0)
            {
                validatedParcels++;
                blinkValid();

                if (validatedParcels >= expectedParcels)
                {
                    currentMode = DELIVERY;

                    unlockBox();

                    sendAction("LOCK_OPEN");

                    deliverySession = false;
                }
            }
        }
    }

    // =========================
    // KEYPAD
    // =========================

    char key = keypad.getKey();

    static bool systemAwake = false;
    static String inputCode = "";

    if (key)
    {
        if (waitingForParcelCount)
        {
            if (key >= '1' && key <= '9')
            {
                expectedParcels = key - '0';

                waitingForParcelCount = false;
                deliverySession = true;

                systemAwake = false;

                trackingInput = "";

                Serial.print("EXPECTING PARCELS: ");
                Serial.println(expectedParcels);

                return;
            }
        }

        if (key == '*')
        {
            waitingForParcelCount = true;

            expectedParcels = 0;
            validatedParcels = 0;

            trackingInput = "";
            inputCode = "";

            deliverySession = false;
            systemAwake = false;

            return;
        }
        else if (!systemAwake && !deliverySession)
        {
            if (key == '#')
            {
                systemAwake = true;
                inputCode = "";
                return;
            }
        }
        else if (key >= '0' && key <= '9')
        {
            Serial.print("TRACK INPUT: ");
            Serial.println(trackingInput);
            if (deliverySession)
            {
                trackingInput += key;

                if (trackingInput.length() == 7)
                {
                    String result = verifyCode(trackingInput);

                    if (deliverySession &&
                        result.indexOf("DELIVERY") >= 0)
                    {
                        validatedParcels++;

                        blinkValid();

                        trackingInput = "";

                        if (validatedParcels >= expectedParcels)
                        {
                            currentMode = DELIVERY;

                            unlockBox();

                            sendAction("LOCK_OPEN");

                            deliverySession = false;
                        }
                    }
                    else
                    {
                        sendAction("INVALID_CODE");
                        blinkInvalid();

                        trackingInput = "";
                    }
                }

                return;
            }

            inputCode += key;

            if (inputCode.length() == 4)
            {
                String result = verifyCode(inputCode);

                if (result.indexOf("OWNER") >= 0)
                {
                    sendAction("PIN_VALID");

                    currentMode = OWNER;

                    unlockBox();

                    sendAction("LOCK_OPEN");
                    systemAwake = false;
                }
        
                else
                {
                    sendAction("INVALID_CODE");
                    blinkInvalid();
                }

                inputCode = "";
                systemAwake = false;
            }
        }
    }

    // =========================
    // REED SWITCH
    // =========================

    if (unlocked)
    {
        if (lidOpened &&
        !lidAlertSent &&
        millis() - lidOpenTime >= 60000)
    {
        sendAction("LID_OPEN_TOO_LONG");

        lidAlertSent = true;
    }
        if (digitalRead(REED_PIN) == HIGH)
{
        if (!lidOpened)
        {
            lidOpened = true;
            lidOpenTime = millis();
            lidAlertSent = false;
        }
    }

        if (lidOpened && digitalRead(REED_PIN) == LOW)
        {
            if (currentMode == OWNER)
            {
                sendAction("RETRIEVE");
            }
            else if (currentMode == DELIVERY)
            {
                sendAction("DELIVERED");
            }

            currentMode = NONE;
            deliverySession = false;
            validatedParcels = 0;
            expectedParcels = 0;
            trackingInput = "";

            lockBox();
            sendAction("LOCK_CLOSED");

            //qrUsed = false; 

            lidOpened = false;

            delay(1000);
        }
    }

    delay(10);
}