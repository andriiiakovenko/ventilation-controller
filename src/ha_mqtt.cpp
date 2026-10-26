#include "ha_mqtt.h"
#include "ventilation_controller.h"
#include <ArduinoJson.h>

HAMqttManager& HAMqttManager::instance() {
    static HAMqttManager _inst;
    return _inst;
}

HAMqttManager::HAMqttManager() : mqttClient(espClient) {}

void HAMqttManager::init() {
    if (!MQTT_ENABLED) return;

    mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    mqttClient.setBufferSize(1024); // Expand buffer for discovery JSONs

    mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->onMessage(topic, payload, length);
    });

    // Register callback with VentilationController so state changes are reflected to MQTT.
    // IMPORTANT: this callback can be invoked from the async_tcp task (web API) while the
    // controller mutex is held, so it must NOT do any socket I/O. Just flag it; the actual
    // publish happens in update() on the loop task.
    VentilationController::instance().registerStateChangeCallback([this]() {
        this->statePublishPending = true;
    });

    Serial.println("[MQTT] Initialized.");
}

bool HAMqttManager::reconnect() {
    if (mqttClient.connected()) return true;

    Serial.print("[MQTT] Attempting connection to broker...");
    bool connected = false;
    if (strlen(MQTT_USERNAME) > 0) {
        connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(MQTT_CLIENT_ID);
    }

    if (connected) {
        Serial.println(" Connected!");
        // Subscribe to control topics
        mqttClient.subscribe("ventilation/fan/speed/set");
        mqttClient.subscribe("ventilation/valve/+/set");
        mqttClient.subscribe("ventilation/mode/set");
        // Subscribe to Qingping / external Wi-Fi sensor topics
        mqttClient.subscribe("qingping/+/up");
        mqttClient.subscribe("ventilation/sensor/+/external");

        publishDiscovery();
        publishAllStates();
        return true;
    } else {
        Serial.printf(" Failed, rc=%d\n", mqttClient.state());
        return false;
    }
}

void HAMqttManager::publishDiscovery() {
    const char* devId = "esp32_ventilation";
    const char* devName = "Ventilation System";

    // 1. Fan Speed (Select Entity for HA)
    {
        JsonDocument doc;
        doc["name"] = "Ventilation Speed";
        doc["unique_id"] = "ventilation_speed_select";
        doc["command_topic"] = "ventilation/fan/speed/set";
        doc["state_topic"] = "ventilation/fan/speed/state";
        JsonArray options = doc["options"].to<JsonArray>();
        options.add("OFF");
        options.add("SLOW");
        options.add("MEDIUM");
        options.add("HIGH");

        JsonObject dev = doc["device"].to<JsonObject>();
        dev["identifiers"][0] = devId;
        dev["name"] = devName;
        dev["model"] = "ESP32-WROOM-32";

        String topic = "homeassistant/select/ventilation_speed/config";
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    // 2. Mode Switch (Auto/Manual)
    {
        JsonDocument doc;
        doc["name"] = "Ventilation Auto Mode";
        doc["unique_id"] = "ventilation_auto_mode";
        doc["command_topic"] = "ventilation/mode/set";
        doc["state_topic"] = "ventilation/mode/state";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";

        JsonObject dev = doc["device"].to<JsonObject>();
        dev["identifiers"][0] = devId;
        dev["name"] = devName;

        String topic = "homeassistant/switch/ventilation_auto_mode/config";
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    // 3. Room Valves Switches
    const char* roomIds[] = {"danylo", "anastasiia", "parents", "gym"};
    const char* valveNames[] = {"Danylo Room Valve", "Anastasiia Room Valve", "Parents Room Valve", "Gym Room Valve"};
    const char* roomNames[] = {"Danylo Room", "Anastasiia Room", "Parents Room", "Gym Room"};

    for (int i = 0; i < 4; i++) {
        JsonDocument doc;
        doc["name"] = valveNames[i];
        doc["unique_id"] = String("valve_") + roomIds[i];
        doc["command_topic"] = String("ventilation/valve/") + roomIds[i] + "/set";
        doc["state_topic"] = String("ventilation/valve/") + roomIds[i] + "/state";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["icon"] = "mdi:valve";

        JsonObject dev = doc["device"].to<JsonObject>();
        dev["identifiers"][0] = devId;
        dev["name"] = devName;

        String topic = String("homeassistant/switch/valve_") + roomIds[i] + "/config";
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    // 4. Room Sensors (Temp, Humidity, Air Quality & Time Since Last Update)
    for (int i = 0; i < 4; i++) {
        // Temperature
        {
            JsonDocument doc;
            doc["name"] = String(roomNames[i]) + " Temperature";
            doc["unique_id"] = String("temp_") + roomIds[i];
            doc["state_topic"] = String("ventilation/sensor/") + roomIds[i] + "/temp";
            doc["unit_of_measurement"] = "°C";
            doc["device_class"] = "temperature";

            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = devId;
            dev["name"] = devName;

            String topic = String("homeassistant/sensor/") + roomIds[i] + "_temp/config";
            String payload;
            serializeJson(doc, payload);
            mqttClient.publish(topic.c_str(), payload.c_str(), true);
        }
        // Humidity
        {
            JsonDocument doc;
            doc["name"] = String(roomNames[i]) + " Humidity";
            doc["unique_id"] = String("hum_") + roomIds[i];
            doc["state_topic"] = String("ventilation/sensor/") + roomIds[i] + "/hum";
            doc["unit_of_measurement"] = "%";
            doc["device_class"] = "humidity";

            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = devId;
            dev["name"] = devName;

            String topic = String("homeassistant/sensor/") + roomIds[i] + "_hum/config";
            String payload;
            serializeJson(doc, payload);
            mqttClient.publish(topic.c_str(), payload.c_str(), true);
        }
        // CO2
        {
            JsonDocument doc;
            doc["name"] = String(roomNames[i]) + " CO2";
            doc["unique_id"] = String("co2_") + roomIds[i];
            doc["state_topic"] = String("ventilation/sensor/") + roomIds[i] + "/co2";
            doc["unit_of_measurement"] = "ppm";
            doc["device_class"] = "carbon_dioxide";

            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = devId;
            dev["name"] = devName;

            String topic = String("homeassistant/sensor/") + roomIds[i] + "_co2/config";
            String payload;
            serializeJson(doc, payload);
            mqttClient.publish(topic.c_str(), payload.c_str(), true);
        }
        // PM2.5
        {
            JsonDocument doc;
            doc["name"] = String(roomNames[i]) + " PM2.5";
            doc["unique_id"] = String("pm25_") + roomIds[i];
            doc["state_topic"] = String("ventilation/sensor/") + roomIds[i] + "/pm25";
            doc["unit_of_measurement"] = "µg/m³";
            doc["device_class"] = "pm25";

            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = devId;
            dev["name"] = devName;

            String topic = String("homeassistant/sensor/") + roomIds[i] + "_pm25/config";
            String payload;
            serializeJson(doc, payload);
            mqttClient.publish(topic.c_str(), payload.c_str(), true);
        }
        // Last Update (seconds since last ESP-NOW packet received)
        {
            JsonDocument doc;
            doc["name"] = String(roomNames[i]) + " Last Update";
            doc["unique_id"] = String("last_update_") + roomIds[i];
            doc["state_topic"] = String("ventilation/sensor/") + roomIds[i] + "/last_update";
            doc["unit_of_measurement"] = "s";
            doc["device_class"] = "duration";
            doc["state_class"] = "measurement";
            doc["icon"] = "mdi:clock-outline";

            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = devId;
            dev["name"] = devName;

            String topic = String("homeassistant/sensor/") + roomIds[i] + "_last_update/config";
            String payload;
            serializeJson(doc, payload);
            mqttClient.publish(topic.c_str(), payload.c_str(), true);
        }
    }

    // Remove legacy "gim" discovery topics (empty retained payload tells HA & Mosquitto to delete them)
    mqttClient.publish("homeassistant/switch/valve_gim/config", "", true);
    mqttClient.publish("homeassistant/sensor/gim_temp/config", "", true);
    mqttClient.publish("homeassistant/sensor/gim_hum/config", "", true);
    mqttClient.publish("homeassistant/sensor/gim_co2/config", "", true);
    mqttClient.publish("homeassistant/sensor/gim_pm25/config", "", true);

    Serial.println("[MQTT] Home Assistant discovery entities published.");
}

void HAMqttManager::publishSpeedState() {
    FanSpeed sp = VentilationController::instance().getFanSpeed();
    const char* speedNames[] = {"OFF", "SLOW", "MEDIUM", "HIGH"};
    mqttClient.publish("ventilation/fan/speed/state", speedNames[(int)sp], true);
}

void HAMqttManager::publishValveState(const String& roomId, bool open) {
    String topic = String("ventilation/valve/") + roomId + "/state";
    mqttClient.publish(topic.c_str(), open ? "ON" : "OFF", true);
}

void HAMqttManager::publishModeState() {
    bool isAuto = (VentilationController::instance().getMode() == MODE_AUTO);
    mqttClient.publish("ventilation/mode/state", isAuto ? "ON" : "OFF", true);
}

void HAMqttManager::publishSensorTelemetry() {
    // Work on a copy so the controller lock isn't held during (potentially slow) socket writes.
    const auto rooms = VentilationController::instance().getRoomsSnapshot();
    unsigned long now = millis();
    for (const auto& pair : rooms) {
        const RoomSensorData& s = pair.second;
        if (s.lastSeen > 0) {
            unsigned long agoSec = (now >= s.lastSeen) ? ((now - s.lastSeen) / 1000UL) : 0UL;
            String lastUpdTopic = String("ventilation/sensor/") + pair.first + "/last_update";
            mqttClient.publish(lastUpdTopic.c_str(), String(agoSec).c_str(), true);
        }
        if (s.isOnline) {
            String tempTopic = String("ventilation/sensor/") + pair.first + "/temp";
            String humTopic = String("ventilation/sensor/") + pair.first + "/hum";

            mqttClient.publish(tempTopic.c_str(), String(s.temperature, 1).c_str(), true);
            mqttClient.publish(humTopic.c_str(), String(s.humidity, 1).c_str(), true);

            if (s.co2 > 0) {
                String co2Topic = String("ventilation/sensor/") + pair.first + "/co2";
                mqttClient.publish(co2Topic.c_str(), String(s.co2).c_str(), true);
            }
            String pmTopic = String("ventilation/sensor/") + pair.first + "/pm25";
            mqttClient.publish(pmTopic.c_str(), String(s.pm25).c_str(), true);
        }
    }
}

void HAMqttManager::publishAllStates() {
    if (!mqttClient.connected()) return;
    publishSpeedState();
    publishModeState();
    publishValveState("danylo", VentilationController::instance().getValveState("danylo"));
    publishValveState("anastasiia", VentilationController::instance().getValveState("anastasiia"));
    publishValveState("parents", VentilationController::instance().getValveState("parents"));
    publishValveState("gym", VentilationController::instance().getValveState("gym"));
    publishSensorTelemetry();
}

void HAMqttManager::onMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);

    // Route Qingping or external JSON sensor payloads before 64-byte control truncation
    if (topicStr.startsWith("qingping/") || topicStr.startsWith("ventilation/sensor/")) {
        String jsonPayload((const char*)payload, length);
        VentilationController::instance().onQingpingJson(topicStr, jsonPayload, "MQTT-Broker");
        return;
    }

    char msg[64];
    if (length >= sizeof(msg)) length = sizeof(msg) - 1;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    String msgStr = String(msg);
    msgStr.trim();

    Serial.printf("[MQTT] Received [%s]: %s\n", topicStr.c_str(), msgStr.c_str());

    if (topicStr == "ventilation/fan/speed/set") {
        msgStr.toUpperCase();
        if (msgStr == "0" || msgStr == "OFF") {
            VentilationController::instance().setFanSpeed(FAN_OFF);
        } else if (msgStr == "1" || msgStr == "SLOW") {
            VentilationController::instance().setFanSpeed(FAN_SLOW);
        } else if (msgStr == "2" || msgStr == "MEDIUM" || msgStr == "MED") {
            VentilationController::instance().setFanSpeed(FAN_MEDIUM);
        } else if (msgStr == "3" || msgStr == "HIGH") {
            VentilationController::instance().setFanSpeed(FAN_HIGH);
        }
    } else if (topicStr.startsWith("ventilation/valve/")) {
        // e.g. ventilation/valve/danylo/set
        int firstSlash = topicStr.indexOf('/', 12);
        String room = topicStr.substring(18, firstSlash);
        bool open = (msgStr.equalsIgnoreCase("ON") || msgStr == "1");
        VentilationController::instance().setValveState(room, open);
    } else if (topicStr == "ventilation/mode/set") {
        bool autoMode = (msgStr.equalsIgnoreCase("ON") || msgStr.equalsIgnoreCase("AUTO") || msgStr == "1");
        VentilationController::instance().setMode(autoMode ? MODE_AUTO : MODE_MANUAL);
    }
}

void HAMqttManager::update() {
    if (!MQTT_ENABLED) return;
    if (WiFi.status() != WL_CONNECTED) return;

    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 10000) {
            lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        mqttClient.loop();

        if (statePublishPending) {
            statePublishPending = false;
            publishAllStates();
        }

        unsigned long now = millis();
        if (now - lastTelemetryPublish >= 5000) {
            lastTelemetryPublish = now;
            publishSensorTelemetry();
        }
    }
}
