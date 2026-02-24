#ifndef TEMPERATURE_CONTROLLER_H
#define TEMPERATURE_CONTROLLER_H

#include <Arduino.h>

class TemperatureController {
public:
    enum Mode { STANDBY, HEATING, COOLING };
    
    TemperatureController();
    
    // Configuration
    void setHysteresis(float hysteresis);
    void setHeatingLimits(float targetTemp, float minTemp = 0, float maxTemp = 100);
    void setCoolingLimits(float targetTemp, float minTemp = 0, float maxTemp = 100);
    
    // Update with current temperature
    Mode update(float currentTemperature);
    
    // Control
    void setMode(Mode mode);
    Mode getMode();
    
    // Get output state
    bool isOutputActive();
    
    // Get targets
    float getTargetTemperature();

private:
    Mode currentMode;
    float hysteresis;
    float heatingTarget;
    float coolingTarget;
    float minValidTemp;
    float maxValidTemp;
    bool outputState;
    
    void updateHeating(float temp);
    void updateCooling(float temp);
};