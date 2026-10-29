# Changelog

All notable changes to the **ESP32 Autonomous Ventilation Controller (VentMaster)** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.0.2] - 2026-10-27

### Added
- **Direct Qingping Air Monitor Lite (`CGDN1`) Wi-Fi Integration (Zero HA Dependency)**:
  - Embedded non-blocking LwIP MQTT broker (`QingpingBroker`) listening on TCP port `1883` (`src/qingping_broker.cpp`).
  - Automatic 15-second live-stream configuration push (`type "17"` and `type "12"`) to `qingping/<MAC>/down` on `SUBSCRIBE` and periodic `PINGREQ` renewal.
  - Supports both JSON (`sensorData` / `data`) and binary TLV (`0x43 0x47`) payloads, plus streaming discard for `>2 KB` historical backlogs.
  - Persistent per-room MAC bindings in NVS (`qp_danylo`, `qp_anastasiia`, `qp_parents`, `qp_gym`) with auto-discovery and 1-click binding under the **Administration** tab.
- **4th Room Support (`Gym Room` / `gym`) & Home Assistant Enhancements**:
  - Renamed 4th zone from `gim` to `gym` (`GPIO 25`), with automatic MQTT discovery cleanup of legacy `gim` topics.
  - Added `<Room> Last Update` (`s`) diagnostic sensor entities in Home Assistant for all 4 rooms.
  - Always display `CO₂ Level` and `PM2.5` (including `0 µg/m³` in clean air) on all dashboard room cards and always publish `pm25` to Home Assistant when online.
- **Firmware Target Identity Verification (`VENT_FW_DESC_V1`)**:
  - Embedded 64-byte `VentFwTargetDesc` header at offset `288` in Master (`master:master`, chip `0x0000`) and Slave (`slave:<room>`, chip `0x0005`) binaries to prevent cross-flashing modules during Web OTA.
- **Upgraded IKEA VINDRIKTNING + ESP32-C3 Slave Firmware (`sensor_node_example`)**:
  - Fast-boot initialization (`initScd40()` at `t = 0.15s` + immediate ESP-NOW broadcast on first SCD40 measurement) and 10-minute Master timeout (`SENSOR_OFFLINE_TIMEOUT_MS = 600000`) to stay online even when powered from the IKEA switched fan pads.
  - Background Wi-Fi auto-reconnect (`maintainWiFi()`) and multi-channel ESP-NOW scanning (`channels 1..13`) when Wi-Fi is disconnected.
  - Zero-config automatic I2C detection (`SDA = GPIO 8`, `SCL = GPIO 9`) for external temperature & humidity sensors (`SHT4x` at `0x44`/`0x45`, `SHT3x` at `0x44`/`0x45`, `AHT20/21` at `0x38`), plus `6.0°C` SCD40 self-heating offset compensation.

---

## [0.0.1] - 2026-10-17

### Added
- **Hardware Fan Speed Control**:
  - Multi-relay control for Fan Speeds: `OFF`, `SLOW` (GPIO 13), `MEDIUM` (GPIO 14), and `HIGH` (GPIO 26) with `100ms` safety deadtime interlock.
- **Motorized Air Damper Control**:
  - Independent relays for **Danylo Room** (GPIO 27), **Anastasiia Room** (GPIO 32), **Parents Room** (GPIO 33), and **Gym Room** (GPIO 25) with 2-second valve opening delay and closed-duct safety interlock.
- **Wireless ESP-NOW Sensor Telemetry**:
  - Direct ESP-NOW receiver on the Main Controller and dedicated PlatformIO project for **ESP32-C3 Super Mini** sensor nodes (`SCD40` + `PM1006K`).
- **Web Dashboard & Home Assistant MQTT Auto-Discovery**:
  - Self-hosted responsive web application (`include/web_assets.h`), progressive Web OTA (`/update`), SoftAP 5-minute auto-disable, and automated build script (`build.ps1`).
