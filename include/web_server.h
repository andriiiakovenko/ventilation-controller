#pragma once

#include <Arduino.h>
#include <WebServer.h>

class WebServerManager {
public:
    static WebServerManager& instance();

    void init();
    void update(); // Called inside main loop (handles HTTP client requests & scheduled reboots)
    void scheduleReboot(unsigned long delayMs);

private:
    WebServerManager();
    WebServer server;
    bool shouldReboot = false;
    unsigned long rebootAt = 0;

    void setupRoutes();
    String buildStatusJson();
};

