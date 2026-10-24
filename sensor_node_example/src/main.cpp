/**
 * Wireless ESP32-C3 Super Mini Sensor Node (ESP-NOW Transmitter + OTA Support)
 * 
 * Hardware Sensors:
 * - Sensirion SCD40:
 *     SDA -> GPIO 8
 *     SCL -> GPIO 9
 * - Cubic PM1006K:
 *     TX of sensor -> GPIO 20 (RX on ESP32-C3)
 * 
 * Monitored Room: "parents" (or "danylo", "anastasiia", "gim")
 * Features:
 * - Broadcasts telemetry over ESP-NOW
 * - Connects to Wi-Fi (STA) with Web OTA (/update) and ArduinoOTA
 * - Reports firmware version and IP address to Main Controller
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <SensirionI2CScd4x.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoOTA.h>

// ================= USER CONFIGURATION =================
#ifndef ROOM_ID
#define ROOM_ID             "parents"     // Monitored room: "parents", "danylo", "anastasiia", "gim"
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION    "0.0.4"       // Current sensor node firmware version
#endif
#define ESPNOW_CHANNEL      1             // Matches controller ESP-NOW / Wi-Fi channel

// Wi-Fi Configuration for OTA
#define WIFI_STA_SSID       "Ridna Hata IOT"
#define WIFI_STA_PASSWORD   "10TuRidniyHati!"
#define WIFI_AP_FALLBACK    "ESP32-Ventilation-AP"
#define WIFI_AP_PASS        "ventilation123"

// Pin assignments
#define I2C_SDA_PIN         8             // SCD40 I2C SDA
#define I2C_SCL_PIN         9             // SCD40 I2C SCL
#define PM1006_RX_PIN       20            // Cubic PM1006K UART RX (connects to sensor TX)

#define TRANSMIT_INTERVAL_S 10            // Send interval in seconds
// ======================================================

// Must match struct definition on Main Controller
struct __attribute__((packed)) ESPNowSensorPacket {
    char roomId[16];
    float temperature;
    float humidity;
    float batteryVoltage;
    uint16_t co2;
    uint16_t pm25;
    uint32_t packetSeq;
    char fwVersion[12];
    char ip[16];
};

// Embedded firmware target descriptor placed in .rodata_custom_desc (at byte offset 288 of .bin)
#define VENT_FW_MAGIC           "VENT_FW_DESC_V1"
#define VENT_FW_DESC_OFFSET     288
#define VENT_FW_HEADER_MIN_SIZE 352 // 288 + sizeof(VentFwTargetDesc)[64]

struct __attribute__((packed)) VentFwTargetDesc {
    char magic[16];    // "VENT_FW_DESC_V1"
    char role[16];     // "master" or "slave"
    char targetId[16]; // "master" or room ID ("danylo", "anastasiia", "parents", "gim")
    char version[16];  // e.g. "0.0.4"
};

extern "C" const __attribute__((section(".rodata_custom_desc"), used)) VentFwTargetDesc VENT_FW_DESC = {
    VENT_FW_MAGIC,
    "slave",
    ROOM_ID,
    FIRMWARE_VERSION
};

struct SlaveOtaSession {
    uint8_t headerBuf[VENT_FW_HEADER_MIN_SIZE];
    size_t headerLen = 0;
    bool headerValidated = false;
    bool rejected = false;
    String errorMsg;
};

static SlaveOtaSession otaSession;

static bool validateSlaveHeader(const uint8_t* buf, size_t len, String& errOut) {
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
    if (chipId == 0x0000) {
        errOut = "Firmware rejected: This binary is compiled for Master Controller (ESP32), not a Slave Sensor Node (ESP32-C3).";
        return false;
    }
    if (chipId != 0x0005) {
        errOut = "Firmware rejected: Incompatible MCU chip ID (expected ESP32-C3 0x0005).";
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

    if (strcmp(role, "slave") != 0) {
        errOut = String("Firmware rejected: Binary role is '") + role + "', expected 'slave' (" ROOM_ID ").";
        return false;
    }

    String targetStr = String(targetId);
    targetStr.toLowerCase();
    targetStr.trim();
    String expectedRoom = String(ROOM_ID);
    expectedRoom.toLowerCase();
    expectedRoom.trim();

    if (targetStr != expectedRoom) {
        errOut = String("Firmware rejected: Binary is built for room '") + targetId +
                 "', but this sensor node is '" ROOM_ID "'.";
        return false;
    }

    return true;
}

// Broadcast MAC address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

RTC_DATA_ATTR uint32_t packetSequence = 0;
static unsigned long lastTransmitTime = 0;

// Sensor objects & state
SensirionI2CScd4x scd4x;
HardwareSerial pmSerial(1);

static bool scdInitialized = false;
static uint16_t latestCo2 = 0;
static float latestTemp = 0.0f;
static float latestHum = 0.0f;

static uint16_t latestPm25 = 0;
static uint8_t pmBuf[20];
static uint8_t pmIdx = 0;
static unsigned long lastPmReceivedTime = 0;

// OTA & Web Server
WebServer otaServer(80);
static bool shouldReboot = false;
static unsigned long rebootAt = 0;

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.printf("  [ESP-NOW] Send: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

bool initEspNow() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed!");
        return false;
    }

    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] Failed to add broadcast peer!");
        return false;
    }

    Serial.printf("[ESP-NOW] Initialized on Channel %d.\n", WiFi.channel());
    return true;
}

void setupOta() {
    otaServer.enableCORS(true);

    // Root status and upload HTML
    otaServer.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Sensor Node - " ROOM_ID "</title></head>"
                      "<body style='font-family:sans-serif;background:#1a202c;color:#edf2f7;padding:20px;'>"
                      "<h2>Sensor Node: " ROOM_ID "</h2>"
                      "<p>Target Identity: <b>slave:" ROOM_ID "</b> (Expected file: <code>ventslave_" ROOM_ID "_*.bin</code>)</p>"
                      "<p>Firmware Version: <b>" FIRMWARE_VERSION "</b></p>"
                      "<p>IP Address: <b>" + WiFi.localIP().toString() + "</b></p>"
                      "<hr style='border-color:#4a5568;'/>"
                      "<h3>Manual OTA Upgrade</h3>"
                      "<form method='POST' action='/update' enctype='multipart/form-data'>"
                      "<input type='file' name='update' accept='.bin' required> "
                      "<input type='submit' value='Upload & Flash'>"
                      "</form></body></html>";
        otaServer.send(200, "text/html", html);
    });

    // REST status endpoint
    otaServer.on("/api/status", HTTP_GET, []() {
        String json = "{\"room\":\"" ROOM_ID "\","
                      "\"role\":\"slave\","
                      "\"fw_target\":\"" + String(VENT_FW_DESC.targetId) + "\","
                      "\"expected_prefix\":\"ventslave_" ROOM_ID "_\","
                      "\"version\":\"" FIRMWARE_VERSION "\","
                      "\"ip\":\"" + WiFi.localIP().toString() + "\","
                      "\"mac\":\"" + WiFi.macAddress() + "\","
                      "\"uptime_sec\":" + String(millis() / 1000) + ","
                      "\"temp\":" + String(latestTemp, 1) + ","
                      "\"hum\":" + String(latestHum, 1) + ","
                      "\"co2\":" + String(latestCo2) + ","
                      "\"pm25\":" + String(latestPm25) + "}";
        otaServer.send(200, "application/json", json);
    });

    // CORS preflight for /update
    otaServer.on("/update", HTTP_OPTIONS, []() {
        otaServer.sendHeader("Access-Control-Allow-Private-Network", "true");
        otaServer.send(204);
    });

    // Web OTA upload endpoint
    otaServer.on(
        "/update", 
        HTTP_POST, 
        []() {
            bool ok = !otaSession.rejected && otaSession.headerValidated && !Update.hasError();
            String res;
            if (ok) {
                res = "{\"status\":\"ok\",\"message\":\"Sensor Node (" ROOM_ID ") OTA Update Successful! Rebooting...\"}";
            } else {
                String errMsg = otaSession.errorMsg;
                if (errMsg.length() == 0) {
                    errMsg = "Sensor Node Flash Error";
                }
                errMsg.replace("\"", "'");
                res = "{\"status\":\"error\",\"message\":\"" + errMsg + "\"}";
            }
            int statusCode = ok ? 200 : (otaSession.rejected ? 400 : 500);
            otaServer.send(statusCode, "application/json", res);
            if (ok) {
                shouldReboot = true;
                rebootAt = millis() + 1200;
                Serial.println("[OTA] Update completed successfully. Rebooting sensor node...");
            }
        }, 
        []() {
            HTTPUpload& upload = otaServer.upload();
            if (upload.status == UPLOAD_FILE_START) {
                otaSession.headerLen = 0;
                otaSession.headerValidated = false;
                otaSession.rejected = false;
                otaSession.errorMsg = "";

                Serial.printf("[OTA] Node [" ROOM_ID "] Firmware update starting: %s\n", upload.filename.c_str());

                // 1. Validate filename if provided
                String fnLower = upload.filename;
                fnLower.toLowerCase();
                int slashIdx = fnLower.lastIndexOf('/');
                if (slashIdx >= 0) fnLower = fnLower.substring(slashIdx + 1);
                int bslashIdx = fnLower.lastIndexOf('\\');
                if (bslashIdx >= 0) fnLower = fnLower.substring(bslashIdx + 1);

                String expectedRoom = String(ROOM_ID);
                expectedRoom.toLowerCase();
                expectedRoom.trim();
                String expectedPrefix = "ventslave_" + expectedRoom + "_";

                if (fnLower.startsWith("ventmaster")) {
                    otaSession.rejected = true;
                    otaSession.errorMsg = "Firmware rejected: Cannot flash Master Controller firmware (" + upload.filename + ") onto Slave Sensor Node '" ROOM_ID "'.";
                    Serial.printf("[OTA] REJECTED by filename: %s\n", otaSession.errorMsg.c_str());
                    return;
                }
                if (fnLower.startsWith("ventslave_") && !fnLower.startsWith(expectedPrefix)) {
                    otaSession.rejected = true;
                    otaSession.errorMsg = "Firmware rejected: File '" + upload.filename + "' does not match sensor node '" ROOM_ID "' (expected '" + expectedPrefix + "*.bin').";
                    Serial.printf("[OTA] REJECTED by filename: %s\n", otaSession.errorMsg.c_str());
                    return;
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (otaSession.rejected) {
                    return;
                }

                size_t offset = 0;
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
                    if (!validateSlaveHeader(otaSession.headerBuf, VENT_FW_HEADER_MIN_SIZE, otaSession.errorMsg)) {
                        otaSession.rejected = true;
                        Serial.printf("[OTA] REJECTED by binary descriptor: %s\n", otaSession.errorMsg.c_str());
                        return;
                    }

                    otaSession.headerValidated = true;
                    Serial.println("[OTA] Binary target verified: role='slave', target='" ROOM_ID "'.");

                    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                        Serial.print("[OTA] Update.begin() error: ");
                        Update.printError(Serial);
                        otaSession.errorMsg = "Update.begin() failed on flash partition.";
                        return;
                    }

                    if (Update.write(otaSession.headerBuf, VENT_FW_HEADER_MIN_SIZE) != VENT_FW_HEADER_MIN_SIZE) {
                        Update.printError(Serial);
                        otaSession.errorMsg = "Flash write error on initial header.";
                        return;
                    }
                }

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
                    Serial.printf("[OTA] Flashed %u bytes successfully.\n", (unsigned int)upload.totalSize);
                } else {
                    Update.printError(Serial);
                    if (otaSession.errorMsg.length() == 0) {
                        otaSession.errorMsg = "Firmware verification failed at Update.end().";
                    }
                }
            }
        }
    );

    otaServer.begin();
    Serial.println("[OTA] HTTP OTA Server started on port 80.");

    // ArduinoOTA service
    ArduinoOTA.setHostname("sensor-node-" ROOM_ID);
    ArduinoOTA.onStart([]() {
        Serial.println("[ArduinoOTA] Start flashing...");
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

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_STA_SSID);
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 6000) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! Node IP: %s (Channel: %d)\n",
                      WiFi.localIP().toString().c_str(), WiFi.channel());
    } else {
        Serial.println("[WiFi] STA connection failed. Trying Controller SoftAP fallback...");
        WiFi.begin(WIFI_AP_FALLBACK, WIFI_AP_PASS);
        start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 4000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected to SoftAP! Node IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[WiFi] Running in standalone STA mode for ESP-NOW.");
            esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
        }
    }
}

void initScd40() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(100);

    scd4x.begin(Wire);

    uint16_t err = scd4x.stopPeriodicMeasurement();
    if (err) {
        Serial.printf("[SCD40] Stop periodic warning: 0x%04X\n", err);
    }
    delay(500);

    err = scd4x.startPeriodicMeasurement();
    if (err) {
        Serial.printf("[SCD40] FAILED to start measurement: 0x%04X\n", err);
        scdInitialized = false;
    } else {
        Serial.println("[SCD40] Periodic measurement started successfully!");
        scdInitialized = true;
    }
}

void pollScd40() {
    if (!scdInitialized) {
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry > 10000) {
            lastRetry = millis();
            initScd40();
        }
        return;
    }

    bool isDataReady = false;
    uint16_t err = scd4x.getDataReadyFlag(isDataReady);
    if (!err && isDataReady) {
        uint16_t co2 = 0;
        float temp = 0.0f;
        float hum = 0.0f;
        err = scd4x.readMeasurement(co2, temp, hum);
        if (!err && co2 > 0) {
            latestCo2 = co2;
            latestTemp = temp;
            latestHum = hum;
            Serial.printf("[SCD40] CO2: %u ppm | Temp: %.2f C | Hum: %.1f %%\n",
                          latestCo2, latestTemp, latestHum);
        } else if (err) {
            Serial.printf("[SCD40] readMeasurement error: 0x%04X\n", err);
        }
    }
}

void initPm1006() {
    pmSerial.begin(9600, SERIAL_8N1, PM1006_RX_PIN, -1);
    Serial.printf("[PM1006K] UART initialized at 9600 baud on RX=GPIO%d\n", PM1006_RX_PIN);
}

void pollPm1006() {
    while (pmSerial.available()) {
        uint8_t b = pmSerial.read();

        if (pmIdx == 0) {
            if (b == 0x16) pmBuf[pmIdx++] = b;
        } else if (pmIdx == 1) {
            if (b == 0x11) pmBuf[pmIdx++] = b;
            else pmIdx = (b == 0x16) ? 1 : 0;
        } else if (pmIdx == 2) {
            if (b == 0x0B) pmBuf[pmIdx++] = b;
            else pmIdx = (b == 0x16) ? 1 : 0;
        } else {
            pmBuf[pmIdx++] = b;
            if (pmIdx >= 20) {
                uint8_t sum = 0;
                for (int i = 0; i < 20; i++) sum += pmBuf[i];

                if (sum == 0) {
                    latestPm25 = ((uint16_t)pmBuf[5] << 8) | pmBuf[6];
                    lastPmReceivedTime = millis();
                    Serial.printf("[PM1006K] Valid Frame -> PM2.5: %u ug/m3\n", latestPm25);
                } else {
                    Serial.printf("[PM1006K] Checksum mismatch: sum=0x%02X\n", sum);
                }
                pmIdx = 0;
            }
        }
    }
}

void sendPacket() {
    ESPNowSensorPacket packet;
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.roomId, ROOM_ID, sizeof(packet.roomId) - 1);
    packet.temperature = latestTemp;
    packet.humidity = latestHum;
    packet.batteryVoltage = 3.30f;
    packet.co2 = latestCo2;
    packet.pm25 = latestPm25;
    packet.packetSeq = packetSequence++;
    strncpy(packet.fwVersion, FIRMWARE_VERSION, sizeof(packet.fwVersion) - 1);

    if (WiFi.status() == WL_CONNECTED) {
        strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
    } else {
        strncpy(packet.ip, "0.0.0.0", sizeof(packet.ip) - 1);
    }

    Serial.println("\n---------------------------------------------");
    Serial.printf("[Sensor Node] [%s] Broadcasting Seq #%u (FW: %s, IP: %s)\n",
                  ROOM_ID, packet.packetSeq, packet.fwVersion, packet.ip);
    Serial.printf("  Temp:  %.1f C\n", packet.temperature);
    Serial.printf("  Hum:   %.1f %%\n", packet.humidity);
    Serial.printf("  CO2:   %u ppm\n", packet.co2);
    Serial.printf("  PM2.5: %u ug/m3\n", packet.pm25);
    Serial.println("---------------------------------------------");

    esp_err_t res = esp_now_send(broadcastAddress, (uint8_t*)&packet, sizeof(packet));
    if (res != ESP_OK) {
        Serial.printf("  -> Error sending packet: %d\n", res);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=============================================");
    Serial.println("  ESP32-C3 SUPER MINI - AIR QUALITY NODE (OTA READY)");
    Serial.printf ("  Room: %s | Firmware: %s\n", ROOM_ID, FIRMWARE_VERSION);
    Serial.println("=============================================");

    initWiFi();
    initEspNow();
    setupOta();
    initScd40();
    initPm1006();

    lastTransmitTime = millis();
}

void loop() {
    // 1. Process continuous sensor inputs
    pollScd40();
    pollPm1006();

    // 2. Process OTA web requests and background services
    otaServer.handleClient();
    ArduinoOTA.handle();

    // 3. Handle scheduled reboot after successful OTA
    if (shouldReboot && millis() >= rebootAt) {
        Serial.println("[OTA] Rebooting now!");
        ESP.restart();
    }

    // 4. Transmit telemetry every TRANSMIT_INTERVAL_S
    unsigned long now = millis();
    if (now - lastTransmitTime >= (TRANSMIT_INTERVAL_S * 1000UL)) {
        lastTransmitTime = now;
        sendPacket();
    }

    delay(10);
}
