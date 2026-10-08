#include "wifi.h"
#include <WiFi.h>
#include "../config.h"

namespace net {
static uint32_t s_lastCheck = 0;
constexpr uint32_t CONNECT_TRY_MS = 8000;   // per network in connectAny()
constexpr uint32_t CHECK_MS = 5000;         // loop(): reconnect check period

bool connectAny() {
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < WIFI_NETWORK_COUNT; ++i) {
        Serial.printf("[wifi] trying %s\n", WIFI_NETWORKS[i].ssid);
        WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
        uint32_t start = millis();
        while (millis() - start < CONNECT_TRY_MS) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[wifi] connected to %s ip=%s\n",
                              WIFI_NETWORKS[i].ssid, WiFi.localIP().toString().c_str());
                return true;
            }
            delay(200);
        }
        Serial.printf("[wifi] %s failed\n", WIFI_NETWORKS[i].ssid);
        WiFi.disconnect();   // keep the driver up: disconnect(true) deinit/re-inits it each try
    }
    return false;
}

bool isOnline() { return WiFi.status() == WL_CONNECTED; }
String deviceIp() { return WiFi.localIP().toString(); }

void loop() {
    if (millis() - s_lastCheck < CHECK_MS) return;
    s_lastCheck = millis();
    if (!isOnline()) {
        Serial.println("[wifi] dropped, reconnecting...");
        connectAny();
    }
}
}
