#pragma once

#include <Arduino.h>
#include <WiFi.h>

#define QINGPING_BROKER_PORT 1883
#define QINGPING_MAX_CLIENTS 6

struct QingpingClientSlot {
    WiFiClient client;
    String mac;             // Normalized 12-hex MAC e.g. "CCB5D1321C16"
    String downTopic;       // Exact subscribed down topic e.g. "qingping/CC-B5-D1-32-1C-16/down"
    unsigned long lastActivity = 0;
    unsigned long lastConfigSent = 0;
};

class QingpingBroker {
public:
    static QingpingBroker& instance();

    void init();
    void update();

    // Send fast-report interval configuration to a connected Qingping
    void sendFastReportConfig(WiFiClient& client, const String& downTopic);
    int getConnectedClientCount() const;

private:
    QingpingBroker();
    WiFiServer server;
    QingpingClientSlot slots[QINGPING_MAX_CLIENTS];

    void handleClientPacket(QingpingClientSlot& slot);
    void publishToClient(WiFiClient& client, const String& topic, const String& payload);
    static void encodeRemainingLength(uint8_t* buf, size_t& idx, size_t length);
};
