#include "TemperatureController.h"

TemperatureController::TemperatureController() 
    : currentMode(STANDBY), hysteresis(1.0), heatingTarget(50.0), 
      coolingTarget(15.0), minValidTemp(0), maxValidTemp(100), 
      outputState(false) {}

void TemperatureController::setHysteresis(float hysteresis) {
    this->hysteresis = hysteresis;
}

void TemperatureController::setHeatingLimits(float targetTemp, float minTemp, float maxTemp) {
    heatingTarget = targetTemp;
    minValidTemp = minTemp;
    maxValidTemp = maxTemp;
}

void TemperatureController::setCoolingLimits(float targetTemp, float minTemp, float maxTemp) {
    coolingTarget = targetTemp;
    minValidTemp = minTemp;
    maxValidTemp = maxTemp;
}

TemperatureController::Mode TemperatureController::update(float currentTemperature) {
    if (currentTemperature < minValidTemp || currentTemperature > maxValidTemp) {
        // Temperature out of valid range - safety shutdown
        outputState = false;
        return STANDBY;
    }
    
    switch (currentMode) {
        case HEATING:
            updateHeating(currentTemperature);
            break;
        case COOLING:
            updateCooling(currentTemperature);
            break;
        case STANDBY:
        default:
            outputState = false;
            break;
    }
    
    return currentMode;
}

void TemperatureController::updateHeating(float temp) {
    if (outputState && temp >= heatingTarget + hysteresis) {
        outputState = false;
    } else if (!outputState && temp <= heatingTarget - hysteresis) {
        outputState = true;
    }
}

void TemperatureController::updateCooling(float temp) {
    if (outputState && temp <= coolingTarget - hysteresis) {
        outputState = false;
    } else if (!outputState && temp >= coolingTarget + hysteresis) {
        outputState = true;
    }
}

void TemperatureController::setMode(Mode mode) {
    currentMode = mode;
    outputState = false; // Reset output when changing modes
}

TemperatureController::Mode TemperatureController::getMode() {
    return currentMode;
}

bool TemperatureController::isOutputActive() {
    return outputState;
}

float TemperatureController::getTargetTemperature() {
    switch (currentMode) {
        case HEATING:
            return heatingTarget;
        case COOLING:
            return coolingTarget;
        default:
            return 0;
    }
}