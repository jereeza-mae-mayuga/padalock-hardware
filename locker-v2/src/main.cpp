#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
///#include <ESP32QRCodeReader.h>

// =====================================================
// PINS
// =====================================================
#define RELAY_PIN 12
#define FLASH_LED 4

#define TRIG_PIN 14
#define ECHO_PIN 15

#define SDA_PIN 13
#define SCL_PIN 2

#define PCF_ADDR 0x20

// =====================================================
// WIFI + API
// =====================================================
const char* ssid = "ZTE_2.4G_7QFQX7";
const char* password = "YU2GR4Tx";

const char* LOCKER_CODE = "LOCKER_1779378809171";
const char* API_URL = "https://padalock-web.vercel.app/api/iot";

const char* DELIVER_API =
    "https://padalock-web.vercel.app/api/parcels/deliver";

const char* CAMERA_COMMAND_API =
    "https://padalock-web.vercel.app/api/camera-command";

// =====================================================
// ULTRASONIC
// =====================================================
const int OBJECT_THRESHOLD = 13;

const unsigned long DELIVERY_CONFIRM_MS = 4000;
const unsigned long RETRIEVAL_CONFIRM_MS = 4000;

unsigned long detectionStart = 0;

// Helps prevent OWNER mode from immediately locking

// =====================================================
// KEYPAD
// =====================================================
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

// =====================================================
// QR CAMERA
// This is the LOCKER ESP32-CAM camera.
// NOT the second surveillance ESP32.
// =====================================================
/*ESP32QRCodeReader reader(
    CAMERA_MODEL_AI_THINKER,
    FRAMESIZE_QVGA
);

QRCodeData qrCodeData;*/

// =====================================================
// STATE
// =====================================================
enum Mode
{
    NONE,
    DELIVERY,
    OWNER
};

Mode currentMode = NONE;

bool unlocked = false;

bool waitingForParcelCount = false;
bool deliverySession = false;

int expectedParcels = 0;
int validatedParcels = 0;

String trackingInput = "";
String ownerInput = "";

String deliveredTrackingNumbers = "";

// =====================================================
// LED
// =====================================================
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

// =====================================================
// API VERIFY
// =====================================================
String verifyCode(String code, String verifyType)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        blinkInvalid();
        return "";
    }

    HTTPClient http;

    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");

    String body =
        "{\"code\":\"" + code +
        "\",\"action\":\"VERIFY\"" +
        ",\"verifyType\":\"" + verifyType + "\"" +
        ",\"lockerCode\":\"" + String(LOCKER_CODE) +
        "\"}";

    int httpCode = http.POST(body);

    String response = "";

    if (httpCode == 200)
    {
        response = http.getString();
    }

    http.end();
    return response;
}

// =====================================================
// SEND SYSTEM ACTION
// =====================================================
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

void sendCameraCommand(
    String command,
    String eventType = "",
    String trackingNumber = ""
)
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;

    http.begin(CAMERA_COMMAND_API);
    http.addHeader("Content-Type", "application/json");

    String body =
        "{\"lockerCode\":\"" + String(LOCKER_CODE) +
        "\",\"command\":\"" + command + "\"";

    if (eventType.length() > 0)
    {
        body += ",\"eventType\":\"" + eventType + "\"";
    }

    if (trackingNumber.length() > 0)
    {
        body += ",\"trackingNumber\":\"" + trackingNumber + "\"";
    }

    body += "}";

    http.POST(body);
    http.end();
}

// =====================================================
// SEND DELIVERED PARCELS
// =====================================================
void sendDeliveredParcels()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    if (deliveredTrackingNumbers.length() == 0)
        return;

    HTTPClient http;

    http.begin(DELIVER_API);
    http.addHeader("Content-Type", "application/json");

    String body = "{\"trackingNumbers\":[";

    int start = 0;

    while (true)
    {
        int commaIndex =
            deliveredTrackingNumbers.indexOf(',', start);

        String number;

        if (commaIndex == -1)
        {
            number =
                deliveredTrackingNumbers.substring(start);
        }
        else
        {
            number =
                deliveredTrackingNumbers.substring(
                    start,
                    commaIndex
                );
        }

        body += "\"" + number + "\"";

        if (commaIndex == -1)
            break;

        body += ",";

        start = commaIndex + 1;
    }

    body += "]}";

    http.POST(body);

    http.end();
}

// =====================================================
// CHECK IF TRACKING NUMBER ALREADY ENTERED
// =====================================================
bool trackingAlreadyAdded(String code)
{
    int start = 0;

    while (start < deliveredTrackingNumbers.length())
    {
        int commaIndex =
            deliveredTrackingNumbers.indexOf(',', start);

        String existing;

        if (commaIndex == -1)
        {
            existing =
                deliveredTrackingNumbers.substring(start);
        }
        else
        {
            existing =
                deliveredTrackingNumbers.substring(
                    start,
                    commaIndex
                );
        }

        if (existing == code)
            return true;

        if (commaIndex == -1)
            break;

        start = commaIndex + 1;
    }

    return false;
}

// =====================================================
// LOCK CONTROL
// =====================================================
void unlockBox()
{
    digitalWrite(RELAY_PIN, HIGH);

    unlocked = true;
    detectionStart = 0;
}

void lockBox()
{
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(FLASH_LED, LOW);

    unlocked = false;
    detectionStart = 0;
}

// =====================================================
// ULTRASONIC
// =====================================================
long readDistanceCm()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    long duration =
        pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0)
        return -1;

    return duration * 0.034 / 2;
}

// =====================================================
// RESET DELIVERY SESSION
// =====================================================
void resetDelivery()
{
    deliverySession = false;
    waitingForParcelCount = false;

    expectedParcels = 0;
    validatedParcels = 0;

    trackingInput = "";
    deliveredTrackingNumbers = "";
}

// =====================================================
// VALID DELIVERY TRACKING NUMBER
// Used by BOTH keypad and QR
// =====================================================
void acceptDeliveryCode(String code)
{
    // Don't count the same tracking number twice
    if (trackingAlreadyAdded(code))
    {
        blinkInvalid();
        return;
    }

    validatedParcels++;

    if (deliveredTrackingNumbers.length() > 0)
        deliveredTrackingNumbers += ",";

    deliveredTrackingNumbers += code;

    blinkValid();

    // All expected parcels were validated
    if (validatedParcels >= expectedParcels)
    {
        currentMode = DELIVERY;

        unlockBox();

        sendAction("LOCK_OPEN");

        // Start DELIVERY surveillance recording
        sendCameraCommand(
            "START",
            "DELIVERY",
            deliveredTrackingNumbers
        );

        deliverySession = false;
    }
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
    Serial.begin(115200);

    pinMode(RELAY_PIN, OUTPUT);

    pinMode(FLASH_LED, OUTPUT);
    digitalWrite(FLASH_LED, LOW);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    lockBox();

    // Keypad / PCF8574
    Wire.begin(SDA_PIN, SCL_PIN);
    keypad.begin();

    // Wi-Fi
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    // Two blinks = WiFi connected
    blinkValid();
    delay(150);
    blinkValid();

    /* QR camera
    reader.setup();
    reader.beginOnCore(1);*/
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
    // =================================================
    // QR DELIVERY SCANNING
    // Only active after:
    //
    // * -> number of parcels
    // =================================================
    /*if (!unlocked &&
        deliverySession &&
        reader.receiveQrCode(&qrCodeData, 100))
    {
        if (qrCodeData.valid)
        {
            String qrValue =
                String((char*)qrCodeData.payload);

            qrValue.trim();

            String result =
                verifyCode(qrValue);

            if (result.indexOf("DELIVERY") >= 0)
            {
                acceptDeliveryCode(qrValue);
            }
            else
            {
                blinkInvalid();
            }
        }
    }*/

    // =================================================
    // KEYPAD
    // =================================================
    char key = keypad.getKey();

    if (key)
    {
        // =============================================
        // WAITING FOR NUMBER OF PARCELS
        // =============================================
        if (waitingForParcelCount)
        {
            if (key >= '1' && key <= '9')
            {
                expectedParcels = key - '0';

                validatedParcels = 0;

                deliveredTrackingNumbers = "";
                trackingInput = "";

                waitingForParcelCount = false;
                deliverySession = true;

                blinkValid();

                return;
            }

            // Cancel
            if (key == '#')
            {
                resetDelivery();
                return;
            }
        }

        // =============================================
        // * = START DELIVERY
        // =============================================
        else if (!unlocked && key == '*')
        {
            resetDelivery();

            waitingForParcelCount = true;

            return;
        }

        // =============================================
        // # = START OWNER PIN
        // =============================================
        else if (!unlocked &&
                 !deliverySession &&
                 key == '#')
        {
            ownerInput = "";

            return;
        }

        // =============================================
        // DELIVERY TRACKING NUMBER BY KEYPAD
        // =============================================
        else if (!unlocked &&
                 deliverySession &&
                 key >= '0' &&
                 key <= '9')
        {
            trackingInput += key;

            // Your current tracking-number test uses 7 digits
            if (trackingInput.length() == 7)
            {
                String result =
                    verifyCode(trackingInput, "DELIVERY");

                if (result.indexOf("DELIVERY") >= 0)
                {
                    acceptDeliveryCode(trackingInput);
                }
                else
                {
                    blinkInvalid();
                }

                trackingInput = "";
            }

            return;
        }

        // =============================================
        // OWNER PIN
        // Current owner PIN = 4 digits
        // =============================================
        else if (!unlocked &&
                 !deliverySession &&
                 key >= '0' &&
                 key <= '9')
        {
            ownerInput += key;

            if (ownerInput.length() == 4)
            {
                String result =
                    verifyCode(ownerInput, "OWNER");
                if (result.indexOf("OWNER") >= 0)
                {
                    blinkValid();

                    currentMode = OWNER;


                    unlockBox();

                    sendAction("PIN_VALID");
                    sendAction("LOCK_OPEN");

                    // Start RETRIEVAL surveillance
                    sendCameraCommand(
                        "START",
                        "RETRIEVE"
                    );
                }
                else
                {
                    blinkInvalid();
                }

                ownerInput = "";
            }
        }
    }

    // =================================================
    // ULTRASONIC
    // =================================================
    if (unlocked)
    {
        long distance =
            readDistanceCm();

        // TEMP TEST:
        // LED ON = ultrasonic sees something within 25 cm
        // LED OFF = no valid detection
        /*
        if (distance != -1 && distance < 25)
        {
            digitalWrite(FLASH_LED, HIGH);
        }
        else
        {
            digitalWrite(FLASH_LED, LOW);
        }*/

        bool objectDetected =
            (
                distance != -1 &&
                distance < OBJECT_THRESHOLD
            );

        // =============================================
        // DELIVERY
        //
        // Object appears for 2 seconds -> LOCK
        // =============================================
        if (currentMode == DELIVERY)
{
    // Parcel must remain detected for 4 seconds
        if (objectDetected)
        {
            if (detectionStart == 0)
            {
                detectionStart = millis();
            }

            if (millis() - detectionStart >= DELIVERY_CONFIRM_MS)
            {
                lockBox();

                currentMode = NONE;

                sendCameraCommand("STOP");

                sendDeliveredParcels();
                sendAction("LOCK_CLOSED");

                resetDelivery();
            }
        }
        else
        {
            // Detection broke, start confirmation over
            detectionStart = 0;
        }
    }

        // =============================================
        // OWNER
        //
        // First confirm an object was there.
        // THEN wait for it to disappear.
        // =============================================
        else if (currentMode == OWNER)
        {
            // Box must remain clear for 4 seconds
            if (!objectDetected)
            {
                if (detectionStart == 0)
                {
                    detectionStart = millis();
                }

                if (millis() - detectionStart >= RETRIEVAL_CONFIRM_MS)
                {
                    lockBox();

                    currentMode = NONE;

                    // Create the NEW retrieval log FIRST
                    sendAction("RETRIEVE");

                    // Then stop camera.
                    // AVI building/upload happens after this,
                    // so the new RETRIEVE log already exists.
                    sendCameraCommand("STOP");

                    sendAction("LOCK_CLOSED");
                }
            }
            else
            {
                // Parcel/object still inside
                detectionStart = 0;
            }
        }
    }

    delay(20);
}