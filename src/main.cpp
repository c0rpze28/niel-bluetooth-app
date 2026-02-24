#include <Wire.h>
#include "TouchButton.h"
#include "NTCThermistor.h"
#include "RampingPWM.h"
#include "TemperatureController.h"
#include "DisplayManager.h"
#include "ThermostatStateMachine.h"
#include "BLE_NUS_Service.h"
#include <Preferences.h>

// ---------------- PIN DEFINITIONS ----------------
#define PWMA_PIN 5
#define AIN1_PIN 7
#define AIN2_PIN 6
#define STBY_PIN 10
#define TOUCH_PIN 2
#define NTC_PIN 1
#define SDA_PIN 4
#define SCL_PIN 3

// Add display dimensions if not defined elsewhere
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 32
#endif

// ---------------- GLOBAL OBJECTS ----------------
TouchButton touchButton(TOUCH_PIN);
NTCThermistor thermistor(NTC_PIN);
RampingPWM pwm(PWMA_PIN);
TemperatureController tempController;
DisplayManager display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
ThermostatStateMachine stateMachine;
BLE_NUS_Service bleService;
Preferences preferences;

// ---------------- CONFIGURATION ----------------
const float HYSTERESIS = 1.0;
const unsigned long RAMP_TIME = 10000;
float coldLimit = 15.0;
float hotLimit = 50.0;

// ---------------- FORWARD DECLARATIONS ----------------
void setupPins();
void setupTemperatureSensor();
void loadLimits();
void saveLimits();
void handleBLECommand(const std::string& cmd);
void onStateChange(ThermostatStateMachine::State oldState, 
                   ThermostatStateMachine::State newState);

// ========== HELPER FUNCTIONS ==========

// State name helper function
const char* getStateName(ThermostatStateMachine::State state) {
    switch (state) {
        case ThermostatStateMachine::STATE_STANDBY:
            return "STANDBY";
        case ThermostatStateMachine::STATE_COLD_RAMP:
            return "COLD_RAMP";
        case ThermostatStateMachine::STATE_COLD_MAINTAIN:
            return "COLD_MAINTAIN";
        case ThermostatStateMachine::STATE_HOT_RAMP:
            return "HOT_RAMP";
        case ThermostatStateMachine::STATE_HOT_MAINTAIN:
            return "HOT_MAINTAIN";
        default:
            return "UNKNOWN";
    }
}

// ========== IMPLEMENTATIONS ==========

void handleBLECommand(const std::string& cmd) {
    Serial.print("BLE Command received: ");
    Serial.println(cmd.c_str());

    // Parse temperature reading commands
    if (cmd == "GET" || cmd == "GET_TEMP") {
        float temp = thermistor.readCelsius();
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.1f", temp);
        bleService.sendData(buffer);
    }
    
    // Set hot limit (format: "SETHOT 50.0" or "SET_HOT_LIMIT 50.0")
    else if (cmd.rfind("SETHOT ", 0) == 0 || cmd.rfind("SET_HOT_LIMIT ", 0) == 0) {
        float newLimit = atof(cmd.substr(cmd.find(' ') + 1).c_str());
        if (newLimit >= 30 && newLimit <= 70) {
            hotLimit = newLimit;
            saveLimits();
            tempController.setHeatingLimits(hotLimit);
            bleService.sendData("HOT_LIMIT_OK");
        } else {
            bleService.sendData("ERROR: Invalid range (30-70)");
        }
    }
    
    // Set cold limit (format: "SETCOLD 15.0" or "SET_COLD_LIMIT 15.0")
    else if (cmd.rfind("SETCOLD ", 0) == 0 || cmd.rfind("SET_COLD_LIMIT ", 0) == 0) {
        float newLimit = atof(cmd.substr(cmd.find(' ') + 1).c_str());
        if (newLimit >= 0 && newLimit <= 30) {
            coldLimit = newLimit;
            saveLimits();
            tempController.setCoolingLimits(coldLimit);
            bleService.sendData("COLD_LIMIT_OK");
        } else if (cmd == "GET_STATUS") {
            float temp = thermistor.readCelsius();
            char buffer[64];
            snprintf(buffer, sizeof(buffer), 
            "T:%.1f,M:%s,S:%s,C:%.1f,H:%.1f",
            temp,
            (tempController.getMode() == TemperatureController::HEATING) ? "H" :
            (tempController.getMode() == TemperatureController::COOLING) ? "C" : "S",
            getStateName(stateMachine.getState()),
            coldLimit,
            hotLimit);
            bleService.sendData(buffer);
        } else {
            bleService.sendData("ERROR: Invalid range (0-30)");
        }
    }
    
    // Mode switching commands
    else if (cmd == "SET_STANDBY" || cmd == "STANDBY") {
        stateMachine.requestStandby();
        bleService.sendData("MODE:STANDBY");
    }
    else if (cmd == "SET_COLD" || cmd == "COLD") {
        stateMachine.requestCold();
        bleService.sendData("MODE:COLD");
    }
    else if (cmd == "SET_HOT" || cmd == "HOT") {
        stateMachine.requestHot();
        bleService.sendData("MODE:HOT");
    }
    
    // Status requests
    else if (cmd == "GET_STATUS") {
        float temp = thermistor.readCelsius();
        char buffer[64];
        snprintf(buffer, sizeof(buffer), 
                 "T:%.1f,M:%s,S:%s,C:%.1f,H:%.1f",
                 temp,
                 (tempController.getMode() == TemperatureController::HEATING) ? "H" :
                 (tempController.getMode() == TemperatureController::COOLING) ? "C" : "S",
                 getStateName(stateMachine.getState()),  // Use helper function
                 coldLimit,
                 hotLimit);
        bleService.sendData(buffer);
    }
    
    // Get current limits
    else if (cmd == "GET_LIMITS") {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "COLD:%.1f,HOT:%.1f", coldLimit, hotLimit);
        bleService.sendData(buffer);
    }
    
    // Get current mode
    else if (cmd == "GET_MODE") {
        const char* modeStr = (tempController.getMode() == TemperatureController::HEATING) ? "HOT" :
                              (tempController.getMode() == TemperatureController::COOLING) ? "COLD" : "STANDBY";
        bleService.sendData(modeStr);
    }
    
    // Help command
    else if (cmd == "HELP") {
        bleService.sendData("Commands: GET, SETHOT [30-70], SETCOLD [0-30], SET_STANDBY, SET_HOT, SET_COLD, GET_STATUS, GET_LIMITS, GET_MODE");
    }
    
    // Unknown command
    else {
        bleService.sendData("UNKNOWN");
    }
}

// ONLY ONE definition of onStateChange - this is the CORRECT one
void onStateChange(ThermostatStateMachine::State oldState, 
                   ThermostatStateMachine::State newState) {
    Serial.printf("State change: %s -> %s\n", 
                  getStateName(oldState),
                  getStateName(newState));
    
    if (bleService.isConnected()) {
        bleService.sendData(getStateName(newState));
    }
}

void setupPins() {
    pinMode(AIN1_PIN, OUTPUT);
    pinMode(AIN2_PIN, OUTPUT);
    pinMode(STBY_PIN, OUTPUT);
    
    digitalWrite(STBY_PIN, LOW);
}

void setupTemperatureSensor() {
    thermistor.setADCResolution(4095);
    thermistor.setCalibration(1870, 5.0, 3400, 66.0);
}

void loadLimits() {
    preferences.begin("thermostat", false);
    coldLimit = preferences.getFloat("coldLimit", 15.0);
    hotLimit = preferences.getFloat("hotLimit", 50.0);
    preferences.end();
    
    tempController.setHeatingLimits(hotLimit);
    tempController.setCoolingLimits(coldLimit);
}

void saveLimits() {
    preferences.begin("thermostat", false);
    preferences.putFloat("coldLimit", coldLimit);
    preferences.putFloat("hotLimit", hotLimit);
    preferences.end();
}

// ---------------- SETUP ----------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== Temperature Controller Starting ===");
    
    // Initialize I2C
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    
    // Initialize display
    if (!display.begin()) {
        Serial.println("Display init failed!");
    }
    display.setRotation(1);
    
    // Initialize pins
    setupPins();
    
    // Initialize temperature sensor
    setupTemperatureSensor();
    
    // Initialize PWM
    pwm.begin();
    pwm.setRampTime(RAMP_TIME);
    
    // Load saved preferences
    loadLimits();
    
    // Configure temperature controller
    tempController.setHysteresis(HYSTERESIS);
    tempController.setHeatingLimits(hotLimit);
    tempController.setCoolingLimits(coldLimit);
    
    // Setup state machine callbacks
    stateMachine.onStateChange(onStateChange);
    stateMachine.onStandbyEntry([]() {
        digitalWrite(STBY_PIN, LOW);
        pwm.stop();
        display.showMessage("Standby", "Mode");
    });
    stateMachine.onColdEntry([]() {
        digitalWrite(STBY_PIN, HIGH);
        digitalWrite(AIN1_PIN, HIGH);
        digitalWrite(AIN2_PIN, LOW);
        pwm.startRamp();
        display.showMessage("Cooling", "Mode");
    });
    stateMachine.onHotEntry([]() {
        digitalWrite(STBY_PIN, HIGH);
        digitalWrite(AIN1_PIN, LOW);
        digitalWrite(AIN2_PIN, HIGH);
        pwm.startRamp();
        display.showMessage("Heating", "Mode");
    });
    
    // Initialize BLE
    bleService.begin("ESP32-TempCtrl", handleBLECommand);
    
    // Start in standby
    stateMachine.requestStandby();
    
    Serial.println("Setup complete");
}

// ---------------- LOOP ----------------
void loop() {
    // Update all components
    touchButton.update();
    pwm.update();
    stateMachine.update();
    
    // Read temperature
    float temperature = thermistor.readCelsius();
    
    // Update temperature controller
    auto controllerMode = tempController.update(temperature);
    
    // Update display
    display.showTemperatureMode(temperature, 
                                static_cast<int>(controllerMode),
                                tempController.getTargetTemperature(),
                                tempController.isOutputActive());
    
    // Handle BLE status updates
    static unsigned long lastBLETime = 0;
    if (bleService.isConnected() && millis() - lastBLETime > 5000) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "TEMP:%.1f", temperature);
        bleService.sendData(buffer);
        lastBLETime = millis();
    }
    
    // Serial debug
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 2000) {
        Serial.printf("Temp: %.1f | Mode: %s | State: %s | BLE: %s\n",
                     temperature,
                     (controllerMode == TemperatureController::STANDBY) ? "STBY" :
                     (controllerMode == TemperatureController::HEATING) ? "HEAT" : "COOL",
                     getStateName(stateMachine.getState()),  // Use helper function
                     bleService.isConnected() ? "CONN" : "DISCONN");
        lastDebugTime = millis();
    }
    
    delay(100);
}