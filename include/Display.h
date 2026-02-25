#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "PeltierController.h"

class Display {
public:
    Display(int sdaPin, int sclPin, int width = 128, int height = 32);
    bool begin();
    void update(PeltierMode mode, float temperature, bool heaterOn, int rampProgress, float hotLimit, float coldLimit);

private:
    int _sda, _scl;
    int _width, _height;
    Adafruit_SSD1306 _display;
};

#endif