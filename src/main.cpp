#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>

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

// Desired temperature thresholds (now modifiable via BLE)
float desiredCold = 20.0;
float desiredHot  = 50.0;

// Sensor calibration
#define TEMP_MIN 15.0
#define TEMP_MAX 60.0
#define ANALOG_MIN 1965
#define ANALOG_MAX 1470

// Temperature limits
#define MIN_COLD_LIMIT 18.0
#define MAX_HOT_LIMIT 65.0

// -----------------------------
// BLE UUIDs (NUS)
// -----------------------------
#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pRxCharacteristic = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;

// Preferences for storing temperature limits
Preferences preferences;

// -----------------------------
// Function declarations
// -----------------------------
void rampUpPWM(const char* stateName);
void stopMotor();
float analogToTemp(int sensorValue);
void setupBLE();
void handleCommand(const std::string& cmd);
void loadTemperatureLimits();
void saveTemperatureLimits();

// -----------------------------
// Setup
// -----------------------------
void setup() {
    Serial.begin(115200);

    // POWER SAVING: Reduce CPU frequency from 160MHz to 80MHz
    setCpuFrequencyMhz(80);
    Serial.println("CPU frequency set to 80MHz for power saving");

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(EN_PIN, PWM_CHANNEL);

    stopMotor();
    
    // Load saved temperature limits from flash
    loadTemperatureLimits();
    
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
    Serial.print("Limits - Cold: "); Serial.print(desiredCold, 1); 
    Serial.print("°C | Hot: "); Serial.print(desiredHot, 1); Serial.println("°C");

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
// Temperature limit management
// -----------------------------
void loadTemperatureLimits() {
    preferences.begin("thermostat", false);
    desiredCold = preferences.getFloat("coldLimit", 20.0);
    desiredHot = preferences.getFloat("hotLimit", 50.0);
    preferences.end();
    
    Serial.println("========================================");
    Serial.print("Loaded limits - Cold: ");
    Serial.print(desiredCold, 1);
    Serial.print("°C | Hot: ");
    Serial.print(desiredHot, 1);
    Serial.println("°C");
    Serial.println("========================================");
}

void saveTemperatureLimits() {
    preferences.begin("thermostat", false);
    preferences.putFloat("coldLimit", desiredCold);
    preferences.putFloat("hotLimit", desiredHot);
    preferences.end();
    
    Serial.println("Temperature limits saved to flash.");
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
        Serial.println("Advertising restarted - ready for reconnection");
    }
    
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
    
    // POWER SAVING: Reduce BLE transmit power
    // Options: ESP_PWR_LVL_N12, N9, N6, N3, N0, P3, P6, P9
    // N0 = 0dBm (good balance of range and power)
    // Default is P9 = +9dBm (maximum power, more heat)
    NimBLEDevice::setPower(ESP_PWR_LVL_N0);
    Serial.println("BLE power set to 0dBm for reduced heat");
    
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
        "2902",
        0,
        2,
        pTxCharacteristic
    );
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
    // Check for SETHOT command
    if (cmd.rfind("SETHOT ", 0) == 0) {
        std::string tempStr = cmd.substr(7); // Get everything after "SETHOT "
        float newHot = atof(tempStr.c_str());
        
        // Validate temperature
        if (newHot >= desiredCold + 1.0 && newHot <= MAX_HOT_LIMIT) {
            desiredHot = newHot;
            saveTemperatureLimits();
            
            Serial.println("========================================");
            Serial.print("Hot limit updated to: ");
            Serial.print(desiredHot, 1);
            Serial.println("°C");
            Serial.println("========================================");
        } else {
            Serial.println("========================================");
            Serial.println("ERROR: Invalid hot temperature limit!");
            Serial.print("Must be between ");
            Serial.print(desiredCold + 1.0, 1);
            Serial.print("°C and ");
            Serial.print(MAX_HOT_LIMIT, 1);
            Serial.println("°C");
            Serial.println("========================================");
        }
        return;
    }
    
    // Check for SETCOLD command
    if (cmd.rfind("SETCOLD ", 0) == 0) {
        std::string tempStr = cmd.substr(8); // Get everything after "SETCOLD "
        float newCold = atof(tempStr.c_str());
        
        // Validate temperature
        if (newCold >= MIN_COLD_LIMIT && newCold <= desiredHot - 1.0) {
            desiredCold = newCold;
            saveTemperatureLimits();
            
            Serial.println("========================================");
            Serial.print("Cold limit updated to: ");
            Serial.print(desiredCold, 1);
            Serial.println("°C");
            Serial.println("========================================");
        } else {
            Serial.println("========================================");
            Serial.println("ERROR: Invalid cold temperature limit!");
            Serial.print("Must be between ");
            Serial.print(MIN_COLD_LIMIT, 1);
            Serial.print("°C and ");
            Serial.print(desiredHot - 1.0, 1);
            Serial.println("°C");
            Serial.println("========================================");
        }
        return;
    }
    
    // Existing commands
    if (cmd == "ON") {
        stopMotor();
        currentState = 0;
        Serial.println("Command: ON -> motor stopped, auto mode enabled.");
    } else if (cmd == "OFF") {
        stopMotor();
        currentState = 0;
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
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", tempC);
        pTxCharacteristic->setValue(buf);
        pTxCharacteristic->notify();
        
        Serial.print("Sent temperature to app: ");
        Serial.print(tempC, 1);
        Serial.println("°C");
    } else {
        Serial.print("Unknown command received: ");
        Serial.println(cmd.c_str());
    }
}