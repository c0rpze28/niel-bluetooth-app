#include <Arduino.h>
#include <NimBLEDevice.h>

// -----------------------------
// Pins
// -----------------------------
#define ANALOG_PIN 0
#define ANALOG_PIN2 2

#define IN1 21
#define IN2 20
#define EN_PIN 10

#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8
#define MAX_DUTY 255

// -----------------------------
// Motor states
// 0 = stopped
// 1 = cold (CW)
// 2 = hot (CCW)
// -----------------------------
int currentState = 0;

// Desired temperature thresholds
float desiredCold = 20.0;
float desiredHot  = 50.0;

// Sensor calibration
#define TEMP_MIN 15.0
#define TEMP_MAX 60.0
#define ANALOG_MIN 1965
#define ANALOG_MAX 1470

// -----------------------------
// BLE UUIDs (NUS)
// -----------------------------
#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pRxCharacteristic = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;

// -----------------------------
// Function declarations
// -----------------------------
void rampUpPWM(const char* stateName);
void stopMotor();
float analogToTemp(int sensorValue);
void setupBLE();
void handleCommand(const std::string& cmd);

// -----------------------------
// Setup
// -----------------------------
void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(EN_PIN, PWM_CHANNEL);

    stopMotor();
    setupBLE();
}

// -----------------------------
// Main loop
// -----------------------------
void loop() {
    int sensorValue = analogRead(ANALOG_PIN);
    int sensorValue2 = analogRead(ANALOG_PIN2);
    float tempC = analogToTemp(sensorValue);

    // Debug
    Serial.print("Temp: "); Serial.print(tempC, 1); Serial.print("°C | Analog0: ");
    Serial.print(sensorValue); Serial.print(" | Analog2: "); Serial.println(sensorValue2);

    // Motor control based on temperature
    if (tempC < desiredCold && currentState != 1) {
        stopMotor();
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        Serial.println("Direction: CLOCKWISE (COLD)");
        rampUpPWM("COLD");
        currentState = 1;
    }
    else if (tempC > desiredHot && currentState != 2) {
        stopMotor();
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        Serial.println("Direction: COUNTERCLOCKWISE (HOT)");
        rampUpPWM("HOT");
        currentState = 2;
    }
    else if (tempC >= desiredCold && tempC <= desiredHot && currentState != 0) {
        stopMotor();
        currentState = 0;
    }

    delay(500);
}

// -----------------------------
// PWM helpers
// -----------------------------
void rampUpPWM(const char* stateName) {
    delay(2000);
    for (int duty = 0; duty <= MAX_DUTY; duty++) {
        ledcWrite(PWM_CHANNEL, duty);
        Serial.print("State: "); Serial.print(stateName); Serial.print(" | PWM: "); Serial.println(duty);
        delay(15000 / MAX_DUTY);
    }
    ledcWrite(PWM_CHANNEL, MAX_DUTY);
    Serial.println("Reached MAX POWER and holding.");
}

void stopMotor() {
    ledcWrite(PWM_CHANNEL, 0);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    Serial.println("State: STOP | PWM: 0");
}

// -----------------------------
// Temperature mapping
// -----------------------------
float analogToTemp(int sensorValue) {
    sensorValue = constrain(sensorValue, ANALOG_MAX, ANALOG_MIN);
    return map(sensorValue, ANALOG_MIN, ANALOG_MAX, TEMP_MIN, TEMP_MAX);
}

// -----------------------------
// BLE setup
// -----------------------------
class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic)  {
        std::string cmd = pCharacteristic->getValue();
        if (!cmd.empty()) handleCommand(cmd);
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
        Serial.println("Client connected");
        NimBLEDevice::getAdvertising()->stop();
    }
    
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
        Serial.print("Client disconnected. Reason: ");
        Serial.println(reason);
        
        delay(500);
        NimBLEDevice::startAdvertising();
        Serial.println("Advertising restarted");
    }
    
    // ADD THIS - This is the key fix
    uint32_t onPassKeyRequest() {
        Serial.println("PassKey Request");
        return 123456;
    }
    
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) {
        if(!connInfo.isEncrypted()) {
            Serial.println("Encrypt connection failed - disconnecting");
            pServer->disconnect(connInfo.getConnHandle());
        }
    }
};

void setupBLE() {
    NimBLEDevice::init("ESP32-C3-NUS");
    NimBLEDevice::deleteAllBonds();
    NimBLEDevice::setSecurityAuth(false, false, true);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    pServer->advertiseOnDisconnect(true);
    NimBLEService* pService = pServer->createService(NUS_SERVICE_UUID);

    // RX characteristic (WRITE)
    pRxCharacteristic = pService->createCharacteristic(
        NUS_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pRxCharacteristic->setCallbacks(new RxCallbacks());

    // TX characteristic (NOTIFY)
    pTxCharacteristic = pService->createCharacteristic(
        NUS_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // CCCD descriptor (required for notifications)
    NimBLEDescriptor* cccd = new NimBLEDescriptor(
        "2902",      // UUID as string
        0,           // properties (0 is fine for CCCD)
        2,           // max length
        pTxCharacteristic
    );
    // initialize CCCD value to 0x00 0x00 (notifications off by default)
    uint8_t cccd_value[2] = {0x00, 0x00};
    cccd->setValue(cccd_value, 2);
    pTxCharacteristic->addDescriptor(cccd);

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    NimBLEDevice::startAdvertising();

    Serial.println("BLE NUS server started and advertising.");
}


// -----------------------------
// Handle commands from app
// -----------------------------
void handleCommand(const std::string& cmd) {
    if (cmd == "ON") {
        stopMotor();
        currentState = 0;
        Serial.println("Command: ON -> motor stopped.");
    } else if (cmd == "OFF") {
        stopMotor();
        Serial.println("Command: OFF -> motor stopped.");
    } else if (cmd == "COLD") {
        stopMotor();
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        rampUpPWM("COLD");
        currentState = 1;
        Serial.println("Command: COLD -> motor running CW.");
    } else if (cmd == "HOT") {
        stopMotor();
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        rampUpPWM("HOT");
        currentState = 2;
        Serial.println("Command: HOT -> motor running CCW.");
    } else if (cmd == "GET") {
        // Send current temp
        int sensorValue = analogRead(ANALOG_PIN);
        float tempC = analogToTemp(sensorValue);
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f", tempC);
        pTxCharacteristic->setValue(buf);
        pTxCharacteristic->notify();
    }
}
