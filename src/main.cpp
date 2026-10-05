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
#include "util/interp.h"
#include "images/jpeg.h"
#include "images/png.h"
#include "images/cache.h"
#include "pokemon/pokeapi.h"

TFT_eSPI tft = TFT_eSPI();
AppState g_state{};

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);
    cache::begin();
    tft.init(); tft.invertDisplay(true); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
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
}

void loop() {
    net::loop();
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= 4000) {
        lastPoll = millis();
        spclient::poll(g_state);
    }
    // interpolate progress for a smooth bar between polls
    AppState view = g_state;
    view.progressMs = interp::currentProgressMs(
        g_state.progressMs, g_state.durationMs, g_state.isPlaying,
        millis() - g_state.lastPollMs);

    // Dirty-redraw: a full drawNow() (and a fresh album-art fetch) only
    // happens when the track changes. Otherwise we repaint just the
    // progress region every ~250ms so the art isn't wiped every frame.
    static char lastTrack[96] = "";
    if (strcmp(lastTrack, g_state.trackName) != 0) {
        pokeapi::pickRandom(g_state);    // fresh random Pokemon each play
        view.pokedexNum = g_state.pokedexNum;
        strncpy(view.pokeName, g_state.pokeName, sizeof(view.pokeName));
        strncpy(view.pokeType, g_state.pokeType, sizeof(view.pokeType));

        img::cacheAlbumArt(g_state.albumArtUrl);

        uint16_t accent = theme::typeColor(g_state.pokeType);
        ui::drawNow(tft, view, accent);
        img::drawAlbumArt(tft, 11, 31, 98, 98);
        img::drawSprite(tft, g_state.pokeSpriteUrl, g_state.pokedexNum, 60, 176);

        strcpy(lastTrack, g_state.trackName);
    } else {
        static uint32_t lastDraw = 0;
        if (millis() - lastDraw >= 250) {   // ~4 fps redraw is plenty
            lastDraw = millis();
            ui::drawProgressRegion(tft, view);   // shared with drawNow() — single source
        }
    }
    delay(10);
}
