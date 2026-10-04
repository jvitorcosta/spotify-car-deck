#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "net/wifi.h"
#include "spotify/auth.h"

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);
    tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting WiFi...", 10, 10, 2);

    if (net::connectAny()) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Online: " + net::deviceIp(), 10, 10, 2);

        tft.fillScreen(TFT_BLACK);
        tft.drawString("Spotify auth...", 10, 10, 2);
        if (spauth::loadRefreshToken().isEmpty()) {
            tft.drawString("Open http://" + net::deviceIp() + "/", 10, 40, 2);
        }
        spauth::runSetupPortalIfNeeded();
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Auth OK", 10, 10, 2);
    } else {
        tft.drawString("WiFi FAILED", 10, 40, 2);
    }
}

void loop() { net::loop(); delay(50); }
