#pragma once

#include <Arduino.h>
#include "config.h"
#include "types.h"

class RelayController {
public:
    static RelayController& instance();

    void init();
    void setFanSpeed(FanSpeed speed);
    FanSpeed getFanSpeed() const;

    void setValve(const String& roomId, bool open);
    bool getValve(const String& roomId) const;

    // Direct named valve helpers
    void setDanyloValve(bool open);
    void setAnastasiiaValve(bool open);
    void setParentsValve(bool open);
    void setGimValve(bool open);

    bool isDanyloValveOpen() const;
    bool isAnastasiiaValveOpen() const;
    bool isParentsValveOpen() const;
    bool isGimValveOpen() const;

private:
    RelayController();
    FanSpeed currentSpeed = FAN_OFF;
    bool valveDanylo = false;
    bool valveAnastasiia = false;
    bool valveParents = false;
    bool valveGim = false;

    void rawWriteRelay(uint8_t pin, bool active);
    void allFanRelaysOff();
};
