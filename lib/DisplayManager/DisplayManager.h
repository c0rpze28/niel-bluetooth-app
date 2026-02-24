#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
    DisplayManager(uint8_t width, uint8_t height, TwoWire* wire = &Wire);
    
    bool begin(uint8_t addr = 0x3C);
    void setRotation(uint8_t rotation);
    
    // Update methods for your specific application
    void showTemperatureMode(float temperature, 
                            TemperatureController::Mode mode,
                            float targetTemp,
                            bool outputActive);
    
    void showMessage(const char* line1, const char* line2 = nullptr);
    void showStatus(const char* status);
    
    // Low-level access
    Adafruit_SSD1306& getDisplay();

private:
    Adafruit_SSD1306 display;
    uint8_t screenWidth;
    uint8_t screenHeight;
    bool initialized;
    
    void drawHeader(const char* title);
    void clearArea(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
};