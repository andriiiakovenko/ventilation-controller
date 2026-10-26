#include "qingping_broker.h"
#include "ventilation_controller.h"

QingpingBroker& QingpingBroker::instance() {
    static QingpingBroker _inst;
    return _inst;
}

QingpingBroker::QingpingBroker() : server(QINGPING_BROKER_PORT) {}

void QingpingBroker::init() {
    server.begin();
    server.setNoDelay(true);
    addLog("Port 1883 Direct MQTT Broker started — waiting for Qingping TCP connections");
    Serial.printf("[QingpingBroker] Embedded Direct MQTT server listening on TCP port %d\n",
                  QINGPING_BROKER_PORT);
}

void QingpingBroker::addLog(const String& entry) {
    unsigned long sec = millis() / 1000UL;
    String line = String("[") + sec + "s] " + entry;
    recentLogs.push_back(line);
    if (recentLogs.size() > 8) {
        recentLogs.erase(recentLogs.begin());
    }
}

std::vector<String> QingpingBroker::getRecentLogs() const {
    return recentLogs;
}

int QingpingBroker::getConnectedClientCount() const {
    int count = 0;
    for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
        if (const_cast<WiFiClient&>(slots[i].client).connected()) {
            count++;
        }
    }
    return count;
}

void QingpingBroker::encodeRemainingLength(uint8_t* buf, size_t& idx, size_t length) {
    do {
        uint8_t encodedByte = length % 128;
        length /= 128;
        if (length > 0) {
            encodedByte |= 0x80;
        }
        buf[idx++] = encodedByte;
    } while (length > 0);
}

void QingpingBroker::publishToClient(WiFiClient& client, const String& topic, const String& payload) {
    if (!client.connected()) return;

    size_t topicLen = topic.length();
    size_t payloadLen = payload.length();
    size_t remLen = 2 + topicLen + payloadLen;

    uint8_t header[8];
    size_t hIdx = 0;
    header[hIdx++] = 0x30; // PUBLISH, QoS 0, no retain
    encodeRemainingLength(header, hIdx, remLen);
    header[hIdx++] = (uint8_t)((topicLen >> 8) & 0xFF);
    header[hIdx++] = (uint8_t)(topicLen & 0xFF);

    client.write(header, hIdx);
    client.write((const uint8_t*)topic.c_str(), topicLen);
    client.write((const uint8_t*)payload.c_str(), payloadLen);
}

void QingpingBroker::sendFastReportConfig(WiFiClient& client, const String& downTopic) {
    if (downTopic.length() == 0) return;

    // 1. Set permanent sampling & report interval to 15 seconds (type 17)
    String cfg17 = "{\"id\":1,\"need_ack\":1,\"type\":\"17\",\"setting\":{\"report_interval\":15,\"collect_interval\":15,\"co2_sampling_interval\":15,\"pm_sampling_interval\":15}}";
    publishToClient(client, downTopic, cfg17);

    // 2. Request real-time 15-second upload stream for 24h (type 12, string & int variants)
    String cfg12 = "{\"type\":\"12\",\"up_itvl\":\"15\",\"duration\":\"86400\"}";
    publishToClient(client, downTopic, cfg12);

    Serial.printf("[QingpingBroker] Sent 15s fast-report config to [%s]\n", downTopic.c_str());
}

void QingpingBroker::handleClientPacket(QingpingClientSlot& slot) {
    WiFiClient& client = slot.client;
    if (client.available() < 2) return;

    int b0 = client.read();
    if (b0 < 0) return;

    uint8_t packetType = (b0 >> 4) & 0x0F;
    uint8_t flags = b0 & 0x0F;

    // Decode MQTT Remaining Length (up to 4 bytes)
    size_t remLen = 0;
    size_t multiplier = 1;
    uint8_t encodedByte = 0;
    int bytesRead = 0;

    unsigned long waitStart = millis();
    do {
        while (!client.available()) {
            if (millis() - waitStart > 200) return;
            delay(1);
        }
        int r = client.read();
        if (r < 0) return;
        encodedByte = (uint8_t)r;
        remLen += (encodedByte & 0x7F) * multiplier;
        multiplier *= 128;
        bytesRead++;
    } while ((encodedByte & 0x80) != 0 && bytesRead < 4);

    if (remLen > 2048) {
        while (remLen > 0 && client.connected()) {
            if (client.available()) {
                client.read();
                remLen--;
            } else if (millis() - waitStart > 300) {
                break;
            }
        }
        return;
    }

    std::vector<uint8_t> buf(remLen);
    size_t got = 0;
    waitStart = millis();
    while (got < remLen) {
        int avail = client.available();
        if (avail > 0) {
            int toRead = min((size_t)avail, remLen - got);
            int r = client.read(buf.data() + got, toRead);
            if (r > 0) got += r;
        } else {
            if (millis() - waitStart > 300) break;
            delay(1);
        }
    }
    if (got < remLen) return;

    slot.lastActivity = millis();
    String remoteIp = client.remoteIP().toString();

    switch (packetType) {
        case 1: { // CONNECT
            const uint8_t connack[4] = {0x20, 0x02, 0x00, 0x00};
            client.write(connack, 4);

            // Parse ClientId from MQTT CONNECT payload (after protocol name + 4 bytes header)
            String clientId = "";
            if (remLen >= 8) {
                uint16_t protoLen = ((uint16_t)buf[0] << 8) | buf[1];
                size_t cidOffset = 2 + protoLen + 4; // level(1) + flags(1) + keepalive(2)
                if (cidOffset + 2 <= remLen) {
                    uint16_t cidLen = ((uint16_t)buf[cidOffset] << 8) | buf[cidOffset + 1];
                    if (cidOffset + 2 + cidLen <= remLen) {
                        clientId = String((const char*)(buf.data() + cidOffset + 2), cidLen);
                    }
                }
            }

            // If clientId starts with "qingping-", extract the MAC
            if (clientId.startsWith("qingping-")) {
                String rawMacPart = clientId.substring(9);
                String normMac = VentilationController::normalizeMac(rawMacPart);
                if (normMac.length() == 12) {
                    slot.mac = normMac;
                    if (slot.downTopic.length() == 0) {
                        slot.downTopic = String("qingping/") + rawMacPart + "/down";
                    }
                    VentilationController::instance().onQingpingConnected(normMac, remoteIp);
                }
            }

            addLog(String("CONNECT from ") + remoteIp + (clientId.length() ? (" (clientId=" + clientId + ")") : ""));
            Serial.printf("[QingpingBroker] CONNECT from %s (clientId=%s)\n", remoteIp.c_str(), clientId.c_str());
            break;
        }
        case 8: { // SUBSCRIBE
            if (remLen >= 5) {
                uint8_t pktIdHi = buf[0];
                uint8_t pktIdLo = buf[1];
                uint16_t topicLen = ((uint16_t)buf[2] << 8) | buf[3];
                String subTopic = "";
                if (4 + topicLen <= remLen) {
                    subTopic = String((const char*)(buf.data() + 4), topicLen);
                }

                const uint8_t suback[5] = {0x90, 0x03, pktIdHi, pktIdLo, 0x00};
                client.write(suback, 5);

                addLog(String("SUBSCRIBE from ") + remoteIp + " to [" + subTopic + "]");
                Serial.printf("[QingpingBroker] Client %s subscribed to [%s]\n",
                              remoteIp.c_str(), subTopic.c_str());

                if (subTopic.length() > 0) {
                    slot.downTopic = subTopic;
                    if (subTopic.startsWith("qingping/")) {
                        int secondSlash = subTopic.indexOf('/', 9);
                        if (secondSlash > 9) {
                            slot.mac = VentilationController::normalizeMac(subTopic.substring(9, secondSlash));
                            VentilationController::instance().onQingpingConnected(slot.mac, remoteIp);
                        }
                    }
                    sendFastReportConfig(client, slot.downTopic);
                    slot.lastConfigSent = millis();
                }
            }
            break;
        }
        case 3: { // PUBLISH
            if (remLen < 2) break;
            uint16_t topicLen = ((uint16_t)buf[0] << 8) | buf[1];
            if (2 + topicLen > remLen) break;

            String topic((const char*)(buf.data() + 2), topicLen);
            size_t payloadOffset = 2 + topicLen;

            uint8_t qos = (flags >> 1) & 0x03;
            if (qos == 1 || qos == 2) {
                if (payloadOffset + 2 > remLen) break;
                uint8_t pktIdHi = buf[payloadOffset];
                uint8_t pktIdLo = buf[payloadOffset + 1];
                payloadOffset += 2;

                if (qos == 1) {
                    const uint8_t puback[4] = {0x40, 0x02, pktIdHi, pktIdLo};
                    client.write(puback, 4);
                }
            }

            size_t payloadLen = remLen - payloadOffset;
            const uint8_t* payloadData = buf.data() + payloadOffset;

            if (topic.startsWith("qingping/")) {
                int secondSlash = topic.indexOf('/', 9);
                if (secondSlash > 9) {
                    String rawTopicMac = topic.substring(9, secondSlash);
                    if (slot.mac.length() == 0) {
                        slot.mac = VentilationController::normalizeMac(rawTopicMac);
                    }
                    if (slot.downTopic.length() == 0) {
                        slot.downTopic = String("qingping/") + rawTopicMac + "/down";
                    }
                }
            }

            // Build readable log snippet
            String preview = "";
            if (payloadLen >= 2 && payloadData[0] == 0x43 && payloadData[1] == 0x47) {
                preview = String("[Binary CG TLV, ") + payloadLen + " bytes]";
            } else {
                size_t showLen = min(payloadLen, (size_t)120);
                preview = String((const char*)payloadData, showLen);
            }
            addLog(String("PUBLISH [") + topic + "] (" + payloadLen + "B): " + preview);

            if (slot.downTopic.length() > 0 && (slot.lastConfigSent == 0 || (millis() - slot.lastConfigSent > 3600000UL))) {
                sendFastReportConfig(client, slot.downTopic);
                slot.lastConfigSent = millis();
            }

            VentilationController::instance().onQingpingPayload(topic, payloadData, payloadLen, remoteIp);
            break;
        }
        case 12: { // PINGREQ
            const uint8_t pingresp[2] = {0xD0, 0x00};
            client.write(pingresp, 2);
            break;
        }
        case 14: { // DISCONNECT
            addLog(String("DISCONNECT from ") + remoteIp);
            client.stop();
            break;
        }
        default:
            break;
    }
}

void QingpingBroker::update() {
    if (server.hasClient()) {
        WiFiClient incoming = server.available();
        if (incoming) {
            incoming.setNoDelay(true);
            int targetIdx = -1;
            unsigned long oldestActivity = ULONG_MAX;
            int oldestIdx = 0;

            for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
                if (!slots[i].client || !slots[i].client.connected()) {
                    targetIdx = i;
                    break;
                }
                if (slots[i].lastActivity < oldestActivity) {
                    oldestActivity = slots[i].lastActivity;
                    oldestIdx = i;
                }
            }

            if (targetIdx < 0) {
                slots[oldestIdx].client.stop();
                targetIdx = oldestIdx;
            }

            slots[targetIdx].client = incoming;
            slots[targetIdx].mac = "";
            slots[targetIdx].downTopic = "";
            slots[targetIdx].lastActivity = millis();
            slots[targetIdx].lastConfigSent = 0;
            addLog(String("TCP connection opened from ") + incoming.remoteIP().toString());
        }
    }

    unsigned long now = millis();
    for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
        if (slots[i].client && slots[i].client.connected()) {
            while (slots[i].client.available() >= 2) {
                handleClientPacket(slots[i]);
            }
            if (now - slots[i].lastActivity > 300000UL) {
                slots[i].client.stop();
            }
        }
    }
}
