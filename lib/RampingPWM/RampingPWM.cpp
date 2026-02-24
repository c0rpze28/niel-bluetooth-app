#include "RampingPWM.h"

RampingPWM::RampingPWM(uint8_t pin, uint8_t channel) 
    : pin(pin), pwmChannel(channel), pwmFreq(20000), pwmResolution(8),
      maxDuty(255), rampDuration(10000), ramping(false), currentDuty(0), 
      initialized(false) {}

void RampingPWM::begin(uint32_t frequency, uint8_t resolution) {
    pwmFreq = frequency;
    pwmResolution = resolution;
    maxDuty = (1 << resolution) - 1;
    
    ledcSetup(pwmChannel, pwmFreq, pwmResolution);
    ledcAttachPin(pin, pwmChannel);
    initialized = true;
}

void RampingPWM::setRampTime(unsigned long rampTime) {
    rampDuration = rampTime;
}

void RampingPWM::setMaxDuty(uint8_t duty) {
    maxDuty = duty;
}

void RampingPWM::startRamp() {
    if (!initialized) return;
    rampStartTime = millis();
    ramping = true;
}

void RampingPWM::stop() {
    ledcWrite(pwmChannel, 0);
    ramping = false;
    currentDuty = 0;
}

void RampingPWM::setDuty(uint8_t duty) {
    if (!initialized) return;
    currentDuty = duty;
    ledcWrite(pwmChannel, duty);
}

void RampingPWM::update() {
    if (!initialized || !ramping) return;
    
    unsigned long elapsed = millis() - rampStartTime;
    if (elapsed >= rampDuration) {
        currentDuty = maxDuty;
        ramping = false;
    } else {
        currentDuty = map(elapsed, 0, rampDuration, 0, maxDuty);
    }
    
    ledcWrite(pwmChannel, currentDuty);
}

bool RampingPWM::isRamping() {
    return ramping;
}

uint8_t RampingPWM::getCurrentDuty() {
    return currentDuty;
}