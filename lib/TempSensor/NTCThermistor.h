#ifndef NTC_THERMISTOR_H
#define NTC_THERMISTOR_H

#include <Arduino.h>

class NTCThermistor {
public:
    struct CalibrationPoint {
        int adcValue;
        float temperature;
    };

    NTCThermistor(uint8_t adcPin);
    
    // Set calibration points
    void setCalibration(int adcLow, float tempLow, int adcHigh, float tempHigh);
    void setCalibrationPoints(const CalibrationPoint& point1, const CalibrationPoint& point2);
    
    // Read temperature
    float readCelsius();
    float readFahrenheit();
    
    // Get raw ADC value
    int readRaw();
    
    // Set ADC resolution (e.g., 4095 for 12-bit)
    void setADCResolution(int maxValue);

private:
    uint8_t pin;
    int adcMax;
    int adcLow, adcHigh;
    float tempLow, tempHigh;
    
    float mapValue(int value);
};