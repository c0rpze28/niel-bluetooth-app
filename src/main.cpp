#include <Arduino.h>
#include <queue>
#include <mutex>

#include "PeltierController.h"
#include "TemperatureSensor.h"
#include "TouchButton.h"
#include "Display.h"
#include "BLEUart.h"

// Pin definitions (unchanged)
#define PWMA   5
#define AIN1   7
#define AIN2   6
#define STBY  10
#define TOUCH_PIN  2
#define NTC_PIN    1
#define SDA_PIN    4
#define SCL_PIN    3

// Global objects
PeltierController peltier(PWMA, AIN1, AIN2, STBY);
TemperatureSensor tempSensor(NTC_PIN);
TouchButton touchButton(TOUCH_PIN);
Display display(SDA_PIN, SCL_PIN);
BLEUart bleUart("ESP32-C3-NUS");  // matches Android app

// Thread-safe command queue for BLE
std::queue<std::string> commandQueue;
std::mutex commandMutex;

// Timing for periodic BLE updates
unsigned long lastBleSend = 0;
const unsigned long bleSendInterval = 1000; // 1 second

void setup() {
    Serial.begin(115200);
    delay(300);

    peltier.begin();
    if (!display.begin()) {
        Serial.println("Display init failed");
        while (1);
    }
    peltier.setMode(PeltierMode::STANDBY);

    // Initialize BLE
    bleUart.begin();
    bleUart.onReceive([](const std::string& data) {
        // Push incoming command to queue (thread-safe)
        std::lock_guard<std::mutex> lock(commandMutex);
        commandQueue.push(data);
    });
}

void loop() {
    // 1. Handle touch button
    touchButton.update();

    if (touchButton.isLongPress()) {
        if (peltier.getMode() == PeltierMode::STANDBY) {
            peltier.setMode(PeltierMode::COLD);
        } else {
            peltier.setMode(PeltierMode::STANDBY);
        }
    }

    if (touchButton.isShortPress()) {
        PeltierMode current = peltier.getMode();
        if (current == PeltierMode::COLD) {
            peltier.setMode(PeltierMode::HOT);
        } else if (current == PeltierMode::HOT) {
            peltier.setMode(PeltierMode::COLD);
        }
    }

    // 2. Read temperature
    float temperature = tempSensor.read();
    Serial.println(temperature);

    // 3. Update Peltier controller
    peltier.update(temperature);

    // 4. Process any pending BLE commands
    {
        std::lock_guard<std::mutex> lock(commandMutex);
        while (!commandQueue.empty()) {
            std::string cmd = commandQueue.front();
            commandQueue.pop();

            if (cmd.rfind("SETHOT ", 0) == 0) {
                float limit = atof(cmd.substr(7).c_str());
                peltier.setHotLimit(limit);
                bleUart.send("HOT SET " + std::to_string(limit));
            }
            else if (cmd.rfind("SETCOLD ", 0) == 0) {
                float limit = atof(cmd.substr(8).c_str());
                peltier.setColdLimit(limit);
                bleUart.send("COLD SET " + std::to_string(limit));
            }
            else if (cmd == "GET") {
                char tempStr[10];
                dtostrf(temperature, 4, 1, tempStr);
                bleUart.send(tempStr);
            }
        }
    }

    // 5. Periodic BLE temperature update (if client connected)
    if (bleUart.isConnected() && millis() - lastBleSend >= bleSendInterval) {
        lastBleSend = millis();
        char tempStr[10];
        dtostrf(temperature, 4, 1, tempStr);
        bleUart.send(tempStr);
    }

    // 6. Update display
    display.update(peltier.getMode(), temperature,
               peltier.isHeaterOn(), peltier.getRampProgress(),
               peltier.getHotLimit(), peltier.getColdLimit());

    delay(100);
}