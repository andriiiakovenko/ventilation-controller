#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

class HAMqttManager {
public:
    static HAMqttManager& instance();

    void init();
    void update(); // Called inside main loop

    void publishAllStates();
    void publishDiscovery();

private:
    HAMqttManager();
    WiFiClient espClient;
    PubSubClient mqttClient;

    unsigned long lastReconnectAttempt = 0;
    unsigned long lastTelemetryPublish = 0;

    // Set from any task when controller state changes; consumed in update()
    // (loop task) so MQTT socket I/O never runs in the Wi-Fi/async_tcp tasks.
    volatile bool statePublishPending = false;

    bool reconnect();
    void onMessage(char* topic, byte* payload, unsigned int length);
    void publishValveState(const String& roomId, bool open);
    void publishSpeedState();
    void publishModeState();
    void publishSensorTelemetry();
};
