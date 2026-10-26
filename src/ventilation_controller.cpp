#include "ventilation_controller.h"

VentilationController& VentilationController::instance() {
    static VentilationController _inst;
    return _inst;
}

VentilationController::VentilationController() {}

void VentilationController::init() {
    // Initialize the default 3 monitored rooms
    RoomSensorData danylo;
    danylo.id = "danylo";
    danylo.friendlyName = "Danylo Room";
    rooms[danylo.id] = danylo;

    RoomSensorData anastasiia;
    anastasiia.id = "anastasiia";
    anastasiia.friendlyName = "Anastasiia Room";
    rooms[anastasiia.id] = anastasiia;

    RoomSensorData parents;
    parents.id = "parents";
    parents.friendlyName = "Parents Room";
    rooms[parents.id] = parents;

    RoomSensorData gym;
    gym.id = "gym";
    gym.friendlyName = "Gym Room";
    rooms[gym.id] = gym;

    // Start with default MANUAL mode, FAN OFF, valves CLOSED
    currentMode = MODE_MANUAL;
    pendingFanSpeed = FAN_OFF;
    RelayController::instance().setFanSpeed(FAN_OFF);
    RelayController::instance().setDanyloValve(false);
    RelayController::instance().setAnastasiiaValve(false);
    RelayController::instance().setParentsValve(false);
    RelayController::instance().setGymValve(false);

    Serial.println("[VentilationController] Controller initialized with 4 rooms (including Gym).");
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
        // Cancel any pending start
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

    // Turning fan ON (SLOW, MEDIUM, or HIGH)
    bool anyOpen = isAnyValveOpen();

    if (anyOpen) {
        // If at least one valve is already open, just switch fan speed immediately
        pendingFanSpeed = FAN_OFF;
        RelayController::instance().setFanSpeed(speed);
        Serial.printf("[VentilationController] Fan speed set to %d (existing valves kept).\n", (int)speed);
        notifyChange();
    } else {
        // All valves are closed:
        // Open all valves, wait 2 seconds delay, and then turn on fan
        Serial.println("[VentilationController] All valves closed. Opening all valves and waiting 2s delay...");
        RelayController::instance().setDanyloValve(true);
        RelayController::instance().setAnastasiiaValve(true);
        RelayController::instance().setParentsValve(true);
        RelayController::instance().setGymValve(true);
        for (auto& pair : rooms) {
            pair.second.valveState = true;
        }

        pendingFanSpeed = speed;
        fanTurnOnAt = millis() + VALVE_OPENING_DELAY_MS;
        notifyChange();
    }
}

bool VentilationController::getValveState(const String& roomId) const {
    return RelayController::instance().getValve(roomId);
}

void VentilationController::setValveState(const String& roomId, bool open) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    RelayController::instance().setValve(roomId, open);
    if (rooms.find(roomId) != rooms.end()) {
        rooms[roomId].valveState = open;
    }

    // Safety: If the last open valve was closed while fan is running or pending, shut fan off
    if (!open && !isAnyValveOpen()) {
        if (getFanSpeed() != FAN_OFF) {
            Serial.println("[VentilationController] All valves closed. Automatically turning Fan OFF.");
            pendingFanSpeed = FAN_OFF;
            RelayController::instance().setFanSpeed(FAN_OFF);
        }
    }

    notifyChange();
}

void VentilationController::onSensorPacket(const ESPNowSensorPacket& packet, const uint8_t* mac) {
    String rId = String(packet.roomId);
    rId.toLowerCase();
    rId.trim();

    std::lock_guard<std::recursive_mutex> lock(stateMutex);
    if (rooms.find(rId) == rooms.end()) {
        // Unknown room ID, register dynamically
        RoomSensorData newRoom;
        newRoom.id = rId;
        newRoom.friendlyName = rId;
        rooms[rId] = newRoom;
    }

    RoomSensorData& room = rooms[rId];
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

    Serial.printf("[ESP-NOW] Sensor [%s]: Temp=%.1f C, Hum=%.1f %%, CO2=%u ppm, PM2.5=%u ug/m3, Bat=%.2f V, FW=%s, IP=%s\n",
                  rId.c_str(), room.temperature, room.humidity, room.co2, room.pm25, room.batteryVoltage,
                  room.fwVersion.c_str(), room.ip.c_str());

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
    unsigned long now = millis();

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
        // 2 or more rooms need ventilation
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
            if (room.isOnline && (now - room.lastSeen > SENSOR_OFFLINE_TIMEOUT_MS)) {
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
