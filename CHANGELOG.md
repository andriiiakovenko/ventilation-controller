# Changelog

All notable changes to the **ESP32 Autonomous Ventilation Controller (VentMaster)** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.0.1] - 2026-10-17

### Added
- **Hardware Fan Speed Control**:
  - Multi-relay control for Fan Speeds: `OFF`, `SLOW` (GPIO 13), `MEDIUM` (GPIO 14), and `HIGH` (GPIO 26).
  - Hardware safety deadtime interlock (`FAN_SWITCH_DEADTIME_MS = 100ms`) ensuring speed taps never energize simultaneously.
- **Motorized Air Damper Control**:
  - Independent relays for **Danylo Room** (GPIO 27), **Anastasiia Room** (GPIO 32), and **Parents Room** (GPIO 33).
- **Intelligent Airflow & Damper Logic**:
  - **Cold-Start Damper Sequence**: Turning the fan on when all valves are closed automatically opens all 3 dampers, waits a non-blocking **2-second delay** (`VALVE_OPENING_DELAY_MS`) for motorized dampers to mechanically open, and then starts the fan.
  - **Preserved Airflow**: If at least one damper is already open when starting the fan, existing damper states are preserved and the fan starts immediately.
  - **Auto-Close on Fan Stop**: Turning the fan OFF automatically closes all room dampers.
  - **Deadheading Protection**: Closing the last remaining open damper automatically shuts down the fan to prevent pressure buildup.
- **Wireless ESP-NOW Sensor Telemetry**:
  - Direct ESP-NOW receiver on the Main Controller listening on the active Wi-Fi channel.
  - In-memory telemetry storage with 3-minute offline timeout tracking.
  - Dedicated PlatformIO project for **ESP32-C3 Super Mini** sensor node ([sensor_node_example/](sensor_node_example/)):
    - Native USB CDC serial output (`-DARDUINO_USB_CDC_ON_BOOT=1`).
    - Configured for **Parents Room** (`"parents"`).
    - **Sensirion SCD40 Support**: Photoacoustic CO2 (ppm), precision temperature (°C), and relative humidity (% RH) over I2C (SDA=GPIO8, SCL=GPIO9).
    - **Cubic PM1006K Support**: Optical particulate matter PM2.5 (µg/m³) parser with 20-byte frame checksum validation on UART1 RX (GPIO20).
    - Channel-aligned broadcast to router channel (Channel 6) matching the Main Controller.
    - Deep sleep support for battery operation.
  - Full end-to-end telemetry pipeline supporting Temperature, Humidity, CO2 (ppm), and PM2.5 (µg/m³).
- **Glassmorphism Web Dashboard**:
  - Self-hosted responsive web application served directly from ESP32 Flash memory (`include/web_assets.h`).
  - Interactive manual speed selector, independent damper toggles, and live room sensor cards.
  - Dynamic Air Quality badges for CO2 (ppm) and PM2.5 (µg/m³) with color-coded safety thresholds (Good / Moderate / Alert).
- **Home Assistant MQTT Auto-Discovery**:
  - Automatic entity registration for Mosquitto broker:
    - `select.ventilation_speed` (`OFF`, `SLOW`, `MEDIUM`, `HIGH`)
    - `switch.ventilation_auto_mode`
    - `switch.valve_danylo`, `switch.valve_anastasiia`, `switch.valve_parents`
    - `sensor.*_temperature` and `sensor.*_humidity`
    - `sensor.*_co2` (Carbon Dioxide in ppm)
    - `sensor.*_pm25` (Particulate Matter 2.5 in µg/m³)
  - Mode switcher (`MANUAL` / `AUTO`).
  - **Options & Administration Section**:
    - Collapsible section grouping administrative functions away from daily controls.
    - Embedded **Firmware Update (OTA)** tool with active slot identification (`ota_0`/`ota_1`), build timestamp, and progress bar.
    - **WiFi-Safe Progressive OTA**: Uses `UPDATE_SIZE_UNKNOWN` to erase flash sectors one-at-a-time during writes (~30ms each) instead of blocking upfront erase (3-7s) that freezes WiFi and drops TCP connections.
    - JSON error responses from OTA endpoint with `Update.printError()` details surfaced in the Web UI.
    - Visual OTA status feedback: ✅/❌ icons, green/red progress bar coloring, and ⏳ spinner during flash write phase.
    - **System Reboot** button with graceful software restart (`/api/restart`).

- **Wi-Fi & SoftAP Management**:
  - Fallback SoftAP (`ESP32-Ventilation-AP`) on IP `192.168.4.1`.
  - **5-Minute Auto-Timeout**: SoftAP automatically turns off after 5 minutes once connected to home Wi-Fi station to eliminate RF clutter, with live countdown badge in Web UI.
- **Home Assistant MQTT Auto-Discovery**:
  - Automatic entity registration for Mosquitto broker:
    - `select.ventilation_speed` (`OFF`, `SLOW`, `MEDIUM`, `HIGH`)
    - `switch.ventilation_auto_mode`
    - `switch.valve_danylo`, `switch.valve_anastasiia`, `switch.valve_parents`
    - `sensor.*_temperature` and `sensor.*_humidity` for all rooms.
- **Build & Release Automation**:
  - PowerShell build script ([build.ps1](build.ps1)) generating timestamped release artifacts: `ventmaster_<version>_<yyyyMMdd_HHmmss>.bin`.
