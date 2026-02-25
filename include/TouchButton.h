#ifndef TOUCH_BUTTON_H
#define TOUCH_BUTTON_H

#include <Arduino.h>

class TouchButton {
public:
    TouchButton(int pin, unsigned long longPressDuration = 3000);
    void update();           // call frequently
    bool isShortPress();     // true once per short press (< longPressDuration)
    bool isLongPress();      // true once per long press (≥ longPressDuration)

private:
    int _pin;
    unsigned long _longPressDuration;
    bool _lastState;
    bool _active;
    unsigned long _pressStartTime;
    bool _shortPressFlag;
    bool _longPressFlag;
};

#endif