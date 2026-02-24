#include "NTCThermistor.h"

NTCThermistor::NTCThermistor(uint8_t adcPin) : pin(adcPin), adcMax(4095) {
    pinMode(pin, INPUT);
}

void NTCThermistor::setCalibration(int adcLow, float tempLow, int adcHigh, float tempHigh) {
    this->adcLow = adcLow;
    this->adcHigh = adcHigh;
    this->tempLow = tempLow;
    this->tempHigh = tempHigh;
}

void NTCThermistor::setCalibrationPoints(const CalibrationPoint& point1, const CalibrationPoint& point2) {
    adcLow = point1.adcValue;
    tempLow = point1.temperature;
    adcHigh = point2.adcValue;
    tempHigh = point2.temperature;
}

float NTCThermistor::readCelsius() {
    int raw = readRaw();
    return mapValue(raw);
}

float NTCThermistor::readFahrenheit() {
    return (readCelsius() * 9.0 / 5.0) + 32.0;
}

int NTCThermistor::readRaw() {
    return analogRead(pin);
}

void NTCThermistor::setADCResolution(int maxValue) {
    adcMax = maxValue;
}

float NTCThermistor::mapValue(int value) {
    // Constrain to calibration range
    if (value < min(adcLow, adcHigh)) value = min(adcLow, adcHigh);
    if (value > max(adcLow, adcHigh)) value = max(adcLow, adcHigh);
    
    // Linear interpolation
    return tempLow + (float)(value - adcLow) * (tempHigh - tempLow) / (adcHigh - adcLow);
}