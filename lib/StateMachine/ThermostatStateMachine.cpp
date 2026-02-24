#include "ThermostatStateMachine.h"

ThermostatStateMachine::ThermostatStateMachine() 
    : currentState(STATE_STANDBY), previousState(STATE_STANDBY), 
      stateEntryTime(0) {}

void ThermostatStateMachine::requestStandby() {
    transitionTo(STATE_STANDBY);
}

void ThermostatStateMachine::requestCold() {
    if (currentState == STATE_STANDBY || currentState == STATE_HOT_MAINTAIN) {
        transitionTo(STATE_COLD_RAMP);
    }
}

void ThermostatStateMachine::requestHot() {
    if (currentState == STATE_STANDBY || currentState == STATE_COLD_MAINTAIN) {
        transitionTo(STATE_HOT_RAMP);
    }
}

void ThermostatStateMachine::rampComplete() {
    if (currentState == STATE_COLD_RAMP) {
        transitionTo(STATE_COLD_MAINTAIN);
    } else if (currentState == STATE_HOT_RAMP) {
        transitionTo(STATE_HOT_MAINTAIN);
    }
}

void ThermostatStateMachine::temperatureTargetReached() {
    // Can be used to trigger events if needed
}

ThermostatStateMachine::State ThermostatStateMachine::getState() {
    return currentState;
}

const char* ThermostatStateMachine::getStateName() {
    switch (currentState) {
        case STATE_STANDBY: return "STANDBY";
        case STATE_COLD_RAMP: return "COLD_RAMP";
        case STATE_COLD_MAINTAIN: return "COLD_MAINTAIN";
        case STATE_HOT_RAMP: return "HOT_RAMP";
        case STATE_HOT_MAINTAIN: return "HOT_MAINTAIN";
        default: return "UNKNOWN";
    }
}

void ThermostatStateMachine::onStateChange(std::function<void(State, State)> callback) {
    stateChangeCallback = callback;
}

void ThermostatStateMachine::onStandbyEntry(std::function<void()> callback) {
    standbyEntryCallback = callback;
}

void ThermostatStateMachine::onColdEntry(std::function<void()> callback) {
    coldEntryCallback = callback;
}

void ThermostatStateMachine::onHotEntry(std::function<void()> callback) {
    hotEntryCallback = callback;
}

void ThermostatStateMachine::update() {
    // State-specific update logic can go here
    // For now, just track time in state
    if (stateEntryTime == 0) {
        stateEntryTime = millis();
    }
}

void ThermostatStateMachine::transitionTo(State newState) {
    if (currentState == newState) return;
    
    previousState = currentState;
    currentState = newState;
    stateEntryTime = millis();
    
    // Call entry callbacks
    switch (newState) {
        case STATE_STANDBY:
            if (standbyEntryCallback) standbyEntryCallback();
            break;
        case STATE_COLD_RAMP:
        case STATE_COLD_MAINTAIN:
            if (coldEntryCallback) coldEntryCallback();
            break;
        case STATE_HOT_RAMP:
        case STATE_HOT_MAINTAIN:
            if (hotEntryCallback) hotEntryCallback();
            break;
    }
    
    // Notify state change
    if (stateChangeCallback) {
        stateChangeCallback(previousState, newState);
    }
}