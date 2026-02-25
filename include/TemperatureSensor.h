#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>

class TemperatureSensor {
public:
    // Default calibration values from original code
    TemperatureSensor(int pin, int adcCold = 1870, int adcHot = 3400,
                      float tempCold = 5.0, float tempHot = 66.0);
    float read(); // returns temperature in °C
private:
    int _pin;
    int _adcCold, _adcHot;
    float _tempCold, _tempHot;
};

#endif