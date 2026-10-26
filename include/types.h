#pragma once

#include <Arduino.h>

enum FanSpeed { FAN_OFF = 0, FAN_SLOW = 1, FAN_MEDIUM = 2, FAN_HIGH = 3 };

enum VentilationMode { MODE_MANUAL = 0, MODE_AUTO = 1 };

struct RoomSensorData {
  String id;
  String friendlyName;
  float temperature = 0.0f;
  float humidity = 0.0f;
  float batteryVoltage = 0.0f;
  uint16_t co2 = 0;       // CO2 in ppm (from SCD40)
  uint16_t pm25 = 0;      // PM2.5 in ug/m3 (from Cubic PM1006K)
  unsigned long lastSeen = 0;
  bool isOnline = false;
  bool valveState = false; // true = OPEN, false = CLOSED
  String fwVersion = "0.0.1";
  String ip = "";
  String mac = "";
};

// ESP-NOW packet structure matching the remote sensor nodes
struct __attribute__((packed)) ESPNowSensorPacket {
  char roomId[16];      // e.g. "danylo", "anastasiia", "parents"
  float temperature;    // deg C
  float humidity;       // % RH
  float batteryVoltage; // e.g. 3.3V or 4.2V
  uint16_t co2;         // CO2 in ppm
  uint16_t pm25;        // PM2.5 in ug/m3
  uint32_t packetSeq;
  char fwVersion[12];   // e.g. "0.0.1"
  char ip[16];          // e.g. "192.168.51.105"
};
// ESP-NOW packet structure matching the remote control nodes
struct __attribute__((packed)) ESPNowControlPacket {
  char roomId[16];         // e.g. "danylo", "anastasiia", "parents"
  uint8_t fanSpeed;        // 0 = OFF, 1 = SLOW, 2 = MEDIUM, 3 = HIGH
  uint8_t ventilationMode; // 0 = MANUAL, 1 = AUTO
  uint32_t packetSeq;
};

// Embedded firmware target descriptor placed in .rodata_custom_desc (at byte offset 288 of .bin)
#define VENT_FW_MAGIC           "VENT_FW_DESC_V1"
#define VENT_FW_DESC_OFFSET     288
#define VENT_FW_HEADER_MIN_SIZE 352 // 288 + sizeof(VentFwTargetDesc)[64]

struct __attribute__((packed)) VentFwTargetDesc {
  char magic[16];    // "VENT_FW_DESC_V1"
  char role[16];     // "master" or "slave"
  char targetId[16]; // "master" or room ID ("danylo", "anastasiia", "parents", "gym")
  char version[16];  // e.g. "0.0.1"
};
