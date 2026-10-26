#pragma once

#include <Arduino.h>
#include <vector>

#define QINGPING_BROKER_PORT 1883
#define QINGPING_MAX_CLIENTS 6

struct QingpingClientSlot {
    int fd = -1;
    String remoteIp;
    String mac;             // Normalized 12-hex MAC e.g. "CCB5D1321C16"
    String downTopic;       // Exact subscribed down topic e.g. "qingping/CC-B5-D1-32-1C-16/down"
    std::vector<uint8_t> rxBuf;
    size_t discardBytes = 0;
    uint8_t pendingAckQos = 0;
    uint8_t pendingAckHi = 0;
    uint8_t pendingAckLo = 0;
    unsigned long lastActivity = 0;
    unsigned long lastConfigSent = 0;
};

class QingpingBroker {
public:
    static QingpingBroker& instance();

    void init();
    void update();

    // Send fast-report interval configuration to a connected Qingping
    void sendFastReportConfig(QingpingClientSlot& slot);
    int getConnectedClientCount() const;
    bool isMacConnected(const String& normMac) const;
    std::vector<String> getRecentLogs() const;
    void addLog(const String& entry);

private:
    QingpingBroker();
    int listenFd = -1;
    QingpingClientSlot slots[QINGPING_MAX_CLIENTS];
    std::vector<String> recentLogs;

    void closeSlot(QingpingClientSlot& slot, const char* reason = nullptr);
    bool processNextPacket(QingpingClientSlot& slot);
    bool sendAll(int fd, const uint8_t* data, size_t len);
    void publishToClient(int fd, const String& topic, const String& payload);
    static void encodeRemainingLength(uint8_t* buf, size_t& idx, size_t length);
};
