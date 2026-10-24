#include "relays.h"

RelayController& RelayController::instance() {
    static RelayController _inst;
    return _inst;
}

RelayController::RelayController() {}

void RelayController::rawWriteRelay(uint8_t pin, bool active) {
    digitalWrite(pin, active ? RELAY_ACTIVE_STATE : RELAY_INACTIVE_STATE);
}

void RelayController::allFanRelaysOff() {
    rawWriteRelay(PIN_FAN_SPEED_1, false);
    rawWriteRelay(PIN_FAN_SPEED_2, false);
    rawWriteRelay(PIN_FAN_SPEED_3, false);
}

void RelayController::init() {
    // Configure all relay pins as outputs and ensure they start de-energized
    pinMode(PIN_FAN_SPEED_1, OUTPUT);
    pinMode(PIN_FAN_SPEED_2, OUTPUT);
    pinMode(PIN_FAN_SPEED_3, OUTPUT);

    pinMode(PIN_VALVE_DANYLO, OUTPUT);
    pinMode(PIN_VALVE_ANASTASIIA, OUTPUT);
    pinMode(PIN_VALVE_PARENTS, OUTPUT);
    pinMode(PIN_VALVE_GIM, OUTPUT);

    allFanRelaysOff();
    rawWriteRelay(PIN_VALVE_DANYLO, false);
    rawWriteRelay(PIN_VALVE_ANASTASIIA, false);
    rawWriteRelay(PIN_VALVE_PARENTS, false);
    rawWriteRelay(PIN_VALVE_GIM, false);

    currentSpeed = FAN_OFF;
    valveDanylo = false;
    valveAnastasiia = false;
    valveParents = false;
    valveGim = false;

    Serial.println("[Relays] Hardware initialized. All relays OFF.");
}

void RelayController::setFanSpeed(FanSpeed speed) {
    if (speed == currentSpeed) return;

    Serial.printf("[Relays] Changing Fan Speed from %d to %d\n", currentSpeed, speed);

    // SAFETY INTERLOCK: Always turn all fan relays off first
    allFanRelaysOff();

    if (speed != FAN_OFF) {
        // Wait deadtime to prevent arcing / simultaneous coil engagement
        delay(FAN_SWITCH_DEADTIME_MS);

        switch (speed) {
            case FAN_SLOW:
                rawWriteRelay(PIN_FAN_SPEED_1, true);
                break;
            case FAN_MEDIUM:
                rawWriteRelay(PIN_FAN_SPEED_2, true);
                break;
            case FAN_HIGH:
                rawWriteRelay(PIN_FAN_SPEED_3, true);
                break;
            default:
                break;
        }
    }

    currentSpeed = speed;
}

FanSpeed RelayController::getFanSpeed() const {
    return currentSpeed;
}

void RelayController::setValve(const String& roomId, bool open) {
    if (roomId == "danylo") {
        setDanyloValve(open);
    } else if (roomId == "anastasiia") {
        setAnastasiiaValve(open);
    } else if (roomId == "parents") {
        setParentsValve(open);
    } else if (roomId == "gim") {
        setGimValve(open);
    }
}

bool RelayController::getValve(const String& roomId) const {
    if (roomId == "danylo") return isDanyloValveOpen();
    if (roomId == "anastasiia") return isAnastasiiaValveOpen();
    if (roomId == "parents") return isParentsValveOpen();
    if (roomId == "gim") return isGimValveOpen();
    return false;
}

void RelayController::setDanyloValve(bool open) {
    valveDanylo = open;
    rawWriteRelay(PIN_VALVE_DANYLO, open);
    Serial.printf("[Relays] Valve Danylo: %s\n", open ? "OPEN" : "CLOSED");
}

void RelayController::setAnastasiiaValve(bool open) {
    valveAnastasiia = open;
    rawWriteRelay(PIN_VALVE_ANASTASIIA, open);
    Serial.printf("[Relays] Valve Anastasiia: %s\n", open ? "OPEN" : "CLOSED");
}

void RelayController::setParentsValve(bool open) {
    valveParents = open;
    rawWriteRelay(PIN_VALVE_PARENTS, open);
    Serial.printf("[Relays] Valve Parents: %s\n", open ? "OPEN" : "CLOSED");
}

void RelayController::setGimValve(bool open) {
    valveGim = open;
    rawWriteRelay(PIN_VALVE_GIM, open);
    Serial.printf("[Relays] Valve Gim: %s\n", open ? "OPEN" : "CLOSED");
}

bool RelayController::isDanyloValveOpen() const {
    return valveDanylo;
}

bool RelayController::isAnastasiiaValveOpen() const {
    return valveAnastasiia;
}

bool RelayController::isParentsValveOpen() const {
    return valveParents;
}

bool RelayController::isGimValveOpen() const {
    return valveGim;
}
