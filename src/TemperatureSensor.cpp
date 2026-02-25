#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(int pin, int adcCold, int adcHot, float tempCold, float tempHot)
    : _pin(pin), _adcCold(adcCold), _adcHot(adcHot), _tempCold(tempCold), _tempHot(tempHot) {
    pinMode(_pin, INPUT);
}

float TemperatureSensor::read() {
    int adcValue = analogRead(_pin);
    adcValue = constrain(adcValue, 0, 4095);
    // Linear mapping (values are scaled by 10 to keep integer arithmetic)
    float temp = map(adcValue, _adcCold, _adcHot, _tempCold * 10, _tempHot * 10) / 10.0;
    return temp;
}