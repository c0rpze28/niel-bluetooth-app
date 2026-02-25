#include "TouchButton.h"

TouchButton::TouchButton(int pin, unsigned long longPressDuration)
    : _pin(pin), _longPressDuration(longPressDuration), _lastState(LOW), _active(false),
      _pressStartTime(0), _shortPressFlag(false), _longPressFlag(false) {
    pinMode(_pin, INPUT);
}

void TouchButton::update() {
    bool currentState = digitalRead(_pin);

    // Press started
    if (currentState == HIGH && _lastState == LOW) {
        _active = true;
        _pressStartTime = millis();
    }

    // Release occurred
    if (currentState == LOW && _lastState == HIGH && _active) {
        unsigned long pressDuration = millis() - _pressStartTime;
        if (pressDuration < _longPressDuration) {
            _shortPressFlag = true;
        }
        _active = false;
    }

    // Long press detection while still held
    if (_active && (millis() - _pressStartTime >= _longPressDuration)) {
        _longPressFlag = true;
        _active = false; // consume so we don't trigger again
    }

    _lastState = currentState;
}

bool TouchButton::isShortPress() {
    if (_shortPressFlag) {
        _shortPressFlag = false;
        return true;
    }
    return false;
}

bool TouchButton::isLongPress() {
    if (_longPressFlag) {
        _longPressFlag = false;
        return true;
    }
    return false;
}