#ifndef PELTIER_CONTROLLER_H
#define PELTIER_CONTROLLER_H

#include <Arduino.h>

enum class PeltierMode {
    STANDBY,
    COLD,
    HOT
};

class PeltierController {
public:
    PeltierController(int pwmPin, int ain1Pin, int ain2Pin, int stbyPin, int pwmChannel = 0);
    void begin();
    void setMode(PeltierMode mode);
    void update(float currentTemperature);
    bool isHeaterOn() const;
    PeltierMode getMode() const;
    bool isRamping() const { return _ramping; }
    int getRampProgress() const;

    // New methods to adjust temperature limits
    void setHotLimit(float limit) { _hotLimit = limit; }
    void setColdLimit(float limit) { _coldLimit = limit; }
    float getHotLimit() const { return _hotLimit; }
    float getColdLimit() const { return _coldLimit; }

private:
    int _pwmPin, _ain1, _ain2, _stby;
    int _pwmChannel;
    PeltierMode _mode;
    bool _heaterOn;
    bool _ramping;
    unsigned long _rampStartTime;
    const unsigned long _rampTime = 10000; // ms
    const int _maxPWM = 255;

    // Adjustable temperature limits (defaults match original)
    float _hotLimit = 50.0;
    float _coldLimit = 15.0;
    const float _hysteresis = 1.0;

    void setDirectionForMode();
    void applyPWM(int value);
    void updateRamping(unsigned long now);
    void updateMaintenance(float temperature);
};

#endif