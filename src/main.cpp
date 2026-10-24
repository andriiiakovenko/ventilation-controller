#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "relays.h"
#include "types.h"
#include "esp_now_receiver.h"
#include "ventilation_controller.h"
#include "web_server.h"
#include "ha_mqtt.h"

static unsigned long bootTime = 0;
static bool softApDisabled = false;

void setupWiFi() {
    // Dual mode: SoftAP provides direct access + keeps Wi-Fi channel stable for ESP-NOW
    WiFi.mode(WIFI_AP_STA);

    // Start SoftAP on configured ESPNOW channel
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, ESPNOW_CHANNEL);
    Serial.printf("[WiFi] SoftAP started: %s (IP: %s, Channel: %d) - will disable in 5 mins if Station connects\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str(),
                  ESPNOW_CHANNEL);

    // If Station credentials are provided, attempt connection
    if (strlen(WIFI_STA_SSID) > 0 && String(WIFI_STA_SSID) != "YourWiFiSSID") {
        Serial.printf("[WiFi] Connecting to Station SSID: %s ...\n", WIFI_STA_SSID);
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);

        unsigned long startAttempt = millis();
        // Wait up to 8 seconds for connection to prevent blocking autonomous startup
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected to %s! Local IP: %s\n",
                          WIFI_STA_SSID,
                          WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[WiFi] Station connection timed out. Running autonomously via SoftAP.");
        }
    } else {
        Serial.println("[WiFi] No Station Wi-Fi credentials set. Running in autonomous AP mode.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    bootTime = millis();
    Serial.println("\n=============================================");
    Serial.println("  ESP32 AUTONOMOUS VENTILATION CONTROLLER");
    Serial.println("=============================================");

    // 1. Initialize hardware relays (ensures everything starts safe and OFF)
    RelayController::instance().init();

    // 2. Initialize ventilation logic controller
    VentilationController::instance().init();

    // 3. Setup Wi-Fi (SoftAP + STA)
    setupWiFi();

    // 4. Initialize ESP-NOW wireless sensor receiver
    if (ESPNowReceiver::instance().init()) {
        ESPNowReceiver::instance().setCallback([](const ESPNowSensorPacket& packet, const uint8_t* mac) {
            VentilationController::instance().onSensorPacket(packet, mac);
        });
    }

    // 5. Initialize Web Server and REST API (including Web OTA)
    WebServerManager::instance().init();

    // 6. Initialize Home Assistant MQTT client
    HAMqttManager::instance().init();

    Serial.println("[System] Boot completed. Controller is running.");
}

void loop() {
    // 0. Dispatch ESP-NOW packets queued by the Wi-Fi task (processed here, in the loop task)
    ESPNowReceiver::instance().process();

    // 1. Run periodic autonomous ventilation checks
    VentilationController::instance().update();

    // 2. Web server background tasks (e.g. scheduled OTA reboot)
    WebServerManager::instance().update();

    // 3. Process MQTT loop and auto-reconnects
    HAMqttManager::instance().update();

    // 4. SoftAP 5-minute auto-disable when connected to home Wi-Fi
    if (!softApDisabled && (millis() - bootTime >= SOFTAP_TIMEOUT_MS)) {
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.softAPdisconnect(true);
            softApDisabled = true;
            Serial.println("[WiFi] 5 minutes elapsed and Station is connected. SoftAP broadcasting disabled.");
        }
    }

    delay(10);
}
