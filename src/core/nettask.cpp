#include "nettask.h"
#include <Arduino.h>
#include <esp_system.h>
#include "shared.h"
#include "mem.h"
#include "../net/wifi.h"
#include "../spotify/client.h"
#include "../images/art.h"
#include "../images/walksprite.h"
#include "../lyrics/lrclib.h"
#include "../pokemon/pick.h"
#include "../pokemon/dex.h"
#include "../util/netplan.h"
#include "../util/genre.h"
#include "../ui/genrebadge.h"
#include "../genre/apple.h"

// History (README "Design & performance history"): these calls used to run inline in
// loop(). Each is a fresh TLS handshake (poll ~1.5 s, player ~1.4 s, PokeAPI 4.2 s, art
// 2.5 s, lyrics 3.8 s, walk sheet 3.5 s), so the display froze during every poll and a
// track change took ~19 s to appear. Here they block only this task.
namespace nettask {

static AppState s_st{};            // this task's private copy; published after changes
static netplan::TrackGen s_gen;
static netplan::Work s_work{};
static netplan::LinkGate s_link;
static netplan::Health s_health;
static netplan::LyricsRetry s_lyricsRetry;   // 5 s / 20 s retries after LRCLIB hiccups
static genre::Cache s_genres;   // last 32 artists' badges: repeats cost no request
static uint32_t s_failsAtOk = 0;   // mem::failTotal() at the last good poll
static bool s_spotifyReady = false;   // WiFi + spclient::begin() done   // pause optional work / restart when polls keep failing   // "No signal" only after several failed polls in a row
static int s_prefetchDex = 0;      // dex whose walker sits in the staged slot (Task 12)
static uint32_t s_t0 = 0;

static void tick() { s_t0 = millis(); }
static void tock(const char* what) {
    Serial.printf("[net] %s %lums\n", what, (unsigned long)(millis() - s_t0));
}

static void onTrackChange() {
    s_st.trackGen = s_gen.gen();
    bool prefetched = s_prefetchDex > 0 && walk::stagedDex() == s_prefetchDex;
    pick::choose(s_st, prefetched ? s_prefetchDex : dex::fromRandom(esp_random()));
    s_prefetchDex = 0;
    shared::publish(s_st);                 // UI shows the new title + Pokemon right away
    if (prefetched) {
        { shared::Guard g; walk::promote(); }
        shared::postWalker(s_st.trackGen); // walker appears together with the title
    }
    s_work = netplan::freshWork(prefetched);
    s_lyricsRetry.reset();               // UI shows "searching" until the first result
    uint8_t badge;
    if (s_genres.find(s_st.artist, &badge)) {   // known artist (or none): no request
        netplan::done(s_work, netplan::Step::Genre);
        if (badge != genre::NONE) shared::postGenre(s_st.trackGen, badge);
    }
    Serial.printf("[net] track gen=%u%s\n", (unsigned)s_st.trackGen,
                  prefetched ? " (prefetched walker)" : "");
}

// Runs one step; false when the step must run again later (lyrics retry).
static bool doStep(netplan::Step step) {
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
            tick();
            shared::artInvalidate();          // UI stops pushing the bitmap before we overwrite it
            bool ok = art::fetch(s_st.albumArtUrl);
            if (ok) shared::postArt(gen);
            tock(ok ? "art" : "art failed");
            mem::log("art");
            break;
        }
        case netplan::Step::Lyrics: {
            tick();
            shared::lyricsInvalidate();       // UI stops reading the arena before we overwrite it
            lyricstatus::Result r =
                lyricsvc::fetchInto(s_st, lyricsvc::arena(), s_lyricsRetry.attempts() + 1);
            bool final = s_lyricsRetry.onResult(r, millis());
            if (r == lyricstatus::Result::Synced || r == lyricstatus::Result::Plain)
                shared::postLyrics(gen);      // lines first, then the status that points at them
            shared::postLyricsStatus(gen, lyricstatus::statusFor(r, final));
            tock(final ? "lyrics" : "lyrics (retry later)");
            mem::log("lyrics");
            if (!final) return false;
            break;
        }
        case netplan::Step::Prefetch: {
            // The Pokemon is random and independent of the song, so the next one can be
            // chosen and its walker loaded now, while this song plays.
            int n = dex::fromRandom(esp_random());
            char url[160];
            dex::spriteUrl(n, url, sizeof(url));
            tick();
            bool ok = walk::loadPmd(n) || walk::loadFallback(url, n);
            s_prefetchDex = ok ? n : 0;
            tock(ok ? "prefetch" : "prefetch failed");
            break;
        }
        case netplan::Step::Genre: {
            tick();
            uint32_t id = 0;
            int rc = 0;
            applegenre::Result r = applegenre::lookup(s_st.artist, &id, &rc);
            if (r == applegenre::Result::Found) {
                uint8_t badge = genrebadge::forGenreId(id);
                s_genres.store(s_st.artist, badge);
                shared::postGenre(gen, badge);
                Serial.printf("[genre] \"%s\": %u -> %s\n", s_st.artist, (unsigned)id,
                              genrebadge::at(badge).label);
            } else if (r == applegenre::Result::None) {
                s_genres.store(s_st.artist, genre::NONE);   // Apple doesn't know: don't ask again
                Serial.printf("[genre] \"%s\": none\n", s_st.artist);
            } else {
                Serial.printf("[genre] \"%s\": error rc=%d\n", s_st.artist, rc);   // not cached
            }
            tock("genre");
            break;
        }
        case netplan::Step::None:
            break;
    }
    netplan::done(s_work, step);
    return true;
}

static void run(void*) {
    uint32_t lastPoll = 0, lastPlayer = 0;
    bool first = true;
    for (;;) {
        if (!s_spotifyReady) {                 // WiFi wasn't up at boot: keep trying
            if (net::isOnline() || net::connectAny()) {
                spclient::begin();
                s_spotifyReady = true;
                mem::log("late wifi");
            } else {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }
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
            if (ok) { s_st.lastPollOkMs = millis(); s_failsAtOk = mem::failTotal(); }
            // Restart only helps a starved heap: allocations failed since the last good poll,
            // or no block big enough for TLS. A dead zone or a 401/429 is not that.
            bool starved = mem::failTotal() != s_failsAtOk || mem::byteLargest() < netplan::TLS_NEED;
            if (!ok) mem::log("poll failed");
            if (s_health.onPoll(ok, net::isOnline(), starved, now) == netplan::Health::Action::Restart) {
                mem::log("restart");
                Serial.printf("[net] self-heal restart (%s)\n",
                              net::isOnline() ? "heap starved, no good poll for 180 s" : "WiFi down for 15 min");
                delay(200);
                ESP.restart();
            }
            if (!s_link.update(ok) && !ok) s_st.status = before;   // isolated failure: keep last state
            const char* id = s_st.trackUri[0] ? s_st.trackUri : s_st.trackName;   // URI: same-title songs differ
            if (s_gen.update(id)) onTrackChange();
            else shared::publish(s_st);
        } else if (now - lastPlayer >= 12000) {
            lastPlayer = now;
            tick();
            spclient::pollPlayerDetails(s_st);
            tock("player");
            shared::publish(s_st);
        } else {
            netplan::Step step = netplan::next(s_work, s_lyricsRetry.ready(now));
            static bool deferred = false;
            if (step != netplan::Step::None) {
                bool optional = step != netplan::Step::Art;
                if (!(optional && s_health.optionalPaused()) &&
                    netplan::canRun(step, (unsigned)mem::byteLargest())) {
                    deferred = false;
                    doStep(step);
                } else if (!deferred) {            // log once per deferral, retry after polls
                    deferred = true;
                    mem::log("defer step");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Stack: measured high-water mark ~5.3 KB of 16 KB (TLS + JSON); 10 KB leaves margin
// and returns 6 KB of heap, which album-art and TLS buffers were running short of.
void start(bool spotifyReady) {
    s_spotifyReady = spotifyReady;
    xTaskCreatePinnedToCore(run, "net", 10240, nullptr, 1, nullptr, 0);
}

}
