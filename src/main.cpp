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
    
    // Control output based on controller recommendation
    if (stateMachine.getState() != ThermostatStateMachine::STATE_STANDBY) {
        if (controllerMode == TemperatureController::HEATING || 
            controllerMode == TemperatureController::COOLING) {
            // PWM is already handling the output via ramping
            // Just ensure direction is correct
        }
    }
    
    // Update display
    display.showTemperatureMode(temperature, 
                                controllerMode,
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
                     stateMachine.getStateName(),
                     bleService.isConnected() ? "CONN" : "DISCONN");
        lastDebugTime = millis();
    }
    
    delay(100);
}

// ---------------- IMPLEMENTATIONS ----------------
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

void handleBLECommand(const std::string& cmd) {
    // Similar command handling as before...
}

void onStateChange(ThermostatStateMachine::State oldState, 
                   ThermostatStateMachine::State newState) {
    Serial.printf("State change: %s -> %s\n", 
                  stateMachine.getStateName(),
                  stateMachine.getStateName());
    
    if (bleService.isConnected()) {
        bleService.sendData(stateMachine.getStateName());
    }
}