#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "types.h"

typedef std::function<void(const ESPNowSensorPacket& packet, const uint8_t* mac)> SensorDataCallback;

class ESPNowReceiver {
public:
    static ESPNowReceiver& instance();

    bool init();
    void setCallback(SensorDataCallback cb);

    // Must be called from loop(): drains queued packets and dispatches the callback
    // in the Arduino loop task context (NOT in the Wi-Fi task).
    void process();

    // Internal static callback dispatched from ESP-NOW C interrupt/task
    // (runs inside the Wi-Fi driver task - must never block!)
    void handleReceivedPacket(const uint8_t* mac, const uint8_t* data, int len);

private:
    ESPNowReceiver();

    struct QueuedPacket {
        ESPNowSensorPacket packet;
        uint8_t mac[6];
    };

    SensorDataCallback callback = nullptr;
    QueueHandle_t rxQueue = nullptr;
    volatile uint32_t droppedPackets = 0;
    volatile uint32_t invalidPackets = 0;
    bool initialized = false;
};
