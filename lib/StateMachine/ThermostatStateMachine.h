#ifndef THERMOSTAT_STATE_MACHINE_H
#define THERMOSTAT_STATE_MACHINE_H

#include <Arduino.h>
#include <functional>

class ThermostatStateMachine {
public:
    enum State {
        STATE_STANDBY,
        STATE_COLD_RAMP,
        STATE_COLD_MAINTAIN,
        STATE_HOT_RAMP,
        STATE_HOT_MAINTAIN
    };
    
    ThermostatStateMachine();
    
    // Events
    void requestStandby();
    void requestCold();
    void requestHot();
    void rampComplete();
    void temperatureTargetReached();
    
    // Get current state
    State getState();
    const char* getStateName();
    
    // State change callbacks
    void onStateChange(std::function<void(State, State)> callback);
    void onStandbyEntry(std::function<void()> callback);
    void onColdEntry(std::function<void()> callback);
    void onHotEntry(std::function<void()> callback);
    
    // Update (call in loop)
    void update();

private:
    State currentState;
    State previousState;
    unsigned long stateEntryTime;
    
    std::function<void(State, State)> stateChangeCallback;
    std::function<void()> standbyEntryCallback;
    std::function<void()> coldEntryCallback;
    std::function<void()> hotEntryCallback;
    
    void transitionTo(State newState);
};