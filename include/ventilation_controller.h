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

    // Sensor processing (ESP-NOW & Qingping Wi-Fi / External)
    void onSensorPacket(const ESPNowSensorPacket& packet, const uint8_t* mac);
    void onQingpingConnected(const String& rawMac, const String& remoteIp);
    bool onQingpingPayload(const String& topic, const uint8_t* data, size_t len, const String& remoteIp = "");
    bool onQingpingJson(const String& topic, const String& payload, const String& remoteIp = "");
    void onExternalRoomReadings(const String& roomId, float temp, float hum, uint16_t co2,
                                uint16_t pm25, uint16_t pm10, uint8_t batPct,
                                const String& sourceType = "qingping",
                                const String& mac = "", const String& ip = "");

    // Qingping MAC <-> Room binding management (persisted in NVS)
    bool bindQingpingToRoom(const String& roomId, const String& rawMac);
    String getRoomForQingpingMac(const String& rawMac) const;
    std::map<String, DiscoveredQingping> getDiscoveredQingpingsSnapshot() const;

    static String normalizeMac(const String& rawMac);
    static String formatMacColon(const String& normMac);

    // NOTE: getRooms() returns a reference that is NOT thread-safe. Use
    // getRoomsSnapshot() from any task other than the Arduino loop task.
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
    void loadQingpingBindings();
    void saveQingpingBinding(const String& roomId, const String& normMac);

    VentilationMode currentMode = MODE_MANUAL;
    std::map<String, RoomSensorData> rooms;
    std::map<String, DiscoveredQingping> discoveredQingpings;
    std::vector<StateChangeCallback> callbacks;

    // Guards controller state
    mutable std::recursive_mutex stateMutex;

    FanSpeed pendingFanSpeed = FAN_OFF;
    unsigned long fanTurnOnAt = 0;

    float humidityHighThreshold = DEFAULT_HUMIDITY_HIGH_THRESHOLD;
    float humidityTargetThreshold = DEFAULT_HUMIDITY_TARGET_THRESHOLD;

    unsigned long lastLogicRun = 0;
    unsigned long lastStatusCheck = 0;
};
