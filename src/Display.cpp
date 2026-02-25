#include "Display.h"
#include <stdio.h>

// Icon bitmaps (unchanged)
static const uint8_t PROGMEM standby_bmp[] = {
    0b00000111, 0b11100000,
    0b00011111, 0b11111000,
    0b00111111, 0b11111100,
    0b01111100, 0b00111110,
    0b01110000, 0b00001110,
    0b11100000, 0b00000111,
    0b11100000, 0b00000111,
    0b11000000, 0b00000011,
    0b11000001, 0b10000011,
    0b11000001, 0b10000011,
    0b11100001, 0b10000111,
    0b11100000, 0b00000111,
    0b01110000, 0b00001110,
    0b01111100, 0b00111110,
    0b00111111, 0b11111100,
    0b00011111, 0b11111000
};

static const uint8_t PROGMEM cold_bmp[] = {
    0b00000011, 0b11000000,
    0b00000111, 0b11100000,
    0b00001111, 0b11110000,
    0b00011111, 0b11111000,
    0b00111111, 0b11111100,
    0b00111111, 0b11111100,
    0b01111111, 0b11111110,
    0b01111111, 0b11111110,
    0b01111111, 0b11111110,
    0b00111111, 0b11111100,
    0b00111111, 0b11111100,
    0b00011111, 0b11111000,
    0b00001111, 0b11110000,
    0b00000111, 0b11100000,
    0b00000011, 0b11000000,
    0b00000001, 0b10000000
};

static const uint8_t PROGMEM hot_bmp[] = {
    0b00000000, 0b00000000,
    0b00000001, 0b10000000,
    0b00000011, 0b11000000,
    0b00000111, 0b11100000,
    0b00001111, 0b11110000,
    0b00011111, 0b11111000,
    0b00111111, 0b11111100,
    0b01111111, 0b11111110,
    0b01111111, 0b11111110,
    0b00111111, 0b11111100,
    0b00011111, 0b11111000,
    0b00001111, 0b11110000,
    0b00000111, 0b11100000,
    0b00000011, 0b11000000,
    0b00000001, 0b10000000,
    0b00000000, 0b00000000
};

Display::Display(int sdaPin, int sclPin, int width, int height)
    : _sda(sdaPin), _scl(sclPin), _width(width), _height(height),
      _display(width, height, &Wire, -1) {}

bool Display::begin() {
    Wire.begin(_sda, _scl);
    Wire.setClock(100000);
    if (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        return false;
    }
    _display.clearDisplay();
    _display.display();
    delay(100);
    _display.setRotation(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    return true;
}

void Display::update(PeltierMode mode, float temperature, bool heaterOn, int rampProgress, float hotLimit, float coldLimit) {
    _display.clearDisplay();

    // Draw centered icon (16px wide, at x = 8)
    const uint8_t* icon = nullptr;
    switch (mode) {
        case PeltierMode::STANDBY: icon = standby_bmp; break;
        case PeltierMode::COLD:    icon = cold_bmp;    break;
        case PeltierMode::HOT:     icon = hot_bmp;     break;
    }
    if (icon) {
        _display.drawBitmap(8, 0, icon, 16, 16, SSD1306_WHITE);
    }

    // Temperature label
    _display.setCursor(0, 16);
    _display.print("TEMP");

    // Temperature value
    _display.setCursor(0, 24);
    char tempStr[6];
    dtostrf(temperature, 4, 1, tempStr);
    _display.print(tempStr);
    _display.print("C");

    // Setpoint label
    _display.setCursor(0, 32);
    _display.print("SET");

    // Setpoint value (use the appropriate limit based on mode)
    _display.setCursor(0, 40);
    if (mode == PeltierMode::HOT) {
        char setStr[6];
        dtostrf(hotLimit, 4, 1, setStr);
        _display.print(setStr);
        _display.print("C");
    } else if (mode == PeltierMode::COLD) {
        char setStr[6];
        dtostrf(coldLimit, 4, 1, setStr);
        _display.print(setStr);
        _display.print("C");
    } else {
        _display.print("---");
    }

    // Status label
    _display.setCursor(0, 48);
    _display.print("STAT");

    // Status value
    _display.setCursor(0, 56);
    if (mode == PeltierMode::STANDBY) {
        _display.print("OFF");
    } else if (heaterOn) {
        _display.print(mode == PeltierMode::HOT ? "HEAT" : "COOL");
    } else {
        _display.print("IDLE");
    }

    // Ramp progress bar (if ramping)
    if (rampProgress > 0 && rampProgress < 100) {
        int barWidth = map(rampProgress, 0, 100, 0, 32);
        _display.fillRect(0, 64, barWidth, 8, SSD1306_WHITE);
        _display.drawRect(0, 64, 32, 8, SSD1306_WHITE);  // outline
    } else if (rampProgress >= 100) {
        // Full bar
        _display.fillRect(0, 64, 32, 8, SSD1306_WHITE);
    }

    _display.display();
}