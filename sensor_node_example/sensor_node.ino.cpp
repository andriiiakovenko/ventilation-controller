# 1 "C:\\Users\\ANDRII~1.IAK\\AppData\\Local\\Temp\\tmpx0uhqxs1"
#include <Arduino.h>
# 1 "C:/DATA/Private/Dropbox/Projects/VentilationControllerESPHome/sensor_node_example/sensor_node.ino"
# 10 "C:/DATA/Private/Dropbox/Projects/VentilationControllerESPHome/sensor_node_example/sensor_node.ino"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>


#define ROOM_ID "parents"
#define PRIMARY_CHANNEL 6
#define FALLBACK_CHANNEL 1

#define STATUS_LED_PIN 8
#define LED_ACTIVE_LEVEL LOW


#define USE_DUMMY_SENSOR true
#define TRANSMIT_INTERVAL_S 10
#define ENABLE_DEEP_SLEEP false



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


uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

RTC_DATA_ATTR uint32_t packetSequence = 0;
static unsigned long lastTransmitTime = 0;

void blinkLed(int durationMs = 80) {
    digitalWrite(STATUS_LED_PIN, LED_ACTIVE_LEVEL);
    delay(durationMs);
    digitalWrite(STATUS_LED_PIN, !LED_ACTIVE_LEVEL);
}

void getSensorReadings(float &temp, float &hum, float &battery) {
#if USE_DUMMY_SENSOR


    int tOffset = (int)(packetSequence % 10) - 5;
    int hOffset = (int)(((packetSequence * 3) % 10)) - 5;

    temp = 22.4f + (tOffset * 0.12f);
    hum = 58.0f + (hOffset * 0.35f);
    battery = 3.30f;
#else

    temp = 22.0f;
    hum = 55.0f;
    battery = 3.30f;
#endif
}

bool initEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();


    esp_wifi_set_channel(PRIMARY_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed!");
        return false;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] Failed to add broadcast peer!");
        return false;
    }

    Serial.println("[ESP-NOW] Initialized with broadcast peer.");
    return true;
}

void sendPacket() {
    float temp = 0.0f;
    float hum = 0.0f;
    float battery = 3.3f;
    getSensorReadings(temp, hum, battery);

    ESPNowSensorPacket packet;
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.roomId, ROOM_ID, sizeof(packet.roomId) - 1);
    packet.temperature = temp;
    packet.humidity = hum;
    packet.batteryVoltage = battery;
    packet.packetSeq = packetSequence++;

    Serial.printf("[Sensor Node] [%s] Sending Seq #%u -> Temp: %.1f C, Hum: %.1f %%, Bat: %.2f V\n",
                  ROOM_ID, packet.packetSeq, temp, hum, battery);


    digitalWrite(STATUS_LED_PIN, LED_ACTIVE_LEVEL);


    esp_wifi_set_channel(PRIMARY_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_err_t res = esp_now_send(broadcastAddress, (uint8_t*)&packet, sizeof(packet));
    if (res == ESP_OK) {
        Serial.printf("  -> Sent on Primary Channel %d\n", PRIMARY_CHANNEL);
    } else {
        Serial.printf("  -> Error sending on channel %d: %d\n", PRIMARY_CHANNEL, res);
    }

    delay(4);


    esp_wifi_set_channel(FALLBACK_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_now_send(broadcastAddress, (uint8_t*)&packet, sizeof(packet));


    esp_wifi_set_channel(PRIMARY_CHANNEL, WIFI_SECOND_CHAN_NONE);

    delay(50);

    digitalWrite(STATUS_LED_PIN, !LED_ACTIVE_LEVEL);
}

void setup() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, !LED_ACTIVE_LEVEL);


    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=============================================");
    Serial.println("  ESP32-C3 SUPER MINI - SENSOR NODE");
    Serial.printf ("  Room: %s | Primary Ch: %d | Fallback Ch: %d\n", ROOM_ID, PRIMARY_CHANNEL, FALLBACK_CHANNEL);
    Serial.println("=============================================");


    blinkLed(100);
    delay(100);
    blinkLed(100);

    if (!initEspNow()) {
        Serial.println("[FATAL] ESP-NOW initialization failed!");
        return;
    }


    sendPacket();

#if ENABLE_DEEP_SLEEP
    Serial.printf("[Sensor Node] Entering deep sleep for %d seconds...\n", TRANSMIT_INTERVAL_S);
    esp_sleep_enable_timer_wakeup((uint64_t)TRANSMIT_INTERVAL_S * 1000000ULL);
    esp_deep_sleep_start();
#else
    lastTransmitTime = millis();
#endif
}

void loop() {
#if !ENABLE_DEEP_SLEEP
    unsigned long now = millis();
    if (now - lastTransmitTime >= (TRANSMIT_INTERVAL_S * 1000UL)) {
        lastTransmitTime = now;
        sendPacket();
    }
    delay(50);
#endif
}
