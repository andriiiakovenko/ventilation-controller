#pragma once

#include <Arduino.h>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include "types.h"
#include "config.h"
#include "relays.h"

typedef std::function<void()> StateChangeCallback;

class VentilationController {
public:
    static VentilationController& instance();

    void init();
    void update(); // Main periodic logic loop

    // State getters & setters
    VentilationMode getMode() const;
    void setMode(VentilationMode mode);

    FanSpeed getFanSpeed() const;
    void setFanSpeed(FanSpeed speed);

    bool getValveState(const String& roomId) const;
    void setValveState(const String& roomId, bool open);

    // Sensor processing
    void onSensorPacket(const ESPNowSensorPacket& packet, const uint8_t* mac);
    // NOTE: getRooms() returns a reference that is NOT thread-safe. Use
    // getRoomsSnapshot() from any task other than the Arduino loop task
    // (e.g. AsyncWebServer handlers running in the async_tcp task).
    const std::map<String, RoomSensorData>& getRooms() const;
    std::map<String, RoomSensorData> getRoomsSnapshot() const;
    const RoomSensorData* getRoom(const String& roomId) const;

    // Callbacks
    void registerStateChangeCallback(StateChangeCallback cb);

    // Thresholds
    float getHumidityHighThreshold() const { return humidityHighThreshold; }
    void setHumidityHighThreshold(float val) { humidityHighThreshold = val; }

    float getHumidityTargetThreshold() const { return humidityTargetThreshold; }
    void setHumidityTargetThreshold(float val) { humidityTargetThreshold = val; }

    bool isAnyValveOpen() const;

private:
    VentilationController();
    void runAutonomousLogic();
    void notifyChange();

    VentilationMode currentMode = MODE_MANUAL;
    std::map<String, RoomSensorData> rooms;
    std::vector<StateChangeCallback> callbacks;

    // Guards controller state: it is accessed from the loop task (ESP-NOW
    // packets, periodic logic, MQTT) and the async_tcp task (web API).
    mutable std::recursive_mutex stateMutex;

    FanSpeed pendingFanSpeed = FAN_OFF;
    unsigned long fanTurnOnAt = 0;

    float humidityHighThreshold = DEFAULT_HUMIDITY_HIGH_THRESHOLD;
    float humidityTargetThreshold = DEFAULT_HUMIDITY_TARGET_THRESHOLD;

    unsigned long lastLogicRun = 0;
    unsigned long lastStatusCheck = 0;
};

