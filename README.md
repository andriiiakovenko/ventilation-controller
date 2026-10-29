# VentMaster: ESP32 Autonomous Smart Ventilation Controller

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Core%206.2+-orange.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino%20ESP32-blue.svg)](https://github.com/espressif/arduino-esp32)
[![MQTT](https://img.shields.io/badge/Home%20Assistant-MQTT%20Auto--Discovery-green.svg)](https://www.home-assistant.io/)
[![Version](https://img.shields.io/badge/Version-0.0.1-brightgreen.svg)](CHANGELOG.md)

**VentMaster** is a resilient, autonomous multi-zone demand-controlled ventilation (DCV) firmware built for the **ESP32-WROOM-32**. It orchestrates a multi-speed central fan and **4 motorized room air dampers (valves)** across:

| Room ID | Room Name | Sensor Type | Device / Label | Default / Bound MAC |
| :--- | :--- | :--- | :--- | :--- |
| `danylo` | **Danylo Room** | Direct Qingping Wi-Fi (`CGDN1`) | *Stue Air Monitor (Qingping)* | `CC:B5:D1:32:21:0E` |
| `anastasiia` | **Anastasiia Room** | Direct Qingping Wi-Fi (`CGDN1`) | *Kontor Air Monitor (Qingping)* | `CC:B5:D1:32:1C:16` |
| `parents` | **Parents Room** | Direct Qingping Wi-Fi (`CGDN1`) | *Foreldre Air Monitor (Qingping)* | `CC:B5:D1:32:23:41` |
| `gym` | **Gym Room** | ESP-NOW Wireless Node | *IKEA VINDRIKTNING + ESP32-C3 (ESP-NOW)* | `64:E8:33:BB:3E:EC` |

The system operates **100% autonomously** with **zero dependency on Home Assistant as a single point of failure**:
- **Direct Qingping Wi-Fi Integration**: Commercial **Qingping Air Monitor Lite (`CGDN1`)** units connect directly over local Wi-Fi to an embedded LwIP non-blocking MQTT broker (`TCP port 1883`) running inside the Master ESP32.
- **Direct ESP-NOW Wireless Nodes**: Upgraded **IKEA VINDRIKTNING** enclosures with **ESP32-C3 Super Mini + Sensirion SCD40 + Cubic PM1006K + optional external I2C Temp/Hum sensor** broadcast directly to the Master ESP32.
- **Responsive Web Dashboard & Guarded OTA**: Built-in web UI (`Dashboard` and `Administration` tabs) with role- and room-verified Over-The-Air (OTA) firmware updates.
- **Home Assistant MQTT Auto-Discovery**: Publishes all controls, valves, room metrics, and `<Room> Last Update` timestamps to Home Assistant when available.

---

## Table of Contents
- [Hardware Architecture & Pinout](#hardware-architecture--pinout)
- [Ventilation & Damper Control Logic](#ventilation--damper-control-logic)
- [Wi-Fi & Networking Logic](#wi-fi--networking-logic)
- [Direct Qingping Air Monitor Lite (CGDN1) Integration](#direct-qingping-air-monitor-lite-cgdn1-integration)
  - [Overview & How It Works](#overview--how-it-works)
  - [Configured Qingping Devices](#configured-qingping-devices)
  - [Step 1: Create Access Configuration on developer.qingping.co](#step-1-create-access-configuration-on-developerqingpingco)
  - [Step 2: Bind Device to Configuration on developer.qingping.co/private/device-binding](#step-2-bind-device-to-configuration-on-developerqingpingcoprivatedevice-binding)
  - [Step 3: Bind & Verify on the VentMaster Web Dashboard](#step-3-bind--verify-on-the-ventmaster-web-dashboard)
  - [Step 4: Reconfiguring, Swapping, or Unbinding Devices](#step-4-reconfiguring-swapping-or-unbinding-devices)
  - [Diagnostics & Live Broker Log](#diagnostics--live-broker-log)
- [Upgraded IKEA VINDRIKTNING + ESP32-C3 Sensor Nodes (ESP-NOW)](#upgraded-ikea-vindriktning--esp32-c3-sensor-nodes-esp-now)
  - [Hardware Wiring & Continuous Power Mod](#hardware-wiring--continuous-power-mod)
  - [Automatic External I2C Temperature & Humidity Sensor Support](#automatic-external-i2c-temperature--humidity-sensor-support)
- [Web Interface & REST API](#web-interface--rest-api)
- [Home Assistant MQTT Integration](#home-assistant-mqtt-integration)
- [Building & Versioning](#building--versioning)
- [Flashing Firmware & Target Verification](#flashing-firmware--target-verification)
- [Project Roadmap & TODO](#project-roadmap--todo)

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
| **Valve: Gym Room** | `GPIO 25` | `LOW` | Motorized 220V/24V damper relay |

### Hardware Safety Protections
1. **Speed Coil Interlocking**: Changing speeds always de-energizes all speed relays first, waits a hardware deadtime of **100 ms** (`FAN_SWITCH_DEADTIME_MS`), and only then energizes the target relay. This eliminates contact arcing and prevents motor winding burnouts.
2. **Motorized Valve Opening Delay**: When starting the fan from `OFF`, the controller waits **2 seconds** (`VALVE_OPENING_DELAY_MS = 2000`) for motorized dampers to mechanically open before energizing the fan motor.
3. **Deadheading / Closed-Duct Interlock**:
   - If the fan is started while all 4 valves are closed, the controller automatically opens the fallback valve (`Gym Room`) before starting the fan.
   - If the fan is running and the last open valve is manually closed, the controller immediately stops the fan (`FAN_OFF`) to protect ductwork and the fan motor.

---

## Ventilation & Damper Control Logic

### 1. Manual Mode
- Full manual control of Fan Speed (`OFF`, `SLOW`, `MED`, `HIGH`) and individual room valves (`Danylo`, `Anastasiia`, `Parents`, `Gym`) via the Web Dashboard, REST API, or Home Assistant.
- Setting fan speed to `OFF` automatically closes all 4 room valves.

### 2. Automatic Mode (Autonomous DCV)
- Evaluates room telemetry across all online rooms (`danylo`, `anastasiia`, `parents`, `gym`) every 10 seconds and on every incoming sensor packet using hysteresis:
  - **Humidity High Threshold** (`>= 65% RH`, configurable via `DEFAULT_HUMIDITY_HIGH_THRESHOLD`): Opens that room's motorized valve and increments the active demand counter.
  - **Humidity Target Threshold** (`<= 55% RH`, configurable via `DEFAULT_HUMIDITY_TARGET_THRESHOLD`): Closes that room's motorized valve once humidity drops back to target.
- **Fan Speed Arbitration**:
  - `0 demanding rooms`: `FAN_SLOW` (if any valve remains open) or `FAN_OFF` (if all valves are closed).
  - `1 demanding room`: `FAN_MEDIUM`.
  - `2+ demanding rooms`: `FAN_HIGH`.

---

## Wi-Fi & Networking Logic

1. **Dual-Mode Coexistence (`WIFI_AP_STA`) with Zero Power-Save Dropouts**:
   - Calls `WiFi.setSleep(false)` both before and after `WiFi.begin(...)` so the ESP32 radio stays awake 100% of the time for both incoming TCP connections on port `1883` and asynchronous ESP-NOW broadcasts.
2. **SoftAP 5-Minute Auto-Disable**:
   - On boot, `ESP32-Ventilation-AP` (`192.168.4.1`) is active for direct access.
   - Once connected to your home Wi-Fi (`Ridna Hata IOT`, e.g. `http://192.168.51.147`), the SoftAP automatically turns off after 5 minutes (`SOFTAP_TIMEOUT_MS`). If home Wi-Fi is unreachable, SoftAP remains active indefinitely.

---

## Direct Qingping Air Monitor Lite (CGDN1) Integration

### Overview & How It Works
Commercial **Qingping Air Monitor Lite (`CGDN1`)** sensors normally report to the Qingping Cloud or a Home Assistant MQTT broker. To eliminate Home Assistant as a single point of failure, **VentMaster embeds its own lightweight, non-blocking LwIP MQTT server on TCP port `1883`** ([`src/qingping_broker.cpp`](src/qingping_broker.cpp)):

1. **Direct Local Wi-Fi Connection**: Each Qingping monitor connects over your home 2.4 GHz Wi-Fi directly to the Master Controller's IP (`192.168.51.147:1883`).
2. **Automatic 15-Second Fast-Report Push**:
   - On standard settings, a Qingping monitor only uploads every 15 minutes.
   - However, the instant a Qingping monitor subscribes to `qingping/<MAC>/down` on VentMaster's embedded broker, VentMaster automatically sends two downlink commands (`type "17"` setting report/sample intervals to `15s` + `type "12"` requesting real-time streaming for `86400s`), and re-sends the streaming renewal on every `PINGREQ` heartbeat.
   - As a result, **your Qingping monitors stream live CO₂, Temperature, Humidity, PM2.5, PM10, and Battery updates every ~10–15 seconds!**
3. **Resilient LwIP Socket Engine**:
   - Handles both JSON (`sensorData` / `data`) and binary TLV (`0x43 0x47` / `"CG"`) payloads.
   - Safely drains and acknowledges (`PUBACK`/`PUBREC`) large historical backlog dumps (`>2 KB`) without running out of RAM.
   - Keeps rooms marked `ONLINE` as long as their TCP socket is active (`isMacConnected`) or a packet arrived within the last 20 minutes.

---

### Configured Qingping Devices

| Room in VentMaster | Original HA Name | Model | MAC Address (Colon) | MAC Address (Plain Hex) | Topic Prefix on Port 1883 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Danylo Room** (`danylo`) | *Stue Air Monitor* | CGDN1 | `CC:B5:D1:32:21:0E` | `CCB5D132210E` | `qingping/CC-B5-D1-32-21-0E/` |
| **Anastasiia Room** (`anastasiia`) | *Kontor Air Monitor* | CGDN1 | `CC:B5:D1:32:1C:16` | `CCB5D1321C16` | `qingping/CC-B5-D1-32-1C-16/` |
| **Parents Room** (`parents`) | *Foreldre Air Monitor* | CGDN1 | `CC:B5:D1:32:23:41` | `CCB5D1322341` | `qingping/CC-B5-D1-32-23-41/` |

> **Important Note on MAC Format in Topics**:
> Whether your Qingping configuration uses **hyphens** (`qingping/CC-B5-D1-32-1C-16/up`), **colons** (`qingping/CC:B5:D1:32:1C:16/up`), or **plain hex** (`qingping/CCB5D1321C16/up`), VentMaster automatically normalizes the MAC address and replies on the exact topic the device subscribed to.

---

### Step 1: Create Access Configuration on `developer.qingping.co`

Before binding a device, create a **Self-built MQTT** configuration profile in the Qingping Developer Portal (you can create one profile per sensor or a shared profile):

1. Log in to the Qingping Developer Portal:  
   **[https://developer.qingping.co/private/access-configuration](https://developer.qingping.co/private/access-configuration)**
   *(If you do not have an account yet, sign up with the same Qingping+ account used in your Qingping mobile app).*
2. In the left sidebar under **Private Configuration**, select **MQTT Configuration** and click the blue **`+ Add`** button in the top-right corner.
3. Fill in the **Add Configuration** modal with these exact settings:
   - **Name**: e.g. `anastasiia-air-monitor`, `danylo-air-monitor`, or `parents-air-monitor`
   - **Device Model**: `Qingping Air Monitor Lite`
   - **Private Type**: `Self-built MQTT`
4. Click the **edit (pencil) icon** inside **Self-built MQTT Information** and fill in:
   | Field | Value to Enter | Notes |
   | :--- | :--- | :--- |
   | **Protocol** | `mqtt` (TCP) | Do **not** select `mqtts` / TLS |
   | **Host** | `192.168.51.147` | **Must be the local IP address of your VentMaster ESP32** (not a hostname like `parents-air-monitor`) |
   | **Port** | `1883` | Embedded broker TCP port on the VentMaster ESP32 |
   | **Username** | `mqtt` *(or any non-empty string if required by the form)* | VentMaster's embedded broker accepts any username/password |
   | **Password** | `mqtt` *(or any non-empty string if required by the form)* | VentMaster's embedded broker accepts any username/password |
   | **Client ID** | `qingping-CC-B5-D1-32-1C-16` | Use the sensor's MAC address (e.g. `qingping-<MAC>`) |
   | **Topic Up** | `qingping/CC-B5-D1-32-1C-16/up` | Replace with each sensor's MAC (`CC-B5-D1-32-21-0E`, `CC-B5-D1-32-1C-16`, or `CC-B5-D1-32-23-41`) |
   | **Topic Down** | `qingping/CC-B5-D1-32-1C-16/down` | Replace with each sensor's MAC (`CC-B5-D1-32-21-0E`, `CC-B5-D1-32-1C-16`, or `CC-B5-D1-32-23-41`) |
5. Set the remaining dropdowns in the modal:
   - **Interval of Uploading**: `15 minutes` *(the portal's minimum dropdown value — VentMaster will automatically override this to **15 seconds** live streaming over `topicDown` as soon as the sensor connects!)*
   - **Interval of Recording**: `15 minutes`
   - **Custom Config**: Leave empty.
6. Click **Confirm** to save the configuration.

---

### Step 2: Bind Device to Configuration on `developer.qingping.co/private/device-binding`

After creating the MQTT configuration profile(s), push the configuration to each physical Qingping Air Monitor Lite:

1. Open the **Device Binding** page in the Qingping Developer Portal:  
   **[https://developer.qingping.co/private/device-binding](https://developer.qingping.co/private/device-binding)**
   *(Or go to **Push Configuration** in the left sidebar under **Private Configuration**).*
2. Ensure your Qingping Air Monitor Lite is **powered on via USB-C** and **connected to your Wi-Fi network** (`Ridna Hata IOT`) so the Qingping Cloud can push the new broker settings to the device.
3. Click **`+ Add`** (or **Bind Device**):
   - **Select Configuration**: Choose the MQTT configuration you created in Step 1 (e.g. `anastasiia-air-monitor` for `CC:B5:D1:32:1C:16`, `danylo-air-monitor` for `CC:B5:D1:32:21:0E`, `parents-air-monitor` for `CC:B5:D1:32:23:41`).
   - **Enter / Select Device MAC**: Enter the MAC address of that Qingping Air Monitor Lite:
     - **Danylo Room (Stue)**: `CC:B5:D1:32:21:0E` (or `CCB5D132210E`)
     - **Anastasiia Room (Kontor)**: `CC:B5:D1:32:1C:16` (or `CCB5D1321C16`)
     - **Parents Room (Foreldre)**: `CC:B5:D1:32:23:41` (or `CCB5D1322341`)
4. Click **Confirm / Push**:
   - Within **5–30 seconds**, the Qingping cloud pushes the Self-built MQTT configuration to the monitor.
   - The monitor disconnects from the cloud/old broker and opens a direct TCP connection to `192.168.51.147:1883`!
   - Once pushed, the monitor stores this broker IP in its internal flash and connects directly to `192.168.51.147:1883` on every boot — even if the internet is unplugged.

---

### Step 3: Bind & Verify on the VentMaster Web Dashboard

1. Open the VentMaster Web Dashboard at **`http://192.168.51.147/`** and switch to the **`Administration`** tab.
2. Scroll to **Qingping Air Monitor Lite (CGDN1) — Direct Wi-Fi Connection**:
   - The 3 default MAC addresses (`CC:B5:D1:32:21:0E`, `CC:B5:D1:32:1C:16`, `CC:B5:D1:32:23:41`) are already pre-bound to `Danylo Room`, `Anastasiia Room`, and `Parents Room`.
   - Under **Auto-Discovered Qingping Monitors**, any connected monitor appears with live metrics (`CO₂`, `Temp`, `Hum`, `PM2.5`, `PM10`) and one-click `Bind → <Room>` buttons.
   - Under **Port 1883 Live Broker Activity Log**, you will see:
     ```text
     [267s] TCP connection opened from 192.168.51.231
     [267s] CONNECT from 192.168.51.231 (id=qingping-CC-B5-D1-32-1C-16, ka=120s)
     [267s] SUBSCRIBE from 192.168.51.231 to [qingping/CC-B5-D1-32-1C-16/down]
     [267s] Sent 15s live-stream config to [qingping/CC-B5-D1-32-1C-16/down]
     [274s] PUBLISH [qingping/CC-B5-D1-32-1C-16/up] (QoS=0, 371B): ...
     ```

---

### Step 4: Reconfiguring, Swapping, or Unbinding Devices

- **Reassigning a Monitor to Another Room**:
  - In **Administration** &rarr; **Qingping Air Monitor Lite (CGDN1)**, click `Bind → <Room>` on the discovered monitor card, or enter its MAC in **Room ← Qingping MAC Assignments** and click **Save MAC**.
  - Bindings are persisted in ESP32 NVS flash (`vent_cfg` namespace: `qp_danylo`, `qp_anastasiia`, `qp_parents`, `qp_gym`).
- **Unbinding a Room (e.g. to switch a room to an IKEA ESP-NOW sensor)**:
  - Clear the MAC input box for that room and click **Save MAC** (or send an ESP-NOW packet for that room).

---

### Diagnostics & Live Broker Log

- **Troubleshooting Checklist**:
  1. **Monitor does not appear in `Port 1883 Live Broker Activity Log`**:
     - Check `developer.qingping.co/private/access-configuration` and verify `host=192.168.51.147` (must be the IP address, **not** a text name like `host=parents-air-monitor`) and `port=1883`.
     - Check `developer.qingping.co/private/device-binding` and verify **Device Number** is `1` (the device is actually bound to that configuration).
     - Ensure your router assigns a static DHCP lease (`192.168.51.147`) to the Master Controller (`MAC: 8C:4F:00:29:8F:E8`).
  2. **Monitor connects (`CONNECT`), subscribes, and then disconnects**:
     - Fixed in firmware via non-blocking LwIP sockets and `WiFi.setSleep(false)`. Ensure Master is running the latest `builds/ventmaster_latest.bin`.

---

## Upgraded IKEA VINDRIKTNING + ESP32-C3 Sensor Nodes (ESP-NOW)

Custom room air quality nodes built inside **IKEA VINDRIKTNING** enclosures using an **ESP32-C3 Super Mini** ([`sensor_node_example/`](sensor_node_example/)) transmit directly to the Master Controller over **ESP-NOW** every **5 seconds** (`TRANSMIT_INTERVAL_S = 5`), while also hosting a local HTTP/ArduinoOTA server on Wi-Fi.

### Hardware Wiring & Continuous Power Mod

| Sensor / Connection | ESP32-C3 Super Mini Pin | Notes |
| :--- | :--- | :--- |
| **Sensirion SCD40** (`SDA`) | `GPIO 8` | I2C Data (`3.3V` power) |
| **Sensirion SCD40** (`SCL`) | `GPIO 9` | I2C Clock (`3.3V` power) |
| **External I2C Temp/Hum** (`SDA`/`SCL`) | `GPIO 8` / `GPIO 9` | Wired in parallel on the same I2C bus (`3.3V`, `GND`, `SDA`, `SCL`) |
| **Cubic PM1006K** (`TX` / `REST` pad) | `GPIO 20` (`UART1 RX`) | 9600 baud, 8N1, 20-byte optical dust frame (`0x16 0x11 0x0B ...`) |
| **+5V Power** | `5V` pin | Solder to `+5V` test pad at the top of the IKEA PCB |
| **Ground (`GND`)** | `GND` pin | **IMPORTANT**: Solder to the **main `GND` test pad** near USB-C (do **not** solder `GND` to the Fan `-` pad!) |

> **Why You Must Use the Main `GND` Pad (Not the Fan `-` Pad)**:
> The IKEA VINDRIKTNING's onboard MCU switches the internal fan's negative (`GND`) transistor ON for ~20 seconds and **OFF (`0V`) for 10–90 seconds** in cycles.
> - If the ESP32-C3's `GND` is soldered to the fan `-` pad, the ESP32-C3 loses power every time the fan pauses!
> - **Software Mitigation Included**: Even if wired to the switched fan pad, the slave firmware starts the SCD40 at `t = 0.15s` after power-on and broadcasts its first ESP-NOW packet the instant the first SCD40 reading completes (`t ≈ 5.2s`), while the Master uses a **10-minute timeout** (`SENSOR_OFFLINE_TIMEOUT_MS = 600000`) so the room stays `ONLINE` across fan pauses.
> - Moving the `GND` wire from the fan `-` pad to the main `GND` test pad gives the ESP32-C3 continuous 24/7 power.

### Automatic External I2C Temperature & Humidity Sensor Support
Because the SCD40 + ESP32-C3 + 5V regulator inside the closed IKEA case generate ~4–6°C of internal self-heating (which lowers the SCD40's internal relative humidity calculation by ~10% RH), the slave firmware ([`sensor_node_example/src/main.cpp`](sensor_node_example/src/main.cpp)) includes **zero-config automatic detection** for an external I2C temperature & humidity sensor wired in parallel on `SDA = GPIO 8`, `SCL = GPIO 9`:
- Automatically scans the I2C bus on boot (and every 30 seconds if hot-plugged) for:
  - **Sensirion SHT40 / SHT41 / SHT45** (`0x44` or `0x45`)
  - **Sensirion SHT30 / SHT31 / SHT35** (`0x44` or `0x45`)
  - **ASAIR AHT20 / AHT21 / AHT25** (`0x38`)
- When detected, the node automatically uses the external sensor for **Temperature** and **Humidity**, while continuing to use the **SCD40** for **CO₂** and the **PM1006K** for **PM2.5**.
- Check `http://<node-ip>/api/status` (e.g. `http://192.168.51.122/api/status`) to view `"th_sensor"` (`"SCD40 (Internal)"`, `"SHT4x (External I2C)"`, `"SHT3x (External I2C)"`, or `"AHT20/21 (External I2C)"`).
- When no external sensor is attached, the firmware applies `scd4x.setTemperatureOffset(6.0f)` to compensate for enclosure self-heating.

---

## Web Interface & REST API

Served from ESP32 Flash memory ([`include/web_assets.h`](include/web_assets.h)) and organized into two main tabs:
1. **`Dashboard` Tab**:
   - **Fan Speed Control**: Segmented buttons for `OFF`, `SLOW`, `MED`, `HIGH`.
   - **Room Air Damper Valves**: Individual switches for `Danylo Room`, `Anastasiia Room`, `Parents Room`, and `Gym Room`.
   - **Room Air Quality Cards**: Always displays **Temperature (`°C`)**, **Humidity (`%`)**, **CO₂ Level (`ppm`)**, and **PM2.5 (`µg/m³`)** (including `0 µg/m³` in clean air) plus sensor source label, MAC/FW version, and seconds since last update.
2. **`Administration` Tab**:
   - **Module & Sensor Fleet OTA Table**: Lists the Master Controller (`ventmaster_*.bin`) and all 4 room sensors (`ventslave_<room>_*.bin`), enforcing strict filename and binary target verification.
   - **Qingping Air Monitor Lite (CGDN1) — Direct Wi-Fi Connection**: Room MAC bindings, auto-discovered Qingping monitors with 1-click bind buttons, and live TCP port `1883` activity logs.
   - **System Diagnostics**: Uptime, free heap, Wi-Fi channel, IP/SoftAP status, MAC address, and system reboot button.

### REST Endpoints
| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/` | Web Dashboard & Administration UI |
| `GET` | `/api/status` | Full JSON state (fan speed, valves, sensors, modules, discovered Qingpings, port 1883 logs) |
| `POST` | `/api/speed?val=0..3` | Set fan speed (`0=OFF`, `1=SLOW`, `2=MED`, `3=HIGH`) |
| `POST` | `/api/valve?room=<id>&state=0\|1` | Toggle room valve (`danylo`, `anastasiia`, `parents`, `gym`) |
| `POST` | `/api/mode?val=manual\|auto` | Switch operating mode (`manual` or `auto`) |
| `POST` | `/api/qingping/bind?room=<id>&mac=<mac>` | Bind, reassign, or unbind a Qingping MAC address to/from a room |
| `POST` | `/api/sensor/external` | Ingest external sensor telemetry (JSON or query parameters) |
| `POST` | `/api/restart` | Gracefully reboot the Master ESP32 controller |
| `POST` | `/update` | Guarded multipart OTA firmware upload (verifies `VENT_FW_DESC_V1` header before flashing) |

---

## Home Assistant MQTT Integration

Built-in **MQTT Auto-Discovery** ([`src/ha_mqtt.cpp`](src/ha_mqtt.cpp)) automatically registers all entities in Home Assistant and cleans up legacy topic names on connect:

- **Controls**:
  - `select.ventilation_speed` (`OFF`, `SLOW`, `MEDIUM`, `HIGH`)
  - `switch.ventilation_auto_mode`
  - `switch.valve_danylo`, `switch.valve_anastasiia`, `switch.valve_parents`, `switch.valve_gym`
- **Per-Room Telemetry (`danylo`, `anastasiia`, `parents`, `gym`)**:
  - Temperature (`°C`), Humidity (`%`), CO₂ (`ppm`), PM2.5 (`µg/m³` — published even when `0 µg/m³`), and **`<Room> Last Update` (`s`)**.

---

## Building & Versioning

Use [`build.ps1`](build.ps1) to build and verify the Master and all 4 Slave firmware binaries:

```powershell
# Build Master + all 4 Slave room binaries (danylo, anastasiia, parents, gym)
.\build.ps1 -Target all

# Build only Master Controller
.\build.ps1 -Target master

# Build only a specific Slave room node (e.g. gym)
.\build.ps1 -Target slaves -Slave gym
```

### Generated Binaries in [`builds/`](builds/)
Every binary embeds a 64-byte `VentFwTargetDesc` signature (`VENT_FW_DESC_V1`) at byte offset `288` verified by `build.ps1` and by the device before flashing:
- **Master (`ESP32-WROOM-32`, chip ID `0x0000`, target `master:master`)**:
  - `builds/ventmaster_<version>_<timestamp>.bin` & `builds/ventmaster_latest.bin`
- **Slaves (`ESP32-C3`, chip ID `0x0005`, target `slave:<room>`)**:
  - `builds/ventslave_danylo_latest.bin`
  - `builds/ventslave_anastasiia_latest.bin`
  - `builds/ventslave_parents_latest.bin`
  - `builds/ventslave_gym_latest.bin`

---

## Flashing Firmware & Target Verification

### 1. Web OTA (Master & Slaves)
- Open `http://192.168.51.147/` &rarr; **Administration** tab, or upload via `curl`:
  ```powershell
  # Flash Master Controller over Wi-Fi OTA
  curl.exe -X POST -F "update=@builds/ventmaster_latest.bin;filename=ventmaster_0.0.1.bin" http://192.168.51.147/update

  # Flash Gym Room ESP32-C3 Sensor Node over Wi-Fi OTA
  curl.exe -X POST -F "update=@builds/ventslave_gym_latest.bin;filename=ventslave_gym_0.0.1.bin" http://192.168.51.122/update
  ```
- **Cross-Flash Protection**: Attempting to upload a Slave binary to the Master, a Master binary to a Slave, or `ventslave_danylo_*.bin` to the `gym` node is automatically rejected before touching flash.

### 2. USB Serial Flashing
```powershell
# Flash Master Controller on COM4
pio run -t upload --upload-port COM4

# Monitor Master Serial Output
pio device monitor -p COM4 -b 115200
```

---

## Project Roadmap & TODO

See [`TODO.md`](TODO.md) for the active hardware rollout checklist and Phase 2 subproject (**Bathroom Tuya Zigbee `ywagc4rj` Temperature & Humidity Sensors**).
