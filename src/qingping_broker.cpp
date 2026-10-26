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
    Serial.printf("[QingpingBroker] %s\n", line.c_str());
    recentLogs.push_back(line);
    if (recentLogs.size() > 30) {
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

    // Send as a single contiguous buffer so TCP doesn't fragment tiny header pieces
    std::vector<uint8_t> pkt(hIdx + topicLen + payloadLen);
    memcpy(pkt.data(), header, hIdx);
    memcpy(pkt.data() + hIdx, topic.c_str(), topicLen);
    memcpy(pkt.data() + hIdx + topicLen, payload.c_str(), payloadLen);
    client.write(pkt.data(), pkt.size());
}

void QingpingBroker::sendFastReportConfig(WiFiClient& client, const String& downTopic) {
    if (downTopic.length() == 0) return;

    // 1. Request real-time 15-second upload stream for 24h (type 12)
    String cfg12 = "{\"type\":\"12\",\"up_itvl\":\"15\",\"duration\":\"86400\"}";
    publishToClient(client, downTopic, cfg12);

    // 2. Set permanent sampling & report interval to 60s / 15s (type 17)
    String cfg17 = "{\"type\":\"17\",\"setting\":{\"report_interval\":60,\"collect_interval\":15,\"co2_sampling_interval\":15,\"pm_sampling_interval\":15}}";
    publishToClient(client, downTopic, cfg17);

    addLog(String("Sent 15s live-stream config to [") + downTopic + "]");
    Serial.printf("[QingpingBroker] Sent 15s fast-report config to [%s]\n", downTopic.c_str());
}

// Helper: reliably read exact byte count from WiFiClient, resetting timeout whenever data arrives
static bool readExactBytes(WiFiClient& client, uint8_t* dst, size_t count, unsigned long idleTimeoutMs = 1500) {
    size_t got = 0;
    unsigned long lastProgress = millis();
    while (got < count) {
        int avail = client.available();
        if (avail > 0) {
            size_t toRead = min((size_t)avail, count - got);
            int r = client.read(dst + got, toRead);
            if (r > 0) {
                got += (size_t)r;
                lastProgress = millis();
            }
        } else {
            if (!client.connected() || (millis() - lastProgress > idleTimeoutMs)) {
                return false;
            }
            delay(1);
        }
    }
    return true;
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

    do {
        uint8_t b = 0;
        if (!readExactBytes(client, &b, 1, 1000)) return;
        encodedByte = b;
        remLen += (encodedByte & 0x7F) * multiplier;
        multiplier *= 128;
        bytesRead++;
    } while ((encodedByte & 0x80) != 0 && bytesRead < 4);

    slot.lastActivity = millis();
    String remoteIp = client.remoteIP().toString();

    // Special streaming handling for PUBLISH (type 3) so even huge historical dumps (>2KB)
    // are 100% consumed, ACKed with PUBACK, and their latest sensor reading is extracted!
    if (packetType == 3) {
        if (remLen < 2) return;
        uint8_t tLenBuf[2];
        if (!readExactBytes(client, tLenBuf, 2, 1500)) return;
        uint16_t topicLen = ((uint16_t)tLenBuf[0] << 8) | tLenBuf[1];

        if ((size_t)(2 + topicLen) > remLen || topicLen > 256) {
            return;
        }

        std::vector<uint8_t> topicBuf(topicLen);
        if (!readExactBytes(client, topicBuf.data(), topicLen, 1500)) return;
        String topic((const char*)topicBuf.data(), topicLen);

        size_t consumed = 2 + topicLen;
        uint8_t qos = (flags >> 1) & 0x03;
        uint8_t pktIdHi = 0, pktIdLo = 0;

        if (qos == 1 || qos == 2) {
            if (consumed + 2 > remLen) return;
            uint8_t pidBuf[2];
            if (!readExactBytes(client, pidBuf, 2, 1500)) return;
            pktIdHi = pidBuf[0];
            pktIdLo = pidBuf[1];
            consumed += 2;
        }

        size_t payloadLen = remLen - consumed;
        const size_t MAX_KEEP = 4096;
        std::vector<uint8_t> payloadBuf;

        if (payloadLen <= MAX_KEEP) {
            payloadBuf.resize(payloadLen);
            if (payloadLen > 0 && !readExactBytes(client, payloadBuf.data(), payloadLen, 2000)) {
                addLog(String("PUBLISH timeout reading ") + payloadLen + "B from " + remoteIp);
                return;
            }
        } else {
            // Large historical batch dump (> 4KB): read all bytes from socket and keep the last 4096 bytes
            // (which contain the newest sensorData reading at the end of the JSON array!)
            payloadBuf.resize(MAX_KEEP);
            size_t remaining = payloadLen;
            uint8_t chunk[512];
            size_t filled = 0;

            while (remaining > 0) {
                size_t toRead = min(remaining, sizeof(chunk));
                if (!readExactBytes(client, chunk, toRead, 2000)) {
                    addLog(String("PUBLISH large timeout (") + remaining + "B left) from " + remoteIp);
                    return;
                }
                if (filled + toRead <= MAX_KEEP) {
                    memcpy(payloadBuf.data() + filled, chunk, toRead);
                    filled += toRead;
                } else if (toRead >= MAX_KEEP) {
                    memcpy(payloadBuf.data(), chunk + (toRead - MAX_KEEP), MAX_KEEP);
                    filled = MAX_KEEP;
                } else {
                    size_t shift = (filled + toRead) - MAX_KEEP;
                    memmove(payloadBuf.data(), payloadBuf.data() + shift, filled - shift);
                    memcpy(payloadBuf.data() + (filled - shift), chunk, toRead);
                    filled = MAX_KEEP;
                }
                remaining -= toRead;
            }
        }

        // IMMEDIATELY acknowledge QoS 1 / QoS 2 so the Qingping clears its transmit queue!
        if (qos == 1) {
            const uint8_t puback[4] = {0x40, 0x02, pktIdHi, pktIdLo};
            client.write(puback, 4);
        } else if (qos == 2) {
            const uint8_t pubrec[4] = {0x50, 0x02, pktIdHi, pktIdLo};
            client.write(pubrec, 4);
        }

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
        if (payloadBuf.size() >= 2 && payloadBuf[0] == 0x43 && payloadBuf[1] == 0x47) {
            preview = String("[Binary CG TLV, ") + payloadLen + " bytes]";
        } else if (payloadBuf.size() > 0) {
            size_t showLen = min(payloadBuf.size(), (size_t)120);
            preview = String((const char*)payloadBuf.data(), showLen);
        }
        addLog(String("PUBLISH [") + topic + "] (QoS=" + qos + ", " + payloadLen + "B): " + preview);

        // Parse and update room telemetry
        VentilationController::instance().onQingpingPayload(topic, payloadBuf.data(), payloadBuf.size(), remoteIp);

        // Send 15s fast-report command after first PUBLISH or every hour
        if (slot.downTopic.length() > 0 && (slot.lastConfigSent == 0 || (millis() - slot.lastConfigSent > 3600000UL))) {
            sendFastReportConfig(client, slot.downTopic);
            slot.lastConfigSent = millis();
        }
        return;
    }

    // For non-PUBLISH packets (CONNECT, SUBSCRIBE, PINGREQ, etc.)
    if (remLen > 1024) {
        uint8_t dump[64];
        size_t rem = remLen;
        while (rem > 0) {
            size_t c = min(rem, sizeof(dump));
            if (!readExactBytes(client, dump, c, 1000)) break;
            rem -= c;
        }
        return;
    }

    std::vector<uint8_t> buf(remLen);
    if (remLen > 0 && !readExactBytes(client, buf.data(), remLen, 1500)) {
        return;
    }

    switch (packetType) {
        case 1: { // CONNECT
            const uint8_t connack[4] = {0x20, 0x02, 0x00, 0x00};
            client.write(connack, 4);

            String clientId = "";
            if (remLen >= 8) {
                uint16_t protoLen = ((uint16_t)buf[0] << 8) | buf[1];
                size_t cidOffset = 2 + protoLen + 4;
                if (cidOffset + 2 <= remLen) {
                    uint16_t cidLen = ((uint16_t)buf[cidOffset] << 8) | buf[cidOffset + 1];
                    if (cidOffset + 2 + cidLen <= remLen) {
                        clientId = String((const char*)(buf.data() + cidOffset + 2), cidLen);
                    }
                }
            }

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
                uint8_t reqQos = 0x00;
                if (4 + topicLen <= remLen) {
                    subTopic = String((const char*)(buf.data() + 4), topicLen);
                    if (4 + topicLen < remLen) {
                        reqQos = buf[4 + topicLen] & 0x01;
                    }
                }

                const uint8_t suback[5] = {0x90, 0x03, pktIdHi, pktIdLo, reqQos};
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
        case 4: { // PUBACK
            break;
        }
        case 6: { // PUBREL (QoS 2 step 2)
            if (remLen >= 2) {
                const uint8_t pubcomp[4] = {0x70, 0x02, buf[0], buf[1]};
                client.write(pubcomp, 4);
            }
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
        default: {
            addLog(String("MQTT pkt type=") + packetType + " len=" + remLen + " from " + remoteIp);
            break;
        }
    }
}

void QingpingBroker::update() {
    if (server.hasClient()) {
        WiFiClient incoming = server.available();
        if (incoming) {
            incoming.setNoDelay(true);
            String newIp = incoming.remoteIP().toString();

            // If there is already an older socket from the same IP, close it first
            int targetIdx = -1;
            for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
                if (slots[i].client && slots[i].client.connected() &&
                    slots[i].client.remoteIP().toString() == newIp) {
                    slots[i].client.stop();
                    targetIdx = i;
                    break;
                }
            }

            if (targetIdx < 0) {
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
            }

            slots[targetIdx].client = incoming;
            slots[targetIdx].mac = "";
            slots[targetIdx].downTopic = "";
            slots[targetIdx].lastActivity = millis();
            slots[targetIdx].lastConfigSent = 0;
            addLog(String("TCP connection opened from ") + newIp);
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
