#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>

// ---------------- Pins ----------------
#define ANALOG_PIN 0
#define ANALOG_PIN2 2

#define IN1 21
#define IN2 20
#define EN_PIN 10

#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8
#define MAX_DUTY 255

// -------- Direction Threshold --------
#define COLD_THRESHOLD 3000
#define HOT_THRESHOLD 2200

// -------- Temperature Mapping --------
#define TEMP_MIN 15.0
#define TEMP_MAX 60.0
#define ANALOG_MIN 2930
#define ANALOG_MAX 1870

// -------- User Limits --------
float desiredColdTemp = 20.0;
float desiredHotTemp = 50.0;

// -------- State --------
int directionState = 0;
int lastDirection = 0;
bool pwmEnabled = false;

Preferences preferences;

// ---------------- BLE UUIDs ----------------
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pRxCharacteristic = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;

// ------------------------------------------------
// Temperature conversion
// ------------------------------------------------
float analogToTemp(int val) {
  val = constrain(val, ANALOG_MAX, ANALOG_MIN);
  return map(val, ANALOG_MIN, ANALOG_MAX, TEMP_MIN, TEMP_MAX);
}

// ------------------------------------------------
// Preferences
// ------------------------------------------------
void loadTemperatureLimits() {
  preferences.begin("thermostat", false);
  desiredColdTemp = preferences.getFloat("coldLimit", 20.0);
  desiredHotTemp = preferences.getFloat("hotLimit", 50.0);
  preferences.end();
}

void saveTemperatureLimits() {
  preferences.begin("thermostat", false);
  preferences.putFloat("coldLimit", desiredColdTemp);
  preferences.putFloat("hotLimit", desiredHotTemp);
  preferences.end();
}

// ------------------------------------------------
// PWM
// ------------------------------------------------
void pwmOff() {
  if (pwmEnabled) {
    ledcWrite(PWM_CHANNEL, 0);
    pwmEnabled = false;
    Serial.println("PWM OFF");
  }
}

void pwmOn() {
  if (!pwmEnabled) {
    ledcWrite(PWM_CHANNEL, 150);
    pwmEnabled = true;
    Serial.println("PWM ON");
  }
}

// ------------------------------------------------
// Ramp (ONLY on direction change)
// ------------------------------------------------
void rampUpPWM(const char* name) {

  delay(2000);

  for (int duty = 0; duty <= MAX_DUTY; duty++) {
    ledcWrite(PWM_CHANNEL, duty);
    delay(15000 / MAX_DUTY);
  }

  pwmEnabled = true;
}

// ------------------------------------------------
// Direction Control
// ------------------------------------------------
void setDirection(int dir) {

  if (dir == directionState) return;

  pwmOff();

  if (dir == 1) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    Serial.println("Direction = COLD");

    if (lastDirection == 2) rampUpPWM("COLD");
  }

  if (dir == 2) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    Serial.println("Direction = HOT");

    if (lastDirection == 1) rampUpPWM("HOT");
  }

  directionState = dir;
  lastDirection = dir;
}

// ------------------------------------------------
// BLE Command Handler
// ------------------------------------------------
void handleCommand(const std::string& cmd) {

  Serial.print("RAW COMMAND: ");
  Serial.println(cmd.c_str());

  if (cmd.rfind("SETCOLD ", 0) == 0) {
    desiredColdTemp = atof(cmd.substr(8).c_str());
    saveTemperatureLimits();
    Serial.println("Cold limit updated");
    return;
  }

  if (cmd.rfind("SETHOT ", 0) == 0) {
    desiredHotTemp = atof(cmd.substr(7).c_str());
    saveTemperatureLimits();
    Serial.println("Hot limit updated");
    return;
  }

  if (cmd == "GET") {
    float t = analogToTemp(analogRead(ANALOG_PIN2));
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", t);
    pTxCharacteristic->setValue(buf);
    pTxCharacteristic->notify();
  }
}

// ------------------------------------------------
// BLE Callbacks (UNCHANGED from yours)
// ------------------------------------------------
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
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
  }

  uint32_t onPassKeyRequest() {
    return 123456;
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) {
    if (!connInfo.isEncrypted()) {
      Serial.println("Encrypt failed — disconnecting");
      pServer->disconnect(connInfo.getConnHandle());
    }
  }
};

// ------------------------------------------------
// BLE Setup (ALL YOUR SETTINGS PRESERVED)
// ------------------------------------------------
void setupBLE() {

  NimBLEDevice::init("ESP32-C3-NUS");

  // Power saving + heat reduction
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);

  NimBLEDevice::deleteAllBonds();
  NimBLEDevice::setSecurityAuth(false, false, true);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  pServer->advertiseOnDisconnect(true);

  NimBLEService* pService = pServer->createService(NUS_SERVICE_UUID);

  pRxCharacteristic = pService->createCharacteristic(
    NUS_RX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE);
  pRxCharacteristic->setCallbacks(new RxCallbacks());

  pTxCharacteristic = pService->createCharacteristic(
    NUS_TX_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY);

  NimBLEDescriptor* cccd = new NimBLEDescriptor("2902", 0, 2, pTxCharacteristic);
  uint8_t cccd_value[2] = { 0x00, 0x00 };
  cccd->setValue(cccd_value, 2);
  pTxCharacteristic->addDescriptor(cccd);

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE NUS started.");
}

// ------------------------------------------------
// Setup
// ------------------------------------------------
void setup() {

  Serial.begin(115200);

  // Power saving CPU
  setCpuFrequencyMhz(80);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(EN_PIN, PWM_CHANNEL);

  loadTemperatureLimits();
  setupBLE();

  pwmOff();
}

// ------------------------------------------------
// Loop
// ------------------------------------------------
void loop() {

  int analog0 = analogRead(ANALOG_PIN);
  int analog2 = analogRead(ANALOG_PIN2);

  float temp = analogToTemp(analog2);

  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print(" | Analog0: ");
  Serial.print(analog0);
  Serial.print(" | Cold limit: ");
  Serial.print(desiredColdTemp);
  Serial.print(" | Hot limit: ");
  Serial.println(desiredHotTemp);  // ← FIXED: removed extra ()

  // Direction from Analog0 ONLY
  if (analog0 >= COLD_THRESHOLD) setDirection(1);
  else if (analog0 >= HOT_THRESHOLD) setDirection(2);

  // Temperature regulates PWM ONLY
  if (directionState == 1) {
    if (temp <= desiredColdTemp) pwmOff();
    else pwmOn();
  }

  if (directionState == 2) {
    if (temp >= desiredHotTemp) pwmOff();
    else pwmOn();
  }

  delay(300);
}