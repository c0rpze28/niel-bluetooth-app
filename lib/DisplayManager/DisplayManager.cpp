#include "DisplayManager.h"
#include "TemperatureController.h"  // Add this include
#include <Arduino.h>

DisplayManager::DisplayManager(uint8_t width, uint8_t height, TwoWire* wire) 
    : display(width, height, wire), screenWidth(width), screenHeight(height), 
      initialized(false) {}

bool DisplayManager::begin(uint8_t addr) {
    initialized = display.begin(SSD1306_SWITCHCAPVCC, addr);
    if (initialized) {
        display.clearDisplay();
        display.display();
    }
    return initialized;
}

void DisplayManager::setRotation(uint8_t rotation) {
    display.setRotation(rotation);
}

void DisplayManager::showTemperatureMode(float temperature, 
                                        int mode,  // Changed to int
                                        float targetTemp,
                                        bool outputActive) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    display.print("Mode: ");
    switch (mode) {
        case 1:  // HEATING
            display.println("HEAT");
            break;
        case 2:  // COOLING
            display.println("COOL");
            break;
        default:  // STANDBY
            display.println("STBY");
            break;
    }
    
    display.print("Temp: ");
    display.print(temperature, 1);
    display.println("C");
    
    display.print("Target: ");
    if (mode != 0) {  // Not STANDBY
        display.print(targetTemp, 1);
        display.println("C");
    } else {
        display.println("---");
    }
    
    display.print("Output: ");
    display.println(outputActive ? "ON" : "OFF");
    
    display.display();
}

void DisplayManager::showMessage(const char* line1, const char* line2) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    display.println(line1);
    if (line2) {
        display.println(line2);
    }
    
    display.display();
}

void DisplayManager::showStatus(const char* status) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(status);
    display.display();
}

Adafruit_SSD1306& DisplayManager::getDisplay() {
    return display;
}