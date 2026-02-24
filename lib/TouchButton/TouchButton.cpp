#include "TouchButton.h"

TouchButton::TouchButton(uint8_t pin, unsigned long longPressTime) 
    : pin(pin), longPressThreshold(longPressTime), pressStartTime(0),
      lastState(HIGH), currentState(HIGH), longPressTriggered(false), 
      eventProcessed(false) {
    pinMode(pin, INPUT_PULLUP); // or INPUT based on your hardware
}

void TouchButton::begin() {
    // Any additional initialization
}

void TouchButton::update() {
    currentState = digitalRead(pin);
    
    if (currentState == LOW && lastState == HIGH) {
        // Button pressed
        pressStartTime = millis();
        longPressTriggered = false;
        eventProcessed = false;
        if (pressedCallback) pressedCallback();
    }
    else if (currentState == HIGH && lastState == LOW) {
        // Button released
        unsigned long pressDuration = millis() - pressStartTime;
        if (pressDuration < longPressThreshold && !eventProcessed) {
            eventProcessed = true;
            if (clickCallback) clickCallback();
        }
        if (releasedCallback) releasedCallback();
    }
    
    // Check for long press
    if (currentState == LOW && !longPressTriggered) {
        unsigned long pressDuration = millis() - pressStartTime;
        if (pressDuration >= longPressThreshold && !eventProcessed) {
            longPressTriggered = true;
            eventProcessed = true;
            if (longPressCallback) longPressCallback();
        }
    }
    
    lastState = currentState;
}

bool TouchButton::isPressed() {
    return currentState == LOW;
}

bool TouchButton::isLongPressed() {
    return isPressed() && (millis() - pressStartTime >= longPressThreshold);
}

unsigned long TouchButton::getPressDuration() {
    return isPressed() ? (millis() - pressStartTime) : 0;
}

void TouchButton::onPressed(std::function<void()> callback) {
    pressedCallback = callback;
}

void TouchButton::onReleased(std::function<void()> callback) {
    releasedCallback = callback;
}

void TouchButton::onClick(std::function<void()> callback) {
    clickCallback = callback;
}

void TouchButton::onLongPress(std::function<void()> callback) {
    longPressCallback = callback;
}