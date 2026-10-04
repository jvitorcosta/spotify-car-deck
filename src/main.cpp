#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "net/wifi.h"
#include "spotify/auth.h"
#include "spotify/client.h"
#include "app_state.h"
#include "ui/theme.h"
#include "ui/screen_now.h"

TFT_eSPI tft = TFT_eSPI();
AppState g_state{};

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
        spclient::begin();
    } else {
        tft.drawString("WiFi FAILED", 10, 40, 2);
    }

    // Task 9: render the static GBA deck once with dummy data so the layout
    // can be eyeballed. Replaces the live poll's redraw until Task 10.
    strcpy(g_state.trackName, "Mr. Blue Sky");
    strcpy(g_state.artist, "Electric Light Orchestra");
    strcpy(g_state.context, "Discover Weekly");
    strcpy(g_state.pokeName, "Lapras");
    strcpy(g_state.deviceName, "Living Room");
    g_state.progressMs = 102000; g_state.durationMs = 238000;
    g_state.isPlaying = true; g_state.popularity = 72;
    ui::drawNow(tft, g_state, theme::typeColor("water"));
}

void loop() {
    net::loop();
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= 4000) {
        lastPoll = millis();
        bool changed = spclient::poll(g_state);
        Serial.printf("[poll] status=%d track=%s %u/%u changed=%d\n",
            (int)g_state.status, g_state.trackName,
            g_state.progressMs, g_state.durationMs, changed);
        // Task 9: dummy static render only — live redraw wired in Task 10.
        (void)changed;
    }
    delay(20);
}
