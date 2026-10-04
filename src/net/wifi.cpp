#include "wifi.h"
#include <WiFi.h>
#include "../config.h"

namespace net {
static uint32_t lastCheck = 0;

bool connectAny() {
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < WIFI_NETWORK_COUNT; ++i) {
        Serial.printf("[wifi] trying %s\n", WIFI_NETWORKS[i].ssid);
        WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
        uint32_t start = millis();
        while (millis() - start < 8000) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[wifi] connected to %s ip=%s\n",
                              WIFI_NETWORKS[i].ssid, WiFi.localIP().toString().c_str());
                return true;
            }
            delay(200);
        }
        Serial.printf("[wifi] %s failed\n", WIFI_NETWORKS[i].ssid);
        WiFi.disconnect(true);
    }
    return false;
}

bool isOnline() { return WiFi.status() == WL_CONNECTED; }
String deviceIp() { return WiFi.localIP().toString(); }

void loop() {
    if (millis() - lastCheck < 5000) return;
    lastCheck = millis();
    if (!isOnline()) {
        Serial.println("[wifi] dropped, reconnecting...");
        connectAny();
    }
}
}
