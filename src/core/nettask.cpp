#include "nettask.h"
#include <Arduino.h>
#include <esp_system.h>
#include <vector>
#include "shared.h"
#include "../net/wifi.h"
#include "../spotify/client.h"
#include "../images/jpeg.h"
#include "../images/walksprite.h"
#include "../lyrics/lrclib.h"
#include "../pokemon/pick.h"
#include "../pokemon/dex.h"
#include "../util/lrc.h"
#include "../util/netplan.h"

// History (README "Design & performance history"): these calls used to run inline in
// loop(). Each is a fresh TLS handshake (poll ~1.5 s, player ~1.4 s, PokeAPI 4.2 s, art
// 2.5 s, lyrics 3.8 s, walk sheet 3.5 s), so the display froze during every poll and a
// track change took ~19 s to appear. Here they block only this task.
namespace nettask {

static AppState s_st{};            // this task's private copy; published after changes
static netplan::TrackGen s_gen;
static netplan::Work s_work{};
static netplan::LinkGate s_link;   // "No signal" only after several failed polls in a row
static int s_prefetchDex = 0;      // dex whose walker sits in the staged slot (Task 12)
static uint32_t s_t0 = 0;

static void tick() { s_t0 = millis(); }
static void tock(const char* what) {
    Serial.printf("[net] %s %lums\n", what, (unsigned long)(millis() - s_t0));
}

static void onTrackChange() {
    s_st.trackGen = s_gen.gen();
    pick::choose(s_st, dex::fromRandom(esp_random()));
    s_prefetchDex = 0;
    shared::publish(s_st);                 // UI shows the new title + Pokemon right away
    s_work = netplan::freshWork(false);
    Serial.printf("[net] track gen=%u\n", (unsigned)s_st.trackGen);
}

static void doStep(netplan::Step step) {
    const uint32_t gen = s_st.trackGen;
    switch (step) {
        case netplan::Step::Walk: {
            tick();
            bool ok = walk::loadPmd(s_st.pokedexNum) ||
                      walk::loadFallback(s_st.pokeSpriteUrl, s_st.pokedexNum);
            tock("walk");
            if (ok) {
                { shared::Guard g; walk::promote(); }
                shared::postWalker(gen);
            }
            break;
        }
        case netplan::Step::Art: {
            uint8_t* jpeg = nullptr;
            int len = 0;
            tick();
            bool ok = img::downloadAlbumArt(s_st.albumArtUrl, &jpeg, &len);
            if (ok) shared::postArt(gen, jpeg, len);
            tock(ok ? "art" : "art failed");
            break;
        }
        case netplan::Step::Lyrics: {
            tick();
            lyricsvc::Result r = lyricsvc::fetch(s_st);
            auto* lines = new std::vector<lrc::LrcLine>();
            if (r.kind == lyricsvc::Kind::Synced) *lines = lrc::parse(r.text);
            Serial.printf("[lyrics] kind=%d lines=%u\n", (int)r.kind, (unsigned)lines->size());
            shared::postLyrics(gen, lines);
            tock("lyrics");
            break;
        }
        case netplan::Step::Prefetch:   // implemented in Task 12
        case netplan::Step::None:
            break;
    }
    netplan::done(s_work, step);
}

static void run(void*) {
    uint32_t lastPoll = 0, lastPlayer = 0;
    bool first = true;
    for (;;) {
        net::loop();
        uint32_t now = millis();
        if (first || now - lastPoll >= 4000) {
            first = false;
            lastPoll = now;
            PlaybackStatus before = s_st.status;
            tick();
            spclient::poll(s_st);
            tock("poll");
            bool ok = s_st.status != PlaybackStatus::Offline;
            if (!s_link.update(ok) && !ok) s_st.status = before;   // isolated failure: keep last state
            if (s_gen.update(s_st.trackName)) onTrackChange();
            else shared::publish(s_st);
        } else if (now - lastPlayer >= 12000) {
            lastPlayer = now;
            tick();
            spclient::pollPlayerDetails(s_st);
            tock("player");
            shared::publish(s_st);
        } else {
            netplan::Step step = netplan::next(s_work);
            if (step != netplan::Step::None) doStep(step);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Stack: measured high-water mark ~5.3 KB of 16 KB (TLS + JSON); 10 KB leaves margin
// and returns 6 KB of heap, which album-art and TLS buffers were running short of.
void start() { xTaskCreatePinnedToCore(run, "net", 10240, nullptr, 1, nullptr, 0); }

}
