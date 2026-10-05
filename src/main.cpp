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
#include "lyrics/lrclib.h"
#include "util/lrc.h"
#include <vector>

TFT_eSPI tft = TFT_eSPI();
AppState g_state{};

static lyricsvc::Result g_lyrics{lyricsvc::Kind::None, ""};
static std::vector<lrc::LrcLine> g_lrcLines;   // parsed when synced lyrics exist

// Current synced lyric line for a playback position ("" if none).
static const char* currentLyric(uint32_t posMs) {
    if (g_lrcLines.empty()) return "";
    int idx = lrc::currentIndex(g_lrcLines, posMs);
    return (idx >= 0) ? g_lrcLines[idx].text.c_str() : "";
}

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

        // Fetch synced lyrics for this track (uses original accented names).
        g_lyrics = lyricsvc::fetch(g_state);
        g_lrcLines = (g_lyrics.kind == lyricsvc::Kind::Synced)
                         ? lrc::parse(g_lyrics.text)
                         : std::vector<lrc::LrcLine>{};
        Serial.printf("[lyrics] kind=%d lines=%u\n",
                      (int)g_lyrics.kind, (unsigned)g_lrcLines.size());

        uint16_t accent = theme::typeColor(g_state.pokeType);
        ui::drawNow(tft, view, accent);
        img::drawAlbumArt(tft, 11, 29, 98, 98);
        img::drawSprite(tft, g_state.pokeSpriteUrl, g_state.pokedexNum, 60, 185);
        ui::drawLyricArea(tft, currentLyric(view.progressMs));

        strcpy(lastTrack, g_state.trackName);
    } else {
        static uint32_t lastDraw = 0;
        if (millis() - lastDraw >= 250) {   // ~4 fps redraw is plenty
            lastDraw = millis();
            ui::drawProgressRegion(tft, view);   // shared with drawNow() — single source
            ui::drawLyricArea(tft, currentLyric(view.progressMs));
        }
    }
    delay(10);
}
