#include "ventilation_controller.h"
#include "qingping_broker.h"
#include <Preferences.h>
#include <ArduinoJson.h>

VentilationController& VentilationController::instance() {
    static VentilationController _inst;
    return _inst;
}

VentilationController::VentilationController() {}

String VentilationController::normalizeMac(const String& rawMac) {
    String out = "";
    for (size_t i = 0; i < rawMac.length(); i++) {
        char c = rawMac[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            out += (char)toupper(c);
        }
    }
    return out;
}

String VentilationController::formatMacColon(const String& normMac) {
    String clean = normalizeMac(normMac);
    if (clean.length() != 12) return clean;
    String out = "";
    for (int i = 0; i < 12; i += 2) {
        if (i > 0) out += ":";
        out += clean.substring(i, i + 2);
    }
    return out;
}

void VentilationController::init() {
    // 1. Danylo Room <- Stue Air Monitor (Qingping CGDN1 over Wi-Fi)
    RoomSensorData danylo;
    danylo.id = "danylo";
    danylo.friendlyName = "Danylo Room";
    danylo.sensorType = "qingping";
    danylo.sensorLabel = "Stue Air Monitor (Qingping)";
    danylo.assignedMac = "CCB5D132210E";
    danylo.mac = "CC:B5:D1:32:21:0E";
    danylo.fwVersion = "CGDN1";
    rooms[danylo.id] = danylo;

    // 2. Anastasiia Room <- Kontor Air Monitor (Qingping CGDN1 over Wi-Fi)
    RoomSensorData anastasiia;
    anastasiia.id = "anastasiia";
    anastasiia.friendlyName = "Anastasiia Room";
    anastasiia.sensorType = "qingping";
    anastasiia.sensorLabel = "Kontor Air Monitor (Qingping)";
    anastasiia.assignedMac = "CCB5D1321C16";
    anastasiia.mac = "CC:B5:D1:32:1C:16";
    anastasiia.fwVersion = "CGDN1";
    rooms[anastasiia.id] = anastasiia;

    // 3. Parents Room <- Foreldre Air Monitor (Qingping CGDN1 over Wi-Fi)
    RoomSensorData parents;
    parents.id = "parents";
    parents.friendlyName = "Parents Room";
    parents.sensorType = "qingping";
    parents.sensorLabel = "Foreldre Air Monitor (Qingping)";
    parents.assignedMac = "CCB5D1322341";
    parents.mac = "CC:B5:D1:32:23:41";
    parents.fwVersion = "CGDN1";
    rooms[parents.id] = parents;

    // 4. Gym Room <- Custom ESP32-C3 Sensor (ESP-NOW)
    RoomSensorData gym;
    gym.id = "gym";
    gym.friendlyName = "Gym Room";
    gym.sensorType = "espnow";
    gym.sensorLabel = "ESP32-C3 Sensor (ESP-NOW)";
    gym.fwVersion = "0.0.4";
    rooms[gym.id] = gym;

    // Load saved Qingping MAC bindings from NVS flash
    loadQingpingBindings();

    // Start with default MANUAL mode, FAN OFF, valves CLOSED
    currentMode = MODE_MANUAL;
    pendingFanSpeed = FAN_OFF;
    RelayController::instance().setFanSpeed(FAN_OFF);
    RelayController::instance().setDanyloValve(false);
    RelayController::instance().setAnastasiiaValve(false);
    RelayController::instance().setParentsValve(false);
    RelayController::instance().setGymValve(false);

    Serial.println("[VentilationController] Controller initialized (3 Qingping Wi-Fi rooms + 1 ESP-NOW Gym room).");
}

void VentilationController::loadQingpingBindings() {
    Preferences prefs;
    if (prefs.begin("vent_cfg", true)) {
        const char* roomIds[] = {"danylo", "anastasiia", "parents", "gym"};
        for (int i = 0; i < 4; i++) {
            String key = String("qp_") + roomIds[i];
            String savedMac = normalizeMac(prefs.getString(key.c_str(), ""));
            if (savedMac.length() == 12) {
                rooms[roomIds[i]].assignedMac = savedMac;
                rooms[roomIds[i]].mac = formatMacColon(savedMac);
                rooms[roomIds[i]].sensorType = "qingping";
                Serial.printf("[VentilationController] Loaded Qingping MAC binding: %s -> %s\n",
                              roomIds[i], rooms[roomIds[i]].mac.c_str());
            }
        }
        prefs.end();
    }
}

void VentilationController::saveQingpingBinding(const String& roomId, const String& normMac) {
    Preferences prefs;
    if (prefs.begin("vent_cfg", false)) {
        String key = String("qp_") + roomId;
        if (normMac.length() == 0) {
            prefs.remove(key.c_str());
        } else {
            prefs.putString(key.c_str(), normMac);
        }
        prefs.end();
    }
}

bool VentilationController::bindQingpingToRoom(const String& roomId, const String& rawMac) {
    String rId = roomId;
    rId.toLowerCase();
    rId.trim();

    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    auto it = rooms.find(rId);
    if (it == rooms.end()) return false;

    String normMac = normalizeMac(rawMac);
    if (normMac.length() != 0 && normMac.length() != 12) {
        return false;
    }

    // If this MAC was previously assigned to another room, unbind it there first
    if (normMac.length() == 12) {
        for (auto& pair : rooms) {
            if (pair.first != rId && pair.second.assignedMac == normMac) {
                pair.second.assignedMac = "";
                saveQingpingBinding(pair.first, "");
            }
        }
    }

    RoomSensorData& room = it->second;
    room.assignedMac = normMac;
    saveQingpingBinding(rId, normMac);

    if (normMac.length() == 12) {
        room.sensorType = "qingping";
        room.mac = formatMacColon(normMac);
        room.fwVersion = "CGDN1";

        // If we already have recent discovered readings from this Qingping, apply them immediately!
        auto dIt = discoveredQingpings.find(normMac);
        if (dIt != discoveredQingpings.end() && dIt->second.hasMetrics) {
            const DiscoveredQingping& dq = dIt->second;
            room.temperature = dq.temperature;
            room.humidity = dq.humidity;
            room.co2 = dq.co2;
            room.pm25 = dq.pm25;
            room.pm10 = dq.pm10;
            room.batteryPct = dq.batteryPct;
            room.batteryVoltage = dq.batteryPct / 20.0f; // approx scale
            room.ip = dq.ip;
            room.lastSeen = dq.lastSeen;
            room.isOnline = (millis() - dq.lastSeen <= SENSOR_OFFLINE_TIMEOUT_MS);
        }
        Serial.printf("[VentilationController] Bound Qingping %s -> room '%s'\n",
                      room.mac.c_str(), rId.c_str());
    } else {
        Serial.printf("[VentilationController] Cleared Qingping binding for room '%s'\n", rId.c_str());
    }

    if (currentMode == MODE_AUTO) {
        runAutonomousLogic();
    }
    notifyChange();
    return true;
}

String VentilationController::getRoomForQingpingMac(const String& rawMac) const {
    String normMac = normalizeMac(rawMac);
    if (normMac.length() != 12) return "";
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    for (const auto& pair : rooms) {
        if (pair.second.assignedMac == normMac) {
            return pair.first;
        }
    }
    return "";
}

std::map<String, DiscoveredQingping> VentilationController::getDiscoveredQingpingsSnapshot() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return discoveredQingpings;
}

void VentilationController::registerStateChangeCallback(StateChangeCallback cb) {
    callbacks.push_back(cb);
}

void VentilationController::notifyChange() {
    for (auto& cb : callbacks) {
        if (cb) cb();
    }
}

VentilationMode VentilationController::getMode() const {
    return currentMode;
}

void VentilationController::setMode(VentilationMode mode) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    if (currentMode != mode) {
        currentMode = mode;
        Serial.printf("[VentilationController] Mode switched to: %s\n", mode == MODE_AUTO ? "AUTO" : "MANUAL");
        if (currentMode == MODE_AUTO) {
            runAutonomousLogic();
        }
        notifyChange();
    }
}

bool VentilationController::isAnyValveOpen() const {
    return RelayController::instance().isDanyloValveOpen() ||
           RelayController::instance().isAnastasiiaValveOpen() ||
           RelayController::instance().isParentsValveOpen() ||
           RelayController::instance().isGymValveOpen();
}

FanSpeed VentilationController::getFanSpeed() const {
    if (pendingFanSpeed != FAN_OFF) {
        return pendingFanSpeed;
    }
    return RelayController::instance().getFanSpeed();
}

void VentilationController::setFanSpeed(FanSpeed speed) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    if (speed == FAN_OFF) {
        pendingFanSpeed = FAN_OFF;
        RelayController::instance().setFanSpeed(FAN_OFF);

        // Turn all valves OFF when fan turns OFF
        RelayController::instance().setDanyloValve(false);
        RelayController::instance().setAnastasiiaValve(false);
        RelayController::instance().setParentsValve(false);
        RelayController::instance().setGymValve(false);
        for (auto& pair : rooms) {
            pair.second.valveState = false;
        }

        Serial.println("[VentilationController] Fan OFF: All valves closed.");
        notifyChange();
        return;
    }

    // Safety interlock: Do not run fan if all valves are closed!
    if (!isAnyValveOpen()) {
        Serial.println("[VentilationController] SAFETY INTERLOCK: Cannot start fan while all valves are closed! Opening Gym valve as fallback.");
        RelayController::instance().setGymValve(true);
        rooms["gym"].valveState = true;
    }

    // If fan is currently completely OFF, wait 2 seconds for motorized valves to open
    if (RelayController::instance().getFanSpeed() == FAN_OFF) {
        if (pendingFanSpeed == FAN_OFF) {
            fanTurnOnAt = millis() + VALVE_OPENING_DELAY_MS;
            Serial.printf("[VentilationController] Fan starting: Waiting %d ms for air valves to open...\n", VALVE_OPENING_DELAY_MS);
        }
        pendingFanSpeed = speed;
    } else {
        pendingFanSpeed = FAN_OFF;
        RelayController::instance().setFanSpeed(speed);
    }

    notifyChange();
}

bool VentilationController::getValveState(const String& roomId) const {
    if (roomId == "danylo") return RelayController::instance().isDanyloValveOpen();
    if (roomId == "anastasiia") return RelayController::instance().isAnastasiiaValveOpen();
    if (roomId == "parents") return RelayController::instance().isParentsValveOpen();
    if (roomId == "gym") return RelayController::instance().isGymValveOpen();
    return false;
}

void VentilationController::setValveState(const String& roomId, bool open) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    if (roomId == "danylo") {
        RelayController::instance().setDanyloValve(open);
    } else if (roomId == "anastasiia") {
        RelayController::instance().setAnastasiiaValve(open);
    } else if (roomId == "parents") {
        RelayController::instance().setParentsValve(open);
    } else if (roomId == "gym") {
        RelayController::instance().setGymValve(open);
    }

    if (rooms.find(roomId) != rooms.end()) {
        rooms[roomId].valveState = open;
    }

    // Safety check: if all valves were just closed while fan is running or pending, stop the fan!
    if (!isAnyValveOpen() && (RelayController::instance().getFanSpeed() != FAN_OFF || pendingFanSpeed != FAN_OFF)) {
        Serial.println("[VentilationController] All valves closed! Stopping fan immediately to protect ductwork.");
        pendingFanSpeed = FAN_OFF;
        RelayController::instance().setFanSpeed(FAN_OFF);
    }

    notifyChange();
}

void VentilationController::onSensorPacket(const ESPNowSensorPacket& packet, const uint8_t* mac) {
    String rId = String(packet.roomId);
    rId.toLowerCase();
    rId.trim();

    std::lock_guard<std::recursive_mutex> lock(stateMutex);

    // Format sender MAC early so we can identify specific ESP-NOW hardware nodes
    String senderMacStr = "";
    if (mac != nullptr) {
        char macBuf[18];
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        senderMacStr = String(macBuf);
    }

    // Route the original 'parents' ESP32-C3 node (64:E8:33:BB:3E:EC) or any 'parents' ESP-NOW packet
    // while 'parents' is bound to a Qingping MAC over to 'gym' room.
    if (rId == "parents" && (senderMacStr == "64:E8:33:BB:3E:EC" || rooms["parents"].assignedMac.length() == 12)) {
        rId = "gym";
    }

    if (rooms.find(rId) == rooms.end()) {
        RoomSensorData newRoom;
        newRoom.id = rId;
        newRoom.friendlyName = rId;
        rooms[rId] = newRoom;
    }

    RoomSensorData& room = rooms[rId];
    room.sensorType = "espnow";
    room.sensorLabel = "IKEA VINDRIKTNING + ESP32-C3 (ESP-NOW)";
    room.temperature = packet.temperature;
    room.humidity = packet.humidity;
    room.batteryVoltage = packet.batteryVoltage;
    room.co2 = packet.co2;
    room.pm25 = packet.pm25;
    room.lastSeen = millis();
    room.isOnline = true;
    if (strlen(packet.fwVersion) > 0) {
        room.fwVersion = String(packet.fwVersion);
    }
    if (strlen(packet.ip) > 0 && String(packet.ip) != "0.0.0.0") {
        room.ip = String(packet.ip);
    }
    if (mac != nullptr) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        room.mac = String(macStr);
    }

    Serial.printf("[ESP-NOW] Sensor [%s]: Temp=%.1f C, Hum=%.1f %%, CO2=%u ppm, PM2.5=%u ug/m3, FW=%s, IP=%s\n",
                  rId.c_str(), room.temperature, room.humidity, room.co2, room.pm25,
                  room.fwVersion.c_str(), room.ip.c_str());

    if (currentMode == MODE_AUTO) {
        runAutonomousLogic();
    }
    notifyChange();
}

static bool extractJsonFloat(JsonVariantConst obj, const char* key, float& outVal) {
    if (obj[key].is<JsonObjectConst>()) {
        JsonVariantConst v = obj[key]["value"];
        if (v.is<float>() || v.is<int>()) {
            outVal = v.as<float>();
            return true;
        }
    } else if (obj[key].is<float>() || obj[key].is<int>()) {
        outVal = obj[key].as<float>();
        return true;
    }
    return false;
}

static bool extractJsonUInt16(JsonVariantConst obj, const char* key, uint16_t& outVal) {
    if (obj[key].is<JsonObjectConst>()) {
        JsonVariantConst v = obj[key]["value"];
        if (v.is<int>() || v.is<float>()) {
            int val = (int)v.as<float>();
            if (val >= 0 && val <= 65535) {
                outVal = (uint16_t)val;
                return true;
            }
        }
    } else if (obj[key].is<int>() || obj[key].is<float>()) {
        int val = (int)obj[key].as<float>();
        if (val >= 0 && val <= 65535) {
            outVal = (uint16_t)val;
            return true;
        }
    }
    return false;
}

void VentilationController::onQingpingConnected(const String& rawMac, const String& remoteIp) {
    String normMac = normalizeMac(rawMac);
    if (normMac.length() != 12) return;

    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    unsigned long now = millis();
    DiscoveredQingping& dq = discoveredQingpings[normMac];
    dq.mac = normMac;
    dq.formattedMac = formatMacColon(normMac);
    if (remoteIp.length() > 0) dq.ip = remoteIp;
    dq.lastSeen = now;

    String targetRoom = getRoomForQingpingMac(normMac);
    if (targetRoom.length() > 0 && rooms.find(targetRoom) != rooms.end()) {
        RoomSensorData& room = rooms[targetRoom];
        room.sensorType = "qingping";
        room.mac = dq.formattedMac;
        if (remoteIp.length() > 0) room.ip = remoteIp;
        if (dq.hasMetrics) {
            room.isOnline = true;
        }
    }
    notifyChange();
}

bool VentilationController::onQingpingPayload(const String& topic, const uint8_t* data, size_t len, const String& remoteIp) {
    if (!data || len == 0) return false;

    // Check if payload starts with Qingping binary TLV magic bytes "CG" (0x43, 0x47)
    if (len >= 5 && data[0] == 0x43 && data[1] == 0x47) {
        String rawMac = "";
        if (topic.startsWith("qingping/")) {
            int secondSlash = topic.indexOf('/', 9);
            if (secondSlash > 9) {
                rawMac = topic.substring(9, secondSlash);
            }
        }
        String normMac = normalizeMac(rawMac);

        float temp = 0.0f, hum = 0.0f;
        uint16_t co2 = 0, pm25 = 0, pm10 = 0, bat = 0;
        bool hasTemp = false, hasHum = false, hasCo2 = false, hasPm = false, hasBat = false;

        // Parse TLV blocks after "CG" header (some packets have 2-byte "CG", some have "CG" + 1-byte cmd + 2-byte len)
        size_t idx = (len > 5 && (data[3] + (data[4] << 8) == (int)len - 5)) ? 5 : 2;
        while (idx + 2 <= len) {
            uint8_t tag = data[idx];
            uint8_t tLen = data[idx + 1];
            if (idx + 2 + tLen > len) break;
            const uint8_t* val = data + idx + 2;

            if (tag == 0x01 && tLen >= 4) { // Temp (int16/10) + Hum (uint16/10)
                int16_t rawT = (int16_t)((uint16_t)val[0] | ((uint16_t)val[1] << 8));
                uint16_t rawH = (uint16_t)val[2] | ((uint16_t)val[3] << 8);
                temp = rawT / 10.0f;
                hum = rawH / 10.0f;
                hasTemp = true;
                hasHum = true;
            } else if (tag == 0x02 && tLen >= 1) { // Battery %
                bat = val[0];
                hasBat = true;
            } else if (tag == 0x12 && tLen >= 4) { // PM2.5 + PM10
                pm25 = (uint16_t)val[0] | ((uint16_t)val[1] << 8);
                pm10 = (uint16_t)val[2] | ((uint16_t)val[3] << 8);
                hasPm = true;
            } else if (tag == 0x13 && tLen >= 2) { // CO2 ppm
                co2 = (uint16_t)val[0] | ((uint16_t)val[1] << 8);
                hasCo2 = true;
            }
            idx += 2 + tLen;
        }

        if (normMac.length() == 12 && (hasTemp || hasHum || hasCo2 || hasPm)) {
            String targetRoom = getRoomForQingpingMac(normMac);
            std::lock_guard<std::recursive_mutex> lock(stateMutex);
            DiscoveredQingping& dq = discoveredQingpings[normMac];
            dq.mac = normMac;
            dq.formattedMac = formatMacColon(normMac);
            if (remoteIp.length() > 0) dq.ip = remoteIp;
            dq.lastSeen = millis();
            if (hasTemp) dq.temperature = temp;
            if (hasHum)  dq.humidity = hum;
            if (hasCo2)  dq.co2 = co2;
            if (hasPm)   { dq.pm25 = pm25; dq.pm10 = pm10; }
            if (hasBat)  dq.batteryPct = (uint8_t)min((uint16_t)100, bat);
            dq.hasMetrics = true;

            if (targetRoom.length() > 0) {
                onExternalRoomReadings(targetRoom, dq.temperature, dq.humidity, dq.co2, dq.pm25, dq.pm10, dq.batteryPct, "qingping", normMac, remoteIp);
            } else {
                notifyChange();
            }
            return true;
        }
    }

    String strPayload((const char*)data, len);
    return onQingpingJson(topic, strPayload, remoteIp);
}

// Fallback extractor for large (>4KB) historical JSON payloads where only the tail 4KB is kept
static bool extractLastJsonNumberFromTail(const String& str, const char* key, float& outVal) {
    String needle = String("\"") + key + "\"";
    int pos = str.lastIndexOf(needle);
    if (pos < 0) return false;
    int colon = str.indexOf(':', pos + needle.length());
    if (colon < 0) return false;

    // Look within the next 64 chars for either a direct number or "value":<number>
    String sub = str.substring(colon + 1, min((int)str.length(), colon + 65));
    int valIdx = sub.indexOf("\"value\"");
    if (valIdx >= 0) {
        int valColon = sub.indexOf(':', valIdx + 7);
        if (valColon >= 0) {
            sub = sub.substring(valColon + 1);
        }
    }
    sub.trim();
    for (size_t i = 0; i < sub.length(); i++) {
        char c = sub[i];
        if ((c >= '0' && c <= '9') || c == '-') {
            outVal = sub.substring(i).toFloat();
            return true;
        }
    }
    return false;
}

bool VentilationController::onQingpingJson(const String& topic, const String& payload, const String& remoteIp) {
    // 1. Extract MAC from topic (e.g. "qingping/CC-B5-D1-32-1C-16/up") first
    String rawMac = "";
    if (topic.startsWith("qingping/")) {
        int secondSlash = topic.indexOf('/', 9);
        if (secondSlash > 9) {
            rawMac = topic.substring(9, secondSlash);
        }
    }

    float temp = 0.0f, hum = 0.0f;
    uint16_t co2 = 0, pm25 = 0, pm10 = 0, bat = 0;
    bool hasTemp = false, hasHum = false, hasCo2 = false, hasPm25 = false, hasPm10 = false, hasBat = false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
        if (rawMac.length() == 0 && doc["mac"].is<const char*>()) {
            rawMac = doc["mac"].as<const char*>();
        }
        if (rawMac.length() == 0 && doc["info"]["mac"].is<const char*>()) {
            rawMac = doc["info"]["mac"].as<const char*>();
        }

        JsonVariantConst sObj = doc.as<JsonVariantConst>();
        if (doc["sensorData"].is<JsonArrayConst>()) {
            JsonArrayConst arr = doc["sensorData"].as<JsonArrayConst>();
            if (arr.size() > 0) {
                sObj = arr[arr.size() - 1]; // latest entry in array
            }
        } else if (doc["data"].is<JsonArrayConst>()) {
            JsonArrayConst arr = doc["data"].as<JsonArrayConst>();
            if (arr.size() > 0) {
                sObj = arr[arr.size() - 1];
            }
        }

        hasTemp = extractJsonFloat(sObj, "temperature", temp) || extractJsonFloat(sObj, "temp", temp);
        hasHum  = extractJsonFloat(sObj, "humidity", hum) || extractJsonFloat(sObj, "hum", hum);
        hasCo2  = extractJsonUInt16(sObj, "co2", co2);
        hasPm25 = extractJsonUInt16(sObj, "pm25", pm25);
        hasPm10 = extractJsonUInt16(sObj, "pm10", pm10);
        hasBat  = extractJsonUInt16(sObj, "battery", bat);
    }

    // Fallback: if payload was a >4KB historical batch dump (tail-sliced) or nested differently,
    // scan for the last occurrence of each metric in the tail string
    if (!hasTemp && !hasHum && !hasCo2) {
        hasTemp = extractLastJsonNumberFromTail(payload, "temperature", temp);
        hasHum  = extractLastJsonNumberFromTail(payload, "humidity", hum);
        float fCo2 = 0, fPm25 = 0, fPm10 = 0, fBat = 0;
        if (extractLastJsonNumberFromTail(payload, "co2", fCo2) && fCo2 >= 0) { co2 = (uint16_t)fCo2; hasCo2 = true; }
        if (extractLastJsonNumberFromTail(payload, "pm25", fPm25) && fPm25 >= 0) { pm25 = (uint16_t)fPm25; hasPm25 = true; }
        if (extractLastJsonNumberFromTail(payload, "pm10", fPm10) && fPm10 >= 0) { pm10 = (uint16_t)fPm10; hasPm10 = true; }
        if (extractLastJsonNumberFromTail(payload, "battery", fBat) && fBat >= 0) { bat = (uint16_t)fBat; hasBat = true; }
    }

    String normMac = normalizeMac(rawMac);

    bool hasAnyMetric = (hasTemp || hasHum || hasCo2 || hasPm25 || hasPm10);

    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    unsigned long now = millis();

    if (normMac.length() == 12) {
        DiscoveredQingping& dq = discoveredQingpings[normMac];
        dq.mac = normMac;
        dq.formattedMac = formatMacColon(normMac);
        if (remoteIp.length() > 0) dq.ip = remoteIp;
        dq.lastSeen = now;
        if (hasTemp) dq.temperature = temp;
        if (hasHum)  dq.humidity = hum;
        if (hasCo2)  dq.co2 = co2;
        if (hasPm25) dq.pm25 = pm25;
        if (hasPm10) dq.pm10 = pm10;
        if (hasBat)  dq.batteryPct = (uint8_t)min((uint16_t)100, bat);
        if (hasAnyMetric) dq.hasMetrics = true;

        // Find if this MAC is bound to one of our rooms
        String targetRoom = getRoomForQingpingMac(normMac);

        // Also check if topic itself specifies a room name (e.g. ventilation/sensor/danylo/external)
        if (targetRoom.length() == 0 && doc["room"].is<const char*>()) {
            targetRoom = String(doc["room"].as<const char*>());
            targetRoom.toLowerCase();
        }

        if (targetRoom.length() > 0 && rooms.find(targetRoom) != rooms.end()) {
            RoomSensorData& room = rooms[targetRoom];
            room.sensorType = "qingping";
            room.mac = dq.formattedMac;
            if (remoteIp.length() > 0) room.ip = remoteIp;
            room.fwVersion = "CGDN1";
            if (hasTemp) room.temperature = temp;
            if (hasHum)  room.humidity = hum;
            if (hasCo2)  room.co2 = co2;
            if (hasPm25) room.pm25 = pm25;
            if (hasPm10) room.pm10 = pm10;
            if (hasBat)  room.batteryPct = dq.batteryPct;

            if (hasAnyMetric || room.isOnline) {
                room.lastSeen = now;
                room.isOnline = true;
            }

            if (hasAnyMetric) {
                Serial.printf("[Qingping] Room [%s] (%s): Temp=%.1f C, Hum=%.1f %%, CO2=%u ppm, PM2.5=%u, PM10=%u, Bat=%u%%\n",
                              targetRoom.c_str(), room.mac.c_str(),
                              room.temperature, room.humidity, room.co2, room.pm25, room.pm10, room.batteryPct);
                if (currentMode == MODE_AUTO) {
                    runAutonomousLogic();
                }
                notifyChange();
            }
        } else if (hasAnyMetric) {
            Serial.printf("[Qingping] Discovered unassigned Qingping [%s] (IP: %s): Temp=%.1f C, Hum=%.1f %%, CO2=%u ppm, PM2.5=%u\n",
                          dq.formattedMac.c_str(), remoteIp.c_str(),
                          dq.temperature, dq.humidity, dq.co2, dq.pm25);
            notifyChange();
        }
        return true;
    }

    // Topic-based room fallback: ventilation/sensor/<room>/external
    if (topic.startsWith("ventilation/sensor/")) {
        int thirdSlash = topic.indexOf('/', 19);
        String room = (thirdSlash > 19) ? topic.substring(19, thirdSlash) : topic.substring(19);
        room.toLowerCase();
        if (rooms.find(room) != rooms.end() && hasAnyMetric) {
            RoomSensorData& r = rooms[room];
            r.sensorType = "qingping";
            if (hasTemp) r.temperature = temp;
            if (hasHum)  r.humidity = hum;
            if (hasCo2)  r.co2 = co2;
            if (hasPm25) r.pm25 = pm25;
            if (hasPm10) r.pm10 = pm10;
            if (hasBat)  r.batteryPct = (uint8_t)min((uint16_t)100, bat);
            r.lastSeen = now;
            r.isOnline = true;
            if (currentMode == MODE_AUTO) {
                runAutonomousLogic();
            }
            notifyChange();
            return true;
        }
    }

    return false;
}

void VentilationController::onExternalRoomReadings(const String& roomId, float temp, float hum, uint16_t co2,
                                                   uint16_t pm25, uint16_t pm10, uint8_t batPct,
                                                   const String& sourceType, const String& mac, const String& ip) {
    String rId = roomId;
    rId.toLowerCase();
    rId.trim();

    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    if (rooms.find(rId) == rooms.end()) return;

    RoomSensorData& r = rooms[rId];
    r.sensorType = sourceType;
    r.temperature = temp;
    r.humidity = hum;
    r.co2 = co2;
    r.pm25 = pm25;
    r.pm10 = pm10;
    r.batteryPct = batPct;
    if (mac.length() > 0) r.mac = formatMacColon(mac);
    if (ip.length() > 0) r.ip = ip;
    r.lastSeen = millis();
    r.isOnline = true;

    if (currentMode == MODE_AUTO) {
        runAutonomousLogic();
    }
    notifyChange();
}

const std::map<String, RoomSensorData>& VentilationController::getRooms() const {
    return rooms;
}

std::map<String, RoomSensorData> VentilationController::getRoomsSnapshot() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    return rooms;
}

const RoomSensorData* VentilationController::getRoom(const String& roomId) const {
    auto it = rooms.find(roomId);
    if (it != rooms.end()) {
        return &(it->second);
    }
    return nullptr;
}

void VentilationController::runAutonomousLogic() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    int demandingRoomsCount = 0;

    for (auto& pair : rooms) {
        RoomSensorData& room = pair.second;

        // Skip offline sensors for automatic triggering
        if (!room.isOnline) continue;

        // Hysteresis logic:
        // Turn on if humidity exceeds high threshold
        // Turn off only if humidity drops below target threshold
        if (room.humidity >= humidityHighThreshold) {
            demandingRoomsCount++;
            setValveState(room.id, true);
        } else if (room.humidity <= humidityTargetThreshold) {
            setValveState(room.id, false);
        } else {
            // Keep existing valve state in between
            if (getValveState(room.id)) {
                demandingRoomsCount++;
            }
        }
    }

    // Determine Fan Speed based on ventilation demand
    FanSpeed desiredSpeed = FAN_OFF;
    if (demandingRoomsCount == 0) {
        desiredSpeed = FAN_SLOW;
    } else if (demandingRoomsCount == 1) {
        desiredSpeed = FAN_MEDIUM;
    } else {
        desiredSpeed = FAN_HIGH;
    }

    // If all valves are closed, don't run the fan against closed dampers
    if (!isAnyValveOpen()) {
        desiredSpeed = FAN_OFF;
    }

    setFanSpeed(desiredSpeed);
}

void VentilationController::update() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    unsigned long now = millis();

    // Check pending delayed fan activation (waiting for valves to open)
    if (pendingFanSpeed != FAN_OFF && now >= fanTurnOnAt) {
        Serial.printf("[VentilationController] 2s valve delay elapsed. Energizing fan at speed %d.\n", (int)pendingFanSpeed);
        RelayController::instance().setFanSpeed(pendingFanSpeed);
        pendingFanSpeed = FAN_OFF;
        notifyChange();
    }

    // Check sensor offline timeouts every 5 seconds
    if (now - lastStatusCheck >= 5000) {
        lastStatusCheck = now;
        bool changed = false;

        for (auto& pair : rooms) {
            RoomSensorData& room = pair.second;
            unsigned long timeoutMs = (room.sensorType == "qingping") ? 1200000UL : SENSOR_OFFLINE_TIMEOUT_MS;
            bool tcpAlive = (room.sensorType == "qingping" && room.assignedMac.length() == 12 &&
                             QingpingBroker::instance().isMacConnected(room.assignedMac));
            if (room.isOnline && !tcpAlive && (now - room.lastSeen > timeoutMs)) {
                room.isOnline = false;
                changed = true;
                Serial.printf("[VentilationController] Room sensor [%s] went OFFLINE\n", room.id.c_str());
            }
        }

        if (changed) {
            if (currentMode == MODE_AUTO) {
                runAutonomousLogic();
            }
            notifyChange();
        }
    }

    // In auto mode, evaluate logic periodically (every 10s) even without packet arrivals
    if (currentMode == MODE_AUTO && (now - lastLogicRun >= 10000)) {
        lastLogicRun = now;
        runAutonomousLogic();
    }
}
