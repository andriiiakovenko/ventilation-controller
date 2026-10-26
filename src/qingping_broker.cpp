#include "qingping_broker.h"
#include "ventilation_controller.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>
#include <fcntl.h>

#undef write
#undef read
#undef close

QingpingBroker& QingpingBroker::instance() {
    static QingpingBroker _inst;
    return _inst;
}

QingpingBroker::QingpingBroker() {}

void QingpingBroker::init() {
    if (listenFd >= 0) {
        lwip_close(listenFd);
        listenFd = -1;
    }

    listenFd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenFd < 0) {
        addLog(String("ERROR: lwip_socket failed errno=") + errno);
        return;
    }

    int enable = 1;
    lwip_setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(QINGPING_BROKER_PORT);

    if (lwip_bind(listenFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        addLog(String("ERROR: lwip_bind port 1883 failed errno=") + errno);
        lwip_close(listenFd);
        listenFd = -1;
        return;
    }

    if (lwip_listen(listenFd, QINGPING_MAX_CLIENTS) < 0) {
        addLog(String("ERROR: lwip_listen failed errno=") + errno);
        lwip_close(listenFd);
        listenFd = -1;
        return;
    }

    int flags = lwip_fcntl(listenFd, F_GETFL, 0);
    lwip_fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);

    addLog("Port 1883 Direct MQTT Broker started (non-blocking LwIP)");
}

void QingpingBroker::addLog(const String& entry) {
    unsigned long sec = millis() / 1000UL;
    String line = String("[") + sec + "s] " + entry;
    Serial.printf("[QingpingBroker] %s\n", line.c_str());
    recentLogs.push_back(line);
    if (recentLogs.size() > 12) {
        recentLogs.erase(recentLogs.begin());
    }
}

std::vector<String> QingpingBroker::getRecentLogs() const {
    return recentLogs;
}

int QingpingBroker::getConnectedClientCount() const {
    int count = 0;
    for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
        if (slots[i].fd >= 0) {
            count++;
        }
    }
    return count;
}

bool QingpingBroker::isMacConnected(const String& normMac) const {
    if (normMac.length() != 12) return false;
    for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
        if (slots[i].fd >= 0 && slots[i].mac == normMac) {
            return true;
        }
    }
    return false;
}

void QingpingBroker::closeSlot(QingpingClientSlot& slot, const char* reason) {
    if (slot.fd >= 0) {
        if (reason) {
            addLog(String("Closed ") + slot.remoteIp + " (" + reason + ")");
        }
        lwip_close(slot.fd);
        slot.fd = -1;
    }
    slot.rxBuf.clear();
    slot.discardBytes = 0;
    slot.pendingAckQos = 0;
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

bool QingpingBroker::sendAll(int fd, const uint8_t* data, size_t len) {
    if (fd < 0 || !data || len == 0) return false;
    size_t sent = 0;
    int retries = 20;
    while (sent < len && retries-- > 0) {
        int r = lwip_send(fd, data + sent, len - sent, MSG_DONTWAIT);
        if (r > 0) {
            sent += (size_t)r;
        } else if (r < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            delay(1);
        } else {
            return false;
        }
    }
    return (sent == len);
}

void QingpingBroker::publishToClient(int fd, const String& topic, const String& payload) {
    if (fd < 0) return;

    size_t topicLen = topic.length();
    size_t payloadLen = payload.length();
    size_t remLen = 2 + topicLen + payloadLen;

    uint8_t header[8];
    size_t hIdx = 0;
    header[hIdx++] = 0x30; // PUBLISH, QoS 0, no retain
    encodeRemainingLength(header, hIdx, remLen);
    header[hIdx++] = (uint8_t)((topicLen >> 8) & 0xFF);
    header[hIdx++] = (uint8_t)(topicLen & 0xFF);

    std::vector<uint8_t> pkt(hIdx + topicLen + payloadLen);
    memcpy(pkt.data(), header, hIdx);
    memcpy(pkt.data() + hIdx, topic.c_str(), topicLen);
    memcpy(pkt.data() + hIdx + topicLen, payload.c_str(), payloadLen);
    sendAll(fd, pkt.data(), pkt.size());
}

void QingpingBroker::sendFastReportConfig(QingpingClientSlot& slot) {
    if (slot.fd < 0 || slot.downTopic.length() == 0) return;

    // 1. Request real-time 15-second upload stream for 24h (type 12)
    String cfg12 = "{\"type\":\"12\",\"up_itvl\":\"15\",\"duration\":\"86400\"}";
    publishToClient(slot.fd, slot.downTopic, cfg12);

    // 2. Set permanent sampling & report interval to 15s (type 17)
    String cfg17 = "{\"id\":1,\"need_ack\":1,\"type\":\"17\",\"setting\":{\"report_interval\":15,\"collect_interval\":15,\"co2_sampling_interval\":15,\"pm_sampling_interval\":15}}";
    publishToClient(slot.fd, slot.downTopic, cfg17);

    addLog(String("Sent 15s live-stream config to [") + slot.downTopic + "]");
}

bool QingpingBroker::processNextPacket(QingpingClientSlot& slot) {
    if (slot.rxBuf.size() < 2) return false;

    uint8_t b0 = slot.rxBuf[0];
    uint8_t packetType = (b0 >> 4) & 0x0F;
    uint8_t flags = b0 & 0x0F;

    size_t remLen = 0;
    size_t multiplier = 1;
    size_t idx = 1;

    while (true) {
        if (idx >= slot.rxBuf.size()) {
            // Need more header bytes to finish decoding Remaining Length
            return false;
        }
        uint8_t encodedByte = slot.rxBuf[idx++];
        remLen += (encodedByte & 0x7F) * multiplier;
        multiplier *= 128;
        if ((encodedByte & 0x80) == 0) {
            break;
        }
        if (idx >= 5) {
            // Malformed MQTT Remaining Length (> 4 bytes)
            closeSlot(slot, "malformed remLen");
            return false;
        }
    }

    size_t headerLen = idx;
    size_t totalPktLen = headerLen + remLen;

    // If a huge historical dump (> 2048B) arrives, stream-discard its body without allocating RAM
    // and send PUBACK as soon as all bytes are drained so the Qingping clears its history buffer!
    if (totalPktLen > 2048) {
        if (packetType != 3) {
            closeSlot(slot, "non-PUBLISH > 2KB");
            return false;
        }
        if (slot.rxBuf.size() < headerLen + 2) {
            return false;
        }
        const uint8_t* b = slot.rxBuf.data() + headerLen;
        uint16_t topicLen = ((uint16_t)b[0] << 8) | b[1];
        uint8_t qos = (flags >> 1) & 0x03;
        size_t minNeeded = headerLen + 2 + topicLen + ((qos == 1 || qos == 2) ? 2 : 0);
        if (topicLen > 256 || minNeeded > totalPktLen) {
            closeSlot(slot, "invalid large PUBLISH");
            return false;
        }
        if (slot.rxBuf.size() < minNeeded) {
            return false;
        }

        String topic((const char*)(b + 2), topicLen);
        slot.pendingAckQos = qos;
        if (qos == 1 || qos == 2) {
            slot.pendingAckHi = b[2 + topicLen];
            slot.pendingAckLo = b[2 + topicLen + 1];
        }

        addLog(String("Draining large history PUBLISH [") + topic + "] (" + remLen + "B, QoS=" + qos + ")");
        if (slot.rxBuf.size() >= totalPktLen) {
            slot.rxBuf.erase(slot.rxBuf.begin(), slot.rxBuf.begin() + totalPktLen);
            if (qos == 1) {
                const uint8_t puback[4] = {0x40, 0x02, slot.pendingAckHi, slot.pendingAckLo};
                sendAll(slot.fd, puback, 4);
            } else if (qos == 2) {
                const uint8_t pubrec[4] = {0x50, 0x02, slot.pendingAckHi, slot.pendingAckLo};
                sendAll(slot.fd, pubrec, 4);
            }
            slot.pendingAckQos = 0;
            return (slot.rxBuf.size() >= 2);
        } else {
            slot.discardBytes = totalPktLen - slot.rxBuf.size();
            slot.rxBuf.clear();
            return false;
        }
    }

    // Wait until the entire MQTT packet has arrived in rxBuf
    if (slot.rxBuf.size() < totalPktLen) {
        return false;
    }

    const uint8_t* body = slot.rxBuf.data() + headerLen;
    slot.lastActivity = millis();

    switch (packetType) {
        case 1: { // CONNECT
            const uint8_t connack[4] = {0x20, 0x02, 0x00, 0x00};
            sendAll(slot.fd, connack, 4);

            String clientId = "";
            uint16_t keepAlive = 0;
            uint8_t connFlags = 0;
            if (remLen >= 8) {
                uint16_t protoLen = ((uint16_t)body[0] << 8) | body[1];
                if (2 + protoLen + 4 <= remLen) {
                    connFlags = body[2 + protoLen + 1];
                    keepAlive = ((uint16_t)body[2 + protoLen + 2] << 8) | body[2 + protoLen + 3];
                }
                size_t cidOffset = 2 + protoLen + 4;
                if (cidOffset + 2 <= remLen) {
                    uint16_t cidLen = ((uint16_t)body[cidOffset] << 8) | body[cidOffset + 1];
                    if (cidOffset + 2 + cidLen <= remLen) {
                        clientId = String((const char*)(body + cidOffset + 2), cidLen);
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
                    VentilationController::instance().onQingpingConnected(normMac, slot.remoteIp);
                }
            }

            addLog(String("CONNECT from ") + slot.remoteIp + " (id=" + clientId + ", ka=" + keepAlive + "s)");
            break;
        }
        case 3: { // PUBLISH
            if (remLen >= 2) {
                uint16_t topicLen = ((uint16_t)body[0] << 8) | body[1];
                size_t consumed = 2 + topicLen;
                uint8_t qos = (flags >> 1) & 0x03;
                uint8_t pktIdHi = 0, pktIdLo = 0;

                if (consumed <= remLen && topicLen <= 256) {
                    String topic((const char*)(body + 2), topicLen);
                    if (qos == 1 || qos == 2) {
                        if (consumed + 2 <= remLen) {
                            pktIdHi = body[consumed];
                            pktIdLo = body[consumed + 1];
                            consumed += 2;
                        }
                    }

                    if (consumed <= remLen) {
                        const uint8_t* payloadPtr = body + consumed;
                        size_t payloadLen = remLen - consumed;

                        // Acknowledge QoS 1 / QoS 2 immediately
                        if (qos == 1) {
                            const uint8_t puback[4] = {0x40, 0x02, pktIdHi, pktIdLo};
                            sendAll(slot.fd, puback, 4);
                        } else if (qos == 2) {
                            const uint8_t pubrec[4] = {0x50, 0x02, pktIdHi, pktIdLo};
                            sendAll(slot.fd, pubrec, 4);
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

                        String preview = "";
                        if (payloadLen >= 2 && payloadPtr[0] == 0x43 && payloadPtr[1] == 0x47) {
                            preview = String("[Binary CG TLV, ") + payloadLen + " bytes]";
                            Serial.printf("[QingpingBroker] CG TLV HEX (%uB): ", (unsigned)payloadLen);
                            for (size_t k = 0; k < min(payloadLen, (size_t)128); k++) {
                                Serial.printf("%02X ", payloadPtr[k]);
                            }
                            Serial.println();
                        } else if (payloadLen > 0) {
                            size_t showLen = min(payloadLen, (size_t)96);
                            const char* startPtr = (const char*)payloadPtr + (payloadLen - showLen);
                            preview = String(startPtr, showLen);
                            preview.replace("\n", " ");
                            preview.replace("\r", "");
                            preview.replace("\t", " ");
                            Serial.printf("[QingpingBroker] JSON Payload (%uB): %.*s\n",
                                          (unsigned)payloadLen,
                                          (int)min(payloadLen, (size_t)1024),
                                          (const char*)payloadPtr);
                        }
                        addLog(String("PUBLISH [") + topic + "] (QoS=" + qos + ", " + payloadLen + "B): " + preview);

                        VentilationController::instance().onQingpingPayload(topic, payloadPtr, payloadLen, slot.remoteIp);

                        if (slot.downTopic.length() > 0 && (slot.lastConfigSent == 0 || (millis() - slot.lastConfigSent > 3600000UL))) {
                            sendFastReportConfig(slot);
                            slot.lastConfigSent = millis();
                        }
                    }
                }
            }
            break;
        }
        case 4: { // PUBACK
            addLog(String("PUBACK from ") + slot.remoteIp);
            break;
        }
        case 6: { // PUBREL
            if (remLen >= 2) {
                const uint8_t pubcomp[4] = {0x70, 0x02, body[0], body[1]};
                sendAll(slot.fd, pubcomp, 4);
            }
            break;
        }
        case 8: { // SUBSCRIBE
            if (remLen >= 5) {
                uint8_t pktIdHi = body[0];
                uint8_t pktIdLo = body[1];
                uint16_t topicLen = ((uint16_t)body[2] << 8) | body[3];
                String subTopic = "";
                uint8_t reqQos = 0x00;
                if (4 + topicLen <= remLen) {
                    subTopic = String((const char*)(body + 4), topicLen);
                    if (4 + topicLen < remLen) {
                        reqQos = body[4 + topicLen] & 0x01;
                    }
                }

                const uint8_t suback[5] = {0x90, 0x03, pktIdHi, pktIdLo, reqQos};
                sendAll(slot.fd, suback, 5);

                addLog(String("SUBSCRIBE from ") + slot.remoteIp + " to [" + subTopic + "]");

                if (subTopic.length() > 0) {
                    slot.downTopic = subTopic;
                    if (subTopic.startsWith("qingping/")) {
                        int secondSlash = subTopic.indexOf('/', 9);
                        if (secondSlash > 9) {
                            slot.mac = VentilationController::normalizeMac(subTopic.substring(9, secondSlash));
                            VentilationController::instance().onQingpingConnected(slot.mac, slot.remoteIp);
                        }
                    }
                    sendFastReportConfig(slot);
                    slot.lastConfigSent = millis();
                }
            }
            break;
        }
        case 12: { // PINGREQ
            const uint8_t pingresp[2] = {0xD0, 0x00};
            sendAll(slot.fd, pingresp, 2);
            if (slot.mac.length() == 12) {
                VentilationController::instance().onQingpingConnected(slot.mac, slot.remoteIp);
            }
            addLog(String("PINGREQ from ") + slot.remoteIp);
            break;
        }
        case 14: { // DISCONNECT
            addLog(String("DISCONNECT from ") + slot.remoteIp);
            closeSlot(slot);
            return false;
        }
        default: {
            addLog(String("MQTT pkt type=") + packetType + " len=" + remLen + " from " + slot.remoteIp);
            break;
        }
    }

    if (slot.fd >= 0 && slot.rxBuf.size() >= totalPktLen) {
        slot.rxBuf.erase(slot.rxBuf.begin(), slot.rxBuf.begin() + totalPktLen);
    }
    return (slot.fd >= 0 && slot.rxBuf.size() >= 2);
}

void QingpingBroker::update() {
    if (listenFd < 0) return;

    // 1. Accept any new incoming TCP connections
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = lwip_accept(listenFd, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientFd < 0) {
            break;
        }

        int flags = lwip_fcntl(clientFd, F_GETFL, 0);
        lwip_fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
        int one = 1;
        lwip_setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        lwip_setsockopt(clientFd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

        char ipStr[20] = {0};
        inet_ntoa_r(clientAddr.sin_addr, ipStr, sizeof(ipStr));
        String newIp = String(ipStr);

        // Drain and replace any existing slot from the same IP
        int targetIdx = -1;
        for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
            if (slots[i].fd >= 0 && slots[i].remoteIp == newIp) {
                uint8_t tmp[512];
                while (true) {
                    int r = lwip_recv(slots[i].fd, tmp, sizeof(tmp), MSG_DONTWAIT);
                    if (r > 0) {
                        slots[i].rxBuf.insert(slots[i].rxBuf.end(), tmp, tmp + r);
                    } else {
                        break;
                    }
                }
                while (slots[i].fd >= 0 && processNextPacket(slots[i])) {}
                closeSlot(slots[i]);
                targetIdx = i;
                break;
            }
        }

        if (targetIdx < 0) {
            unsigned long oldestActivity = ULONG_MAX;
            int oldestIdx = 0;
            for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
                if (slots[i].fd < 0) {
                    targetIdx = i;
                    break;
                }
                if (slots[i].lastActivity < oldestActivity) {
                    oldestActivity = slots[i].lastActivity;
                    oldestIdx = i;
                }
            }
            if (targetIdx < 0) {
                closeSlot(slots[oldestIdx], "evicted oldest");
                targetIdx = oldestIdx;
            }
        }

        slots[targetIdx].fd = clientFd;
        slots[targetIdx].remoteIp = newIp;
        slots[targetIdx].mac = "";
        slots[targetIdx].downTopic = "";
        slots[targetIdx].rxBuf.clear();
        slots[targetIdx].rxBuf.reserve(2048);
        slots[targetIdx].discardBytes = 0;
        slots[targetIdx].pendingAckQos = 0;
        slots[targetIdx].lastActivity = millis();
        slots[targetIdx].lastConfigSent = 0;
        addLog(String("TCP connection opened from ") + newIp);
    }

    // 2. Read incoming bytes and process complete MQTT packets on all active slots
    unsigned long now = millis();
    uint8_t tmp[512];
    for (int i = 0; i < QINGPING_MAX_CLIENTS; i++) {
        QingpingClientSlot& slot = slots[i];
        if (slot.fd < 0) continue;

        bool peerClosed = false;
        bool recvError = false;
        int errCode = 0;

        while (slot.fd >= 0) {
            size_t maxRead = sizeof(tmp);
            if (slot.discardBytes > 0) {
                maxRead = min(maxRead, slot.discardBytes);
            } else {
                if (slot.rxBuf.size() >= 2048) {
                    while (slot.fd >= 0 && processNextPacket(slot)) {}
                    if (slot.rxBuf.size() >= 2048) {
                        closeSlot(slot, "rxBuf full");
                        break;
                    }
                }
                maxRead = min(maxRead, (size_t)(2048 - slot.rxBuf.size()));
            }

            int r = lwip_recv(slot.fd, tmp, maxRead, MSG_DONTWAIT);
            if (r > 0) {
                size_t got = (size_t)r;
                if (slot.discardBytes > 0) {
                    slot.discardBytes -= got;
                    if (slot.discardBytes == 0 && slot.pendingAckQos > 0) {
                        if (slot.pendingAckQos == 1) {
                            const uint8_t puback[4] = {0x40, 0x02, slot.pendingAckHi, slot.pendingAckLo};
                            sendAll(slot.fd, puback, 4);
                        } else if (slot.pendingAckQos == 2) {
                            const uint8_t pubrec[4] = {0x50, 0x02, slot.pendingAckHi, slot.pendingAckLo};
                            sendAll(slot.fd, pubrec, 4);
                        }
                        slot.pendingAckQos = 0;
                    }
                } else {
                    slot.rxBuf.insert(slot.rxBuf.end(), tmp, tmp + got);
                    while (slot.fd >= 0 && processNextPacket(slot)) {}
                }
                slot.lastActivity = now;
            } else if (r == 0) {
                peerClosed = true;
                break;
            } else {
                if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                    recvError = true;
                    errCode = errno;
                }
                break;
            }
        }

        // Process any complete packets in rxBuf before handling close
        while (slot.fd >= 0 && processNextPacket(slot)) {}

        if (slot.fd >= 0) {
            unsigned long checkNow = millis();
            if (peerClosed) {
                closeSlot(slot, "EOF");
            } else if (recvError) {
                String reason = String("errno=") + errCode;
                closeSlot(slot, reason.c_str());
            } else if (checkNow > slot.lastActivity && (checkNow - slot.lastActivity > 300000UL)) {
                closeSlot(slot, "idle timeout");
            }
        }
    }
}
