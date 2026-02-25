#include "PeltierController.h"

PeltierController::PeltierController(int pwmPin, int ain1Pin, int ain2Pin, int stbyPin, int pwmChannel)
    : _pwmPin(pwmPin), _ain1(ain1Pin), _ain2(ain2Pin), _stby(stbyPin), _pwmChannel(pwmChannel),
      _mode(PeltierMode::STANDBY), _heaterOn(false), _ramping(false), _rampStartTime(0) {}

void PeltierController::begin() {
    pinMode(_ain1, OUTPUT);
    pinMode(_ain2, OUTPUT);
    pinMode(_stby, OUTPUT);
    digitalWrite(_stby, LOW);

    ledcSetup(_pwmChannel, 20000, 8);
    ledcAttachPin(_pwmPin, _pwmChannel);
    ledcWrite(_pwmChannel, 0);
}

void PeltierController::setMode(PeltierMode mode) {
    if (_mode == mode) return;
    _mode = mode;
    _ramping = true;
    _rampStartTime = millis();
    _heaterOn = true;

    if (mode != PeltierMode::STANDBY) {
        digitalWrite(_stby, HIGH);
    } else {
        digitalWrite(_stby, LOW);
        _heaterOn = false;
        _ramping = false;
        ledcWrite(_pwmChannel, 0);
    }
}

void PeltierController::update(float temperature) {
    unsigned long now = millis();
    if (_ramping) {
        updateRamping(now);
    } else {
        updateMaintenance(temperature);
    }
    if (_mode != PeltierMode::STANDBY) {
        setDirectionForMode();
    }
}

void PeltierController::updateRamping(unsigned long now) {
    unsigned long elapsed = now - _rampStartTime;
    int pwmValue;
    if (elapsed >= _rampTime) {
        pwmValue = _maxPWM;
        _ramping = false;
    } else {
        pwmValue = map(elapsed, 0, _rampTime, 0, _maxPWM);
    }
    applyPWM(pwmValue);
    _heaterOn = true;
}

void PeltierController::updateMaintenance(float temperature) {
    if (_mode == PeltierMode::HOT) {
        if (_heaterOn && temperature >= _hotLimit + _hysteresis) {
            _heaterOn = false;
        } else if (!_heaterOn && temperature <= _hotLimit - _hysteresis) {
            _heaterOn = true;
        }
    } else if (_mode == PeltierMode::COLD) {
        if (_heaterOn && temperature <= _coldLimit - _hysteresis) {
            _heaterOn = false;
        } else if (!_heaterOn && temperature >= _coldLimit + _hysteresis) {
            _heaterOn = true;
        }
    } else { // STANDBY
        _heaterOn = false;
    }
    applyPWM(_heaterOn ? _maxPWM : 0);
}

void PeltierController::setDirectionForMode() {
    if (_mode == PeltierMode::COLD) {
        digitalWrite(_ain1, HIGH);
        digitalWrite(_ain2, LOW);
    } else if (_mode == PeltierMode::HOT) {
        digitalWrite(_ain1, LOW);
        digitalWrite(_ain2, HIGH);
    }
}

void PeltierController::applyPWM(int value) {
    ledcWrite(_pwmChannel, value);
}

bool PeltierController::isHeaterOn() const {
    return _heaterOn;
}

PeltierMode PeltierController::getMode() const {
    return _mode;
}

int PeltierController::getRampProgress() const {
    if (!_ramping) return 0;
    unsigned long elapsed = millis() - _rampStartTime;
    if (elapsed >= _rampTime) return 100;
    return (elapsed * 100) / _rampTime;
}