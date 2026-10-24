# VentMaster: ESP32 Autonomous Smart Ventilation Controller

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Core%206.2+-orange.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino%20ESP32-blue.svg)](https://github.com/espressif/arduino-esp32)
[![MQTT](https://img.shields.io/badge/Home%20Assistant-MQTT%20Auto--Discovery-green.svg)](https://www.home-assistant.io/)
[![Version](https://img.shields.io/badge/Version-0.0.1-brightgreen.svg)](CHANGELOG.md)

**VentMaster** is a resilient, autonomous multi-zone demand-controlled ventilation (DCV) firmware built for the **ESP32-WROOM-32**. It orchestrates a multi-speed central fan and motorized room air dampers (valves) for:
- **Danylo Room**
- **Anastasiia Room**
- **Parents Room**

The system operates **100% autonomously** using direct **ESP-NOW** wireless sensor telemetry, offers a modern responsive **Web Dashboard** with **Over-The-Air (OTA) updates**, and seamlessly integrates into **Home Assistant** via MQTT Auto-Discovery.

---

## Table of Contents
- [Hardware Architecture & Pinout](#hardware-architecture--pinout)
- [Ventilation & Damper Control Logic](#ventilation--damper-control-logic)
- [Wi-Fi & Networking Logic](#wi-fi--networking-logic)
- [Web Interface & REST API](#web-interface--rest-api)
- [Home Assistant MQTT Integration](#home-assistant-mqtt-integration)
- [Wireless Room Sensors (ESP-NOW)](#wireless-room-sensors-esp-now)
- [Building & Versioning](#building--versioning)
- [Flashing Firmware](#flashing-firmware)

---

## Hardware Architecture & Pinout

Configured in [`include/config.h`](include/config.h):

| Function | ESP32 GPIO | Active Level | Description / Protection |
| :--- | :--- | :--- | :--- |
| **Fan Speed 1 (Slow)** | `GPIO 13` | `LOW` | Interlocked speed tap relay |
| **Fan Speed 2 (Medium)** | `GPIO 14` | `LOW` | Interlocked speed tap relay |
| **Fan Speed 3 (High)** | `GPIO 26` | `LOW` | Interlocked speed tap relay |
| **Valve: Danylo Room** | `GPIO 27` | `LOW` | Motorized 220V/24V damper relay |
| **Valve: Anastasiia Room** | `GPIO 32` | `LOW` | Motorized 220V/24V damper relay |
| **Valve: Parents Room** | `GPIO 33` | `LOW` | Motorized 220V/24V damper relay |

### Hardware Safety Protections
1. **Speed Coil Interlocking**: Changing speeds always de-energizes all speed relays first, waits a hardware deadtime of **100 ms** (`FAN_SWITCH_DEADTIME_MS`), and only then energizes the target relay. This eliminates contact arcing and prevents motor winding burnouts.
2. **Active LOW Optocoupler Compatibility**: By default, `#define RELAY_ACTIVE_STATE LOW` matches standard 5V/12V optocoupled relay modules.

---

## Ventilation & Damper Control Logic

### 1. Manual Mode
- **Cold-Start Sequence (All Valves Closed)**:
  When the fan is turned on (`SLOW`, `MED`, or `HIGH`) and all room valves are currently closed:
  1. The controller immediately opens all 3 room valves (`Danylo`, `Anastasiia`, `Parents`).
  2. It enters a non-blocking **2-second delay** (`VALVE_OPENING_DELAY_MS = 2000`) allowing motorized actuators to physically open their dampers.
  3. Once the 2 seconds elapse, the fan speed relay is energized.
- **Preserved Airflow (1 or 2 Valves Already Open)**:
  If one or more valves are already open when turning the fan on, existing valve positions are preserved and the fan starts immediately without waiting.
- **Auto-Close on Fan Stop**:
  When fan speed is set to `OFF`, all 3 room valves are automatically closed.
- **Deadheading Protection**:
  If the fan is running and you manually close the last open valve, the controller immediately turns the fan `OFF` to prevent pressure buildup and motor strain.

### 2. Automatic Mode (Autonomous DCV)
- Periodically evaluates wireless room telemetry using hysteresis:
  - **Humidity High Threshold** (`>= 65% RH`): Room valve opens, demand counter increments.
  - **Humidity Target Threshold** (`<= 55% RH`): Room valve closes once air is refreshed.
- **Fan Speed Arbitration**:
  - `0 demanding rooms`: Baseline low speed or off.
  - `1 demanding room`: Medium speed.
  - `2+ demanding rooms`: High speed.
- If all valves are closed, the fan remains `OFF`.

---

## Wi-Fi & Networking Logic

### 1. Dual-Mode Coexistence
The ESP32 boots in dual mode (`WIFI_AP_STA`) to ensure:
- Direct wireless connectivity for setup.
- Simultaneous **ESP-NOW** packet reception on channel `1` (`ESPNOW_CHANNEL`).

### 2. SoftAP 5-Minute Auto-Disable
- On power-up, the fallback access point `ESP32-Ventilation-AP` is active at `http://192.168.4.1`.
- Once connected to your home Wi-Fi station (`Ridna Hata IOT`), a **5-minute timer** (`SOFTAP_TIMEOUT_MS`) begins.
- After 5 minutes, the SoftAP is gracefully turned off, switching the radio to Station-only mode to eliminate 2.4GHz RF interference and reduce power.
- **Fail-Safe**: If your home Wi-Fi is unreachable or down, the SoftAP stays enabled indefinitely so you are never locked out.
- The web interface displays a live countdown: `AP Active (4:32)` &rarr; `AP Disabled (Wi-Fi Station only)`.

---

## Web Interface & REST API

Served entirely from ESP32 Flash memory (`include/web_assets.h`):

### UI Features
- **Fan Speed Selector**: Segmented controls for `OFF`, `SLOW`, `MED`, and `HIGH`.
- **Air Damper Valves**: Interactive toggle switches for each room.
- **Room Sensor Cards**: Real-time temperature (°C), humidity (% RH), battery voltage, and online/offline status indicator.
- **Mode Badge**: Clickable toggle between `MANUAL` and `AUTO`.
- **Options & Administration Section**:
  - Collapsible/expandable section keeping the main dashboard clean.
  - **Firmware Update (OTA)**: Shows installed version, build timestamp, active partition slot (`ota_0`/`ota_1`), and file uploader with live progress bar.
  - **System Reboot**: Graceful software restart button.

### REST Endpoints
| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/` | Web dashboard UI |
| `GET` | `/api/status` | JSON state (fan speed, valves, sensors, mode, uptime, firmware version, partition) |
| `POST` | `/api/speed?val=0..3` | Set fan speed (`0=OFF`, `1=SLOW`, `2=MED`, `3=HIGH`) |
| `POST` | `/api/valve?room=danylo&state=0\|1` | Toggle individual room valve |
| `POST` | `/api/mode?val=manual\|auto` | Switch operating mode |
| `POST` | `/api/restart` | Software reboot the ESP32 controller |
| `POST` | `/update?size=<bytes>` | Multipart OTA firmware file upload |


---

## Home Assistant MQTT Integration

Built-in **MQTT Auto-Discovery** automatically configures Home Assistant without editing `configuration.yaml`:

### Discovered Entities
- **Select**: `select.ventilation_speed` (`OFF`, `SLOW`, `MEDIUM`, `HIGH`)
- **Switch**: `switch.ventilation_auto_mode`
- **Valves**:
  - `switch.valve_danylo`
  - `switch.valve_anastasiia`
  - `switch.valve_parents`
- **Sensors**:
  - `sensor.temp_danylo`, `sensor.hum_danylo`
  - `sensor.temp_anastasiia`, `sensor.hum_anastasiia`
  - `sensor.temp_parents`, `sensor.hum_parents`

### Configuration (`include/config.h`)
```cpp
#define MQTT_ENABLED        true
#define MQTT_BROKER_HOST    "192.168.51.100"       // Home Assistant IP
#define MQTT_BROKER_PORT    1883
#define MQTT_USERNAME       "mqtt-user"
#define MQTT_PASSWORD       "YourPassword"
#define MQTT_CLIENT_ID      "esp32_ventilation_controller"
```

---

## Wireless Room Sensors (ESP-NOW)

Remote sensor nodes transmit directly to the main controller using **ESP-NOW** without router reliance.

- **Dedicated Project**: [`sensor_node_example/`](sensor_node_example/) (with [`platformio.ini`](sensor_node_example/platformio.ini) and [`src/main.cpp`](sensor_node_example/src/main.cpp)).
- **Hardware Supported**: **ESP32-C3 Super Mini** or standard ESP32.
- **Sensors Configured**:
  - **Sensirion SCD40**: I2C CO2 (ppm), Temperature (°C), Humidity (% RH) on **SDA: GPIO 8**, **SCL: GPIO 9**.
  - **Cubic PM1006K**: UART PM2.5 (µg/m³) on **RX: GPIO 20** (9600 baud, 8N1).
- **Room Assignment**: Change `#define ROOM_ID "parents"` (or `"danylo"`, `"anastasiia"`).
- **Wi-Fi Channel**: Set `#define ESPNOW_CHANNEL 6` matching the 2.4 GHz channel of your router.
- **Flashing**:
  ```powershell
  pio run -d sensor_node_example -t upload --upload-port COM5
  ```

---

## Building & Versioning

The project includes an automated PowerShell build script: [`build.ps1`](build.ps1).

### Running a Build
```powershell
# Build with current version from config.h (0.0.1)
.\build.ps1

# Or specify an explicit version
.\build.ps1 -Version "0.0.2"
```

### Build Artifacts
Binaries are exported to the `builds/` directory:
- `builds/ventmaster_<version>_<yyyyMMdd_HHmmss>.bin` *(e.g. `ventmaster_0.0.1_20261017_103516.bin`)*
- `builds/ventmaster_latest.bin` *(symlinked/copied for instant OTA flashing)*

---

## Flashing Firmware

### 1. Wireless Web OTA (Recommended)
1. Build the firmware using `.\build.ps1`.
2. Open the web interface at `http://192.168.51.147` (or `http://192.168.4.1`).
3. Scroll to the **Firmware Update (OTA)** card.
4. Click **Select .bin File** and choose `builds/ventmaster_latest.bin`.
5. Click **Upload & Flash**. The device flashes, reboots, and refreshes automatically!

### 2. USB Flashing
Connect via USB cable and run:
```powershell
pio run -t upload
pio device monitor
```
