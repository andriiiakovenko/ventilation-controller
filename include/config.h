#pragma once

#include <Arduino.h>

#define FIRMWARE_VERSION "0.0.1"


// ==========================================
// PIN DEFINITIONS & RELAY SETTINGS
// ==========================================
// Optocoupler relays are typically ACTIVE_LOW:
// LOW = Relay ENERGIZED (ON)
// HIGH = Relay DE-ENERGIZED (OFF)
#define RELAY_ACTIVE_STATE LOW
#define RELAY_INACTIVE_STATE HIGH

// Fan Speed Relay Pins
#define PIN_FAN_SPEED_1 13 // Slow
#define PIN_FAN_SPEED_2 14 // Medium
#define PIN_FAN_SPEED_3 26 // High

// Room Air Valve Relay Pins
#define PIN_VALVE_DANYLO 27
#define PIN_VALVE_ANASTASIIA 32
#define PIN_VALVE_PARENTS 33
#define PIN_VALVE_GIM 25

// Safety deadtime when switching fan speeds (in milliseconds)
// Prevents two speed taps from briefly energizing together
#define FAN_SWITCH_DEADTIME_MS 100

// Time to wait for motorized air valves to open before starting the fan (milliseconds)
#define VALVE_OPENING_DELAY_MS 2000

// ==========================================
// WI-FI SETTINGS
// ==========================================
// If router is unreachable or not configured, fallback to SoftAP
#define WIFI_STA_SSID "Ridna Hata IOT"
#define WIFI_STA_PASSWORD "10TuRidniyHati!"

#define WIFI_AP_SSID "ESP32-Ventilation-AP"
#define WIFI_AP_PASSWORD "ventilation123"
#define ESPNOW_CHANNEL 1
#define SOFTAP_TIMEOUT_MS (5 * 60 * 1000) // SoftAP auto-disables 5 mins after boot

// ==========================================
// HOME ASSISTANT / MQTT SETTINGS
// ==========================================
#define MQTT_ENABLED true
#define MQTT_BROKER_HOST                                                       \
  "192.168.51.100" // Se`t your Home Assistant / Mosquitto IP
#define MQTT_BROKER_PORT 1883
#define MQTT_USERNAME "mqtt-user"
#define MQTT_PASSWORD "MyVentilation123!"
#define MQTT_CLIENT_ID "esp32_ventilation_controller"

// MQTT Topics
#define MQTT_BASE_TOPIC "homeassistant"
#define MQTT_DEVICE_NAME "ventilation_controller"

// ==========================================
// AUTONOMOUS VENTILATION THRESHOLDS
// ==========================================
#define DEFAULT_HUMIDITY_HIGH_THRESHOLD                                        \
  65.0f // % RH: Open valve & increase fan speed
#define DEFAULT_HUMIDITY_TARGET_THRESHOLD 55.0f // % RH: Return to normal
#define SENSOR_OFFLINE_TIMEOUT_MS                                              \
  180000 // 3 minutes timeout for wireless sensors
