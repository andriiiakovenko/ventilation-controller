#include "web_server.h"
#include "web_assets.h"
#include "ventilation_controller.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <StreamString.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_ota_ops.h>

// Embedded target identity in .rodata_custom_desc (placed at byte offset 288 of firmware.bin)
extern "C" const __attribute__((section(".rodata_custom_desc"), used)) VentFwTargetDesc VENT_FW_DESC = {
    VENT_FW_MAGIC,
    "master",
    "master",
    FIRMWARE_VERSION
};

struct MasterOtaSession {
    uint8_t headerBuf[VENT_FW_HEADER_MIN_SIZE];
    size_t headerLen = 0;
    bool headerValidated = false;
    bool rejected = false;
    String errorMsg;
};

static MasterOtaSession otaSession;

static bool validateMasterHeader(const uint8_t* buf, size_t len, String& errOut) {
    if (len < VENT_FW_HEADER_MIN_SIZE) {
        errOut = "Firmware file is too small (< 352 bytes).";
        return false;
    }

    // 1. Check ESP image magic byte (0xE9)
    if (buf[0] != 0xE9) {
        errOut = "Invalid binary header (missing ESP32 0xE9 magic byte).";
        return false;
    }

    // 2. Check ESP chip_id at offset 12 (uint16_t little-endian: 0x0000 = ESP32, 0x0005 = ESP32-C3)
    uint16_t chipId = (uint16_t)buf[12] | ((uint16_t)buf[13] << 8);
    if (chipId == 0x0005) {
        errOut = "Firmware rejected: This binary is compiled for ESP32-C3 (Slave Sensor Node), not the Master ESP32 Controller.";
        return false;
    }
    if (chipId != 0x0000) {
        errOut = "Firmware rejected: Incompatible MCU chip ID (expected ESP32 0x0000).";
        return false;
    }

    // 3. Check embedded VentFwTargetDesc at offset 288
    const VentFwTargetDesc* desc = reinterpret_cast<const VentFwTargetDesc*>(buf + VENT_FW_DESC_OFFSET);
    if (strncmp(desc->magic, VENT_FW_MAGIC, sizeof(desc->magic)) != 0) {
        errOut = "Firmware rejected: Missing target identity signature (VENT_FW_DESC_V1). Rebuild with build.ps1.";
        return false;
    }

    char role[17] = {0};
    char targetId[17] = {0};
    memcpy(role, desc->role, 16);
    memcpy(targetId, desc->targetId, 16);

    if (strcmp(role, "master") != 0 || strcmp(targetId, "master") != 0) {
        errOut = String("Firmware rejected: Binary is built for '") + role + ":" + targetId +
                 "', but this device is 'master:master'.";
        return false;
    }

    return true;
}

WebServerManager& WebServerManager::instance() {
    static WebServerManager _inst;
    return _inst;
}

WebServerManager::WebServerManager() : server(80) {}

String WebServerManager::buildStatusJson() {
    JsonDocument doc;

    auto& ctrl = VentilationController::instance();
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = String(__DATE__) + " " + String(__TIME__);
    
    const esp_partition_t *running = esp_ota_get_running_partition();
    doc["running_partition"] = running ? running->label : "factory";
    
    doc["mode"] = (ctrl.getMode() == MODE_AUTO) ? "auto" : "manual";
    doc["speed"] = (int)ctrl.getFanSpeed();
    
    wifi_mode_t currentMode = WiFi.getMode();
    doc["ap_active"] = (currentMode == WIFI_AP || currentMode == WIFI_AP_STA);
    doc["wifi_channel"] = WiFi.channel();
    doc["uptime_sec"] = millis() / 1000;
    doc["local_ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

    JsonObject valvesObj = doc["valves"].to<JsonObject>();
    valvesObj["danylo"] = ctrl.getValveState("danylo");
    valvesObj["anastasiia"] = ctrl.getValveState("anastasiia");
    valvesObj["parents"] = ctrl.getValveState("parents");
    valvesObj["gim"] = ctrl.getValveState("gim");

    JsonObject sensorsObj = doc["sensors"].to<JsonObject>();
    JsonArray modulesArr = doc["modules"].to<JsonArray>();

    // 1. Main Controller
    JsonObject mainMod = modulesArr.add<JsonObject>();
    mainMod["id"] = "main";
    mainMod["name"] = "Ventilation Master";
    mainMod["model"] = "ESP32-WROOM-32";
    mainMod["role"] = "controller";
    mainMod["fw_target"] = VENT_FW_DESC.targetId;
    mainMod["expected_prefix"] = "ventmaster_";
    mainMod["version"] = FIRMWARE_VERSION;
    mainMod["build_date"] = String(__DATE__) + " " + String(__TIME__);
    mainMod["partition"] = running ? running->label : "factory";
    mainMod["ip"] = doc["local_ip"];
    mainMod["mac"] = doc["mac"];
    mainMod["online"] = true;
    mainMod["update_url"] = "/update";

    unsigned long now = millis();

    // This runs in the async_tcp task: iterate a locked copy, never the live map
    // (the loop task mutates it on every ESP-NOW packet).
    const auto rooms = ctrl.getRoomsSnapshot();
    for (const auto& pair : rooms) {
        const RoomSensorData& s = pair.second;
        JsonObject sObj = sensorsObj[pair.first].to<JsonObject>();
        sObj["name"] = s.friendlyName;
        sObj["temp"] = s.temperature;
        sObj["hum"] = s.humidity;
        sObj["co2"] = s.co2;
        sObj["pm25"] = s.pm25;
        sObj["bat"] = s.batteryVoltage;
        sObj["online"] = s.isOnline;
        sObj["version"] = s.fwVersion;
        sObj["ip"] = s.ip;
        sObj["mac"] = s.mac;
        sObj["ago_sec"] = s.lastSeen > 0 ? (float)(now - s.lastSeen) / 1000.0f : -1.0f;

        // Sensor module
        JsonObject nodeMod = modulesArr.add<JsonObject>();
        nodeMod["id"] = pair.first;
        nodeMod["name"] = s.friendlyName;
        nodeMod["model"] = "ESP32-C3 Sensor";
        nodeMod["role"] = "sensor";
        nodeMod["fw_target"] = pair.first;
        nodeMod["expected_prefix"] = "ventslave_" + pair.first + "_";
        nodeMod["version"] = s.fwVersion;
        nodeMod["ip"] = s.ip;
        nodeMod["mac"] = s.mac;
        nodeMod["online"] = s.isOnline;
        nodeMod["ago_sec"] = sObj["ago_sec"];
        if (s.ip.length() > 0 && s.ip != "0.0.0.0") {
            nodeMod["update_url"] = "http://" + s.ip + "/update";
        } else {
            nodeMod["update_url"] = "";
        }
    }

    String response;
    serializeJson(doc, response);
    return response;
}

void WebServerManager::scheduleReboot(unsigned long delayMs) {
    shouldReboot = true;
    rebootAt = millis() + delayMs;
    Serial.printf("[WebServer] System reboot scheduled in %lu ms\n", delayMs);
}

void WebServerManager::update() {
    server.handleClient();
    ArduinoOTA.handle();

    if (shouldReboot && millis() >= rebootAt) {
        Serial.println("[WebServer] Rebooting now for OTA update...");
        ESP.restart();
    }
}

void WebServerManager::setupRoutes() {
    server.enableCORS(true);

    // Serve Web Dashboard directly from flash PROGMEM
    server.on("/", HTTP_GET, [this]() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    // REST API Status
    server.on("/api/status", HTTP_GET, [this]() {
        server.send(200, "application/json", buildStatusJson());
    });

    // POST /api/speed?val=0..3
    server.on("/api/speed", HTTP_POST, [this]() {
        if (server.hasArg("val")) {
            int speedVal = server.arg("val").toInt();
            if (speedVal >= 0 && speedVal <= 3) {
                VentilationController::instance().setFanSpeed((FanSpeed)speedVal);
                server.send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
        server.send(400, "application/json", "{\"status\":\"invalid_param\"}");
    });

    // POST /api/valve?room=danylo&state=1
    server.on("/api/valve", HTTP_POST, [this]() {
        if (server.hasArg("room") && server.hasArg("state")) {
            String room = server.arg("room");
            int state = server.arg("state").toInt();
            VentilationController::instance().setValveState(room, state == 1);
            server.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
        server.send(400, "application/json", "{\"status\":\"invalid_param\"}");
    });

    // POST /api/mode?val=manual|auto
    server.on("/api/mode", HTTP_POST, [this]() {
        if (server.hasArg("val")) {
            String modeStr = server.arg("val");
            modeStr.toLowerCase();
            if (modeStr == "auto") {
                VentilationController::instance().setMode(MODE_AUTO);
            } else {
                VentilationController::instance().setMode(MODE_MANUAL);
            }
            server.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
        server.send(400, "application/json", "{\"status\":\"invalid_param\"}");
    });

    // POST /api/restart
    server.on("/api/restart", HTTP_POST, [this]() {
        server.send(200, "application/json", "{\"status\":\"rebooting\"}");
        this->scheduleReboot(1000);
    });

    // CORS preflight for /update (same as Slave sensor nodes)
    server.on("/update", HTTP_OPTIONS, [this]() {
        server.sendHeader("Access-Control-Allow-Private-Network", "true");
        server.send(204);
    });

    // OTA Firmware Update Endpoint (same WebServer + HTTPUpload pattern as Slave sensor nodes)
    server.on(
        "/update", 
        HTTP_POST, 
        // --- Completion handler (runs after all upload chunks are received) ---
        [this]() {
            bool updateOK = !otaSession.rejected && otaSession.headerValidated && !Update.hasError();
            String jsonResponse;
            if (updateOK) {
                jsonResponse = "{\"status\":\"ok\",\"message\":\"Master Controller OTA Update Successful! Rebooting...\"}";
            } else {
                String errMsg = otaSession.errorMsg;
                if (errMsg.length() == 0) {
                    StreamString errStr;
                    Update.printError(errStr);
                    errMsg = errStr;
                    errMsg.trim();
                }
                if (errMsg.length() == 0) {
                    errMsg = "Master Controller Flash Error";
                }
                errMsg.replace("\"", "'");
                jsonResponse = "{\"status\":\"error\",\"message\":\"" + errMsg + "\"}";
            }
            Serial.printf("[OTA] Final result: %s (%s)\n",
                          updateOK ? "SUCCESS" : "FAILED",
                          updateOK ? "OK" : otaSession.errorMsg.c_str());

            int statusCode = updateOK ? 200 : (otaSession.rejected ? 400 : 500);
            server.send(statusCode, "application/json", jsonResponse);

            if (updateOK) {
                this->scheduleReboot(1200);
            }
        },
        // --- Upload chunk handler (called for each chunk of the file) ---
        [this]() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                otaSession.headerLen = 0;
                otaSession.headerValidated = false;
                otaSession.rejected = false;
                otaSession.errorMsg = "";

                Serial.println("=========================================");
                Serial.printf("[OTA] Master Firmware update starting: %s\n", upload.filename.c_str());
                Serial.println("=========================================");

                // 1. Validate filename if provided
                String fnLower = upload.filename;
                fnLower.toLowerCase();
                int slashIdx = fnLower.lastIndexOf('/');
                if (slashIdx >= 0) fnLower = fnLower.substring(slashIdx + 1);
                int bslashIdx = fnLower.lastIndexOf('\\');
                if (bslashIdx >= 0) fnLower = fnLower.substring(bslashIdx + 1);

                if (fnLower.startsWith("ventslave")) {
                    otaSession.rejected = true;
                    otaSession.errorMsg = "Firmware rejected: Cannot flash slave sensor firmware (" + upload.filename + ") onto Master Controller.";
                    Serial.printf("[OTA] REJECTED by filename: %s\n", otaSession.errorMsg.c_str());
                    return;
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (otaSession.rejected) {
                    return;
                }

                size_t offset = 0;

                // Buffer the first 352 bytes to verify ESP32 header + embedded VentFwTargetDesc BEFORE touching flash
                if (!otaSession.headerValidated) {
                    size_t needed = VENT_FW_HEADER_MIN_SIZE - otaSession.headerLen;
                    size_t toCopy = (upload.currentSize < needed) ? upload.currentSize : needed;
                    memcpy(otaSession.headerBuf + otaSession.headerLen, upload.buf, toCopy);
                    otaSession.headerLen += toCopy;
                    offset += toCopy;

                    if (otaSession.headerLen < VENT_FW_HEADER_MIN_SIZE) {
                        return;
                    }

                    // Validate binary header & embedded VentFwTargetDesc before touching flash
                    if (!validateMasterHeader(otaSession.headerBuf, VENT_FW_HEADER_MIN_SIZE, otaSession.errorMsg)) {
                        otaSession.rejected = true;
                        Serial.printf("[OTA] REJECTED by binary descriptor: %s\n", otaSession.errorMsg.c_str());
                        return;
                    }

                    otaSession.headerValidated = true;
                    Serial.println("[OTA] Binary target verified: role='master', target='master'.");

                    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                        Serial.print("[OTA] Update.begin() FAILED: ");
                        Update.printError(Serial);
                        otaSession.errorMsg = "Update.begin() failed on flash partition.";
                        return;
                    }

                    // Write the validated 352-byte header to flash first
                    if (Update.write(otaSession.headerBuf, VENT_FW_HEADER_MIN_SIZE) != VENT_FW_HEADER_MIN_SIZE) {
                        Update.printError(Serial);
                        otaSession.errorMsg = "Flash write error on initial header.";
                        return;
                    }
                }

                // Write remaining chunk bytes to flash
                size_t remLen = upload.currentSize - offset;
                if (remLen > 0 && !Update.hasError()) {
                    if (Update.write(upload.buf + offset, remLen) != remLen) {
                        Update.printError(Serial);
                        otaSession.errorMsg = "Flash write error during payload transfer.";
                    }
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (otaSession.rejected) {
                    return;
                }
                if (!otaSession.headerValidated) {
                    otaSession.rejected = true;
                    otaSession.errorMsg = "Firmware rejected: File is too small (< 352 bytes).";
                    Serial.printf("[OTA] REJECTED: %s\n", otaSession.errorMsg.c_str());
                    return;
                }
                if (Update.end(true)) {
                    Serial.printf("[OTA] Flashed %u bytes successfully. Rebooting shortly...\n", (unsigned int)upload.totalSize);
                } else {
                    Update.printError(Serial);
                    if (otaSession.errorMsg.length() == 0) {
                        otaSession.errorMsg = "Firmware verification failed at Update.end().";
                    }
                }
            }
        }
    );

    // 404 handler
    server.onNotFound([this]() {
        server.send(404, "text/plain", "Not Found");
    });
}

void WebServerManager::init() {
    setupRoutes();
    server.begin();
    Serial.println("[WebServer] HTTP Server started on port 80 (with OTA support).");

    // ArduinoOTA service (same as Slave sensor nodes)
    ArduinoOTA.setHostname("ventilation-master");
    ArduinoOTA.onStart([]() {
        Serial.println("[ArduinoOTA] Start flashing Master...");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[ArduinoOTA] Flashing complete.");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[ArduinoOTA] Error[%u]\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("[ArduinoOTA] Service listening.");
}
