#ifndef TOUCH_BUTTON_H
#define TOUCH_BUTTON_H

#include <Arduino.h>
#include <functional>

enum ButtonEvent {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_LONG_PRESS,
    BUTTON_CLICK
};

class TouchButton {
public:
    TouchButton(uint8_t pin, unsigned long longPressTime = 3000);
    
    void begin();
    void update();
    
    // Set callbacks for different events
    void onPressed(std::function<void()> callback);
    void onReleased(std::function<void()> callback);
    void onClick(std::function<void()> callback);
    void onLongPress(std::function<void()> callback);
    
    // Get state
    bool isPressed();
    bool isLongPressed();
    unsigned long getPressDuration();

private:
    uint8_t pin;
    unsigned long longPressThreshold;
    unsigned long pressStartTime;
    bool lastState;
    bool currentState;
    bool longPressTriggered;
    bool eventProcessed;
    
    std::function<void()> pressedCallback;
    std::function<void()> releasedCallback;
    std::function<void()> clickCallback;
    std::function<void()> longPressCallback;
};

#endif