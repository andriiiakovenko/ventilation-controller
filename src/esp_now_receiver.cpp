#include "esp_now_receiver.h"
#include <esp_idf_version.h>

ESPNowReceiver& ESPNowReceiver::instance() {
    static ESPNowReceiver _inst;
    return _inst;
}

ESPNowReceiver::ESPNowReceiver() {}

// Dual compatibility wrapper for Arduino Core v2 and v3
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
static void espNowRawCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (recv_info != nullptr) {
        ESPNowReceiver::instance().handleReceivedPacket(recv_info->src_addr, data, len);
    }
}
#else
static void espNowRawCallback(const uint8_t *mac, const uint8_t *data, int len) {
    ESPNowReceiver::instance().handleReceivedPacket(mac, data, len);
}
#endif

bool ESPNowReceiver::init() {
    if (initialized) return true;

    // Queue used to hand packets from the Wi-Fi task over to the loop task.
    if (rxQueue == nullptr) {
        rxQueue = xQueueCreate(16, sizeof(QueuedPacket));
        if (rxQueue == nullptr) {
            Serial.println("[ESP-NOW] Error creating RX queue!");
            return false;
        }
    }

    // ESP-NOW requires Wi-Fi subsystem to be active
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Error initializing ESP-NOW!");
        return false;
    }

    esp_now_register_recv_cb(espNowRawCallback);
    initialized = true;
    Serial.println("[ESP-NOW] Receiver successfully initialized and listening.");
    return true;
}

void ESPNowReceiver::setCallback(SensorDataCallback cb) {
    callback = cb;
}

// WARNING: Runs inside the Wi-Fi driver task. It must return immediately:
// no Serial output, no MQTT/TCP traffic, no shared-state mutation.
// Blocking here stalls the whole network stack (HTTP server, MQTT, ...).
void ESPNowReceiver::handleReceivedPacket(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < 32 || data == nullptr || mac == nullptr || rxQueue == nullptr) {
        invalidPackets = invalidPackets + 1;
        return;
    }

    QueuedPacket item;
    memset(&item, 0, sizeof(item));
    memcpy(&item.packet, data, (len < (int)sizeof(ESPNowSensorPacket)) ? len : sizeof(ESPNowSensorPacket));
    memcpy(item.mac, mac, 6);

    // Never wait: drop the packet if the loop task is lagging behind.
    if (xQueueSend(rxQueue, &item, 0) != pdTRUE) {
        droppedPackets = droppedPackets + 1;
    }
}

void ESPNowReceiver::process() {
    if (rxQueue == nullptr) return;

    static uint32_t reportedDropped = 0;
    static uint32_t reportedInvalid = 0;
    if (droppedPackets != reportedDropped) {
        reportedDropped = droppedPackets;
        Serial.printf("[ESP-NOW] Warning: RX queue full, %u packets dropped so far\n", (unsigned)reportedDropped);
    }
    if (invalidPackets != reportedInvalid) {
        reportedInvalid = invalidPackets;
        Serial.printf("[ESP-NOW] Warning: %u invalid packets (size < 32) received so far\n", (unsigned)reportedInvalid);
    }

    QueuedPacket item;
    while (xQueueReceive(rxQueue, &item, 0) == pdTRUE) {
        // Ensure null-terminated strings
        item.packet.roomId[sizeof(item.packet.roomId) - 1] = '\0';
        item.packet.fwVersion[sizeof(item.packet.fwVersion) - 1] = '\0';
        item.packet.ip[sizeof(item.packet.ip) - 1] = '\0';

        if (callback) {
            callback(item.packet, item.mac);
        }
    }
}
