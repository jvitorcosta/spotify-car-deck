#include <Arduino.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "net/wifi.h"
#include "spotify/auth.h"
#include "spotify/client.h"
#include "app_state.h"
#include "core/shared.h"
#include "core/nettask.h"
#include "core/mem.h"
#include "ui/screen_now.h"
#include "ui/cjkdata.h"
#include "ui/pokeball.h"
#include "ui/lyricmsg.h"
#include "util/genre.h"
#include "util/interp.h"
#include "util/netplan.h"
#include "images/art.h"
#include "images/walksprite.h"
#include "images/cache.h"
#include <esp_system.h>
#include <esp_task_wdt.h>

// Task watchdog: 30 s instead of the default 5 s. SpotifyArduino's response loops spin
// (yield() never lets the lower-priority idle task run) and on a reset TLS connection one spun
// ~11 s on core 0, so the idle task missed the 5 s watchdog and the board rebooted
// (the "unexplained rst:0xc"). A genuine hang still reboots, after 30 s.
static const uint32_t TASK_WDT_S = 30;

static const char* resetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_SW:       return "software (self-heal restart)";
        case ESP_RST_PANIC:    return "panic";
        case ESP_RST_INT_WDT:  return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT:      return "other watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_EXT:      return "external pin";
        default:               return "other";
    }
}

// UI loop (core 1): draws only. All network/SD work runs in core/nettask on core 0 and
// arrives through core/shared (README "Design & performance history").
TFT_eSPI tft = TFT_eSPI();

// The art box and the decoded bitmap are the same 92x92 area (pushArtIfValid pushes it whole).
static_assert(ui::ART_W == art::W && ui::ART_H == art::H, "art box and bitmap sizes differ");

static char g_topSig[96] = "";                 // last drawn top-strip state

// Pushes the album-art bitmap if it is valid for the track on screen. Check and push happen
// under one lock so the network task cannot start overwriting the bitmap in between.
static void pushArtIfValid(uint32_t gen) {
    shared::Guard g;
    if (shared::artValidLocked(gen))
        tft.pushImage(ui::ART_X, ui::ART_Y, art::W, art::H, art::bitmap());
}

static bool g_notesDance = false;   // a timed lyric line is on screen (set by drawDialogue)

// Dialogue box for the track on screen: the lyric line, or a Pokemon-style status message
// (searching / retrying / intro / no lyrics). The status age drives the FAIL -> IDLE switch.
static void drawDialogue(const AppState& view, uint32_t gen) {
    static lyricstatus::Status lastStatus = lyricstatus::Status::Searching;
    static uint32_t lastGen = 0, since = 0;
    static shared::LyricView lv;      // static: keep the loop stack small
    static lyricmsg::Out out;
    shared::lyricView(gen, view.progressMs, lv);
    uint32_t now = millis();
    if (gen != lastGen || lv.status != lastStatus) {
        lastGen = gen;
        lastStatus = lv.status;
        since = now;
    }
    lyricmsg::In in{lv.status, now - since, view.progressMs, lv.firstLineMs, lv.line,
                    view.pokeName, gen};
    lyricmsg::compose(in, out);
    ui::drawLyricArea(tft, out.text, out.notes);
    g_notesDance = out.dance;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("[boot] reset reason: %s\n", resetReason());
    esp_task_wdt_init(TASK_WDT_S, true);   // reconfigures the already-running watchdog
    mem::installFailHook();
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);
    shared::begin();
    walk::begin();            // allocate both walker slots before the heap fragments
    art::begin();             // fixed album-art bitmap (before WiFi)
    {   // CJK font sanity check (glyphs stay in flash)
        int w = 0;
        bool ok = ui::cjk().glyph(0x3042, &w) != nullptr;   // HIRAGANA LETTER A
        Serial.printf("[cjk] ranges=%d glyph(U+3042)=%s w=%d\n", ui::cjk().rangeCount(),
                      ok ? "ok" : "missing", w);
    }
    cache::begin();
    tft.init(); tft.invertDisplay(true); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting WiFi...", 10, 10, 2);

    bool spotifyReady = false;
    if (net::connectAny()) {
        mem::log("boot+wifi");
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Spotify auth...", 10, 10, 2);
        if (spauth::loadRefreshToken().isEmpty()) {
            tft.drawString("Open http://" + net::deviceIp() + "/", 10, 40, 2);
        }
        spauth::runSetupPortalIfNeeded();
        spclient::begin();
        spotifyReady = true;
    } else {
        tft.drawString("WiFi not found - retrying...", 10, 40, 2);
    }
    nettask::start(spotifyReady);   // always: it keeps retrying WiFi if the hotspot is late
}

void loop() {
    AppState st;
    shared::snapshot(st);
    AppState view = st;   // interpolate progress for a smooth bar between polls
    view.progressMs = interp::currentProgressMs(st.progressMs, st.durationMs, st.isPlaying,
                                                millis() - st.lastPollMs);

    // Screen mode: deck when playing/paused; a status screen when offline or stopped.
    int mode = 0;   // 0 = deck, 1 = offline, 2 = nothing playing
    if (!net::isOnline() || st.status == PlaybackStatus::Offline ||
        netplan::stale(millis(), st.lastPollOkMs, 20000)) mode = 1;
    else if (st.status == PlaybackStatus::Stopped) mode = 2;

    static int lastMode = -1;
    static uint32_t shownGen = 0;
    static bool walkerOn = false;
    static uint8_t shownGenre = genre::NONE;   // badge for the track on screen
    if (mode != 0) {
        if (lastMode != mode) {   // draw the status screen once (no flicker)
            ui::drawOffline(tft, mode == 1 ? "No signal..." : "Nothing playing");
            lastMode = mode;
        }
    } else if (lastMode != 0 || st.trackGen != shownGen) {
        // Full deck redraw: immediately on track change (text + Pokemon name; art, lyric
        // and walker fill in as the network task delivers them), or back from a status screen.
        bool newTrack = st.trackGen != shownGen;
        lastMode = 0;
        if (newTrack) {
            shownGenre = genre::NONE;
            shownGen = st.trackGen;
            walkerOn = false;
        }
        uint8_t cached;   // a cached badge is posted with the track change: draw it with the title
        if (shared::takeGenre(shownGen, &cached)) shownGenre = cached;
        ui::drawNow(tft, view, shownGenre);
        g_topSig[0] = '\0';
        pushArtIfValid(shownGen);   // back from a status screen: same track's art is still valid
        drawDialogue(view, shownGen);
        mem::log("track");
    } else {
        // Media arriving from the network task for the track on screen.
        if (shared::takeArt(shownGen)) pushArtIfValid(shownGen);
        if (shared::takeWalker(shownGen)) walkerOn = true;
        uint8_t g;
        if (shared::takeGenre(shownGen, &g)) {   // genre arrived (or was cached): artist row only
            shownGenre = g;
            ui::drawArtistRow(tft, view, g);
        }

        static uint32_t lastDraw = 0, lastWalk = 0, lastCd = 0, lastTick = 0, animMs = 0;
        static int walkStep = 0, cdFrame = 0, ballFrame = 0;
        uint32_t now = millis();
        uint32_t dt = now - lastTick;
        lastTick = now;
        if (st.isPlaying) animMs += dt;   // walk cycle runs only while playing

        // top strip: redraw only when device / shuffle / repeat change
        char sig[96];
        snprintf(sig, sizeof(sig), "%s|%s|%d|%d|%d", st.deviceName, st.deviceType,
                 (int)st.shuffle, st.repeat, (int)st.isPlaying);
        if (strcmp(sig, g_topSig) != 0) {
            strcpy(g_topSig, sig);
            ui::drawTopStrip(tft, st);
        }
        if (st.isPlaying && now - lastCd >= 160) {   // spinning CD ~6 fps
            lastCd = now;
            ui::drawCdFrame(tft, cdFrame = (cdFrame + 1) & 3);
            ui::drawPokeballFrame(tft, ballFrame = (ballFrame + 1) % pokeball::FRAMES);
        }
        if (now - lastDraw >= 250) {
            lastDraw = now;
            ui::drawProgressRegion(tft, view);
            drawDialogue(view, shownGen);
        }
        // Note icons bob while timed lyrics are shown and the music plays; at rest otherwise.
        static uint32_t lastNotes = 0;
        static int noteFrame = 0;
        static bool notesMoved = false;
        if (g_notesDance && st.isPlaying) {
            if (now - lastNotes >= 330) {            // ~3 steps/s: alive, not distracting
                lastNotes = now;
                ui::drawNoteFrame(tft, ++noteFrame & 0x7FFF);
                notesMoved = true;
            }
        } else if (notesMoved) {
            ui::drawNoteFrame(tft, -1);
            notesMoved = false;
        }
        if (walkerOn && now - lastWalk >= 120) {     // ~8 fps walker
            lastWalk = now;
            if (st.isPlaying) walkStep++;
            ui::drawWalker(tft, view, animMs, walkStep);
        }
    }
    delay(10);
}
