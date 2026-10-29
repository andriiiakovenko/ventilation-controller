# Ventilation Controller — Project Roadmap & TODO List

## Phase 1: Core Autonomous Ventilation System (Current Focus)
- [x] **Master Controller (ESP32-WROOM-32)**
  - [x] 3-speed fan control (`Slow`, `Medium`, `High`, `Off`) with 100 ms safety deadtime and 2 s motorized valve opening delay.
  - [x] 4 motorized air valve relays (`Danylo`, `Anastasiia`, `Parents`, `Gym`) with safety interlock (never run fan with all valves closed).
  - [x] Web Dashboard, REST API, and guarded Web OTA (`/update`) with binary target descriptor verification (`VENT_FW_DESC_V1`).
  - [x] Home Assistant MQTT Auto-Discovery + `<Room> Last Update` telemetry.
  - [x] Embedded LwIP non-blocking TCP port `1883` MQTT broker (`QingpingBroker`) for direct Wi-Fi sensor connections without Home Assistant dependency.
- [ ] **Direct Qingping Air Monitor Lite (`CGDN1`) Room Sensors**
  - [x] **Anastasiia Room** (*Kontor Air Monitor*, MAC `CC:B5:D1:32:1C:16`, IP `192.168.51.231`) — Connected directly to Master on port `1883` and reporting every ~10–15 s.
  - [ ] **Danylo Room** (*Stue Air Monitor*, MAC `CC:B5:D1:32:21:0E`) — Point device to Master broker (`192.168.51.147:1883`) and verify live telemetry.
  - [ ] **Parents Room** (*Foreldre Air Monitor*, MAC `CC:B5:D1:32:23:41`) — Point device to Master broker (`192.168.51.147:1883`) and verify live telemetry.
- [ ] **Upgraded IKEA VINDRIKTNING + ESP32-C3 Super Mini + SCD40 Sensors (ESP-NOW)**
  - [x] **Gym Room (`gym`)** (`MAC: 64:E8:33:BB:3E:EC`, IP `192.168.51.122`) — Running fast-boot `slave:gym` firmware with auto-reconnect and multi-channel ESP-NOW broadcast.
  - [ ] **Hardware Mod on Gym Room Sensor**:
    - [ ] Move ESP32-C3 `GND` wire from the switched fan `-` pad to the continuous `GND` test pad on the IKEA PCB so the ESP32-C3 stays powered 24/7 even when the internal fan pauses.
    - [ ] Solder external I2C temperature & humidity sensor (`SHT4x` / `SHT3x` / `AHT20`) to `SDA = GPIO 8`, `SCL = GPIO 9` (firmware already auto-detects it on boot).
  - [ ] **Upgrade Remaining 2 IKEA VINDRIKTNING Units**:
    - [ ] Assemble ESP32-C3 + SCD40 + external I2C Temp/Hum sensor into the other 2 IKEA enclosures (using continuous `+5V`/`GND` pads) and flash `builds/ventslave_<room>_latest.bin`.

---

## Phase 2: Subproject — Bathroom Zigbee Temperature & Humidity Sensors (`ywagc4rj`)
Integrate the installed **Tuya Zigbee Temperature & Humidity Sensors (`ywagc4rj`)** from the bathrooms into the Ventilation Controller once Phase 1 is complete:
- [ ] **1. Choose & Configure Zigbee Transport to Master ESP32**:
  - **Option A (Zero New Hardware)**: Subscribe Master ESP32 to `zigbee2mqtt/<bathroom_sensor>` topics on local MQTT broker (`192.168.51.100:1883`) or forward from Home Assistant ZHA to `POST http://192.168.51.147/api/sensor/external` / MQTT `ventilation/sensor/<bathroom>/external`.
  - **Option B (100% HA-Independent Local Zigbee Bridge)**: Point a standalone LAN/Wi-Fi Zigbee coordinator (e.g. SLZB-06 / Sonoff ZBBridge with Tasmota) directly at the Master ESP32's embedded MQTT broker (`192.168.51.147:1883`).
  - **Option C (Direct Hardware Receiver)**: Add a local Zigbee receiver (e.g. ESP32-C6/H2 or CC2652 over UART/ESP-NOW) or custom BLE firmware (if Telink TLSR8258-based) so the Master ESP32 receives bathroom sensor packets directly over the air.
- [ ] **2. Dashboard & Telemetry Integration**:
  - [ ] Add Bathroom sensor card(s) to the Web Dashboard and `/api/status` JSON.
  - [ ] Expose Bathroom sensor status via Home Assistant MQTT Auto-Discovery.
- [ ] **3. Bathroom Shower / Humidity Boost Logic**:
  - [ ] Define automatic fan boost behavior when bathroom humidity spikes (e.g. above high humidity threshold during showers) and how valves should be positioned during bathroom exhaust boost.
