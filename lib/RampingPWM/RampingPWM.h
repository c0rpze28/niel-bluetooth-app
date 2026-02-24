#ifndef RAMPING_PWM_H
#define RAMPING_PWM_H

#include <Arduino.h>

class RampingPWM {
public:
    RampingPWM(uint8_t pin, uint8_t channel = 0);
    
    // Configuration
    void begin(uint32_t frequency = 20000, uint8_t resolution = 8);
    void setRampTime(unsigned long rampTime);
    void setMaxDuty(uint8_t maxDuty);
    
    // Control
    void startRamp();
    void stop();
    void setDuty(uint8_t duty);
    
    // Update (call in loop)
    void update();
    
    // Status
    bool isRamping();
    uint8_t getCurrentDuty();

private:
    uint8_t pin;
    uint8_t pwmChannel;
    uint32_t pwmFreq;
    uint8_t pwmResolution;
    uint8_t maxDuty;
    
    unsigned long rampDuration;
    unsigned long rampStartTime;
    bool ramping;
    uint8_t currentDuty;
    bool initialized;
};