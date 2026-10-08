#pragma once
#include <cstdint>
#include "lyricstatus.h"
// What the network task does next. PURE, host-tested.
// The task polls Spotify on a fixed cadence and, between polls, does ONE per-track
// work step at a time so a slow download never delays the next poll by more than
// one step (see README "Design & performance history").
namespace netplan {

// Track-change detection from successive now-playing ids (the track URI; the name only when
// there is no URI), so two songs with the same title still count as a change.
class TrackGen {
public:
    // True (and the generation increments) when `track` is non-empty and differs
    // from the last non-empty id seen.
    bool update(const char* track);
    uint32_t gen() const { return gen_; }
private:
    char last_[96] = {0};
    uint32_t gen_ = 0;
};

// "No signal" debounce. A single failed poll (TLS hiccup, HTTP -1) used to flip the
// whole deck to the No-signal screen; only a run of failures means the link is down.
class LinkGate {
public:
    static constexpr int FAILS_FOR_DOWN = 3;
    // Feed each poll result; returns true while the link should be shown as down.
    bool update(bool ok) {
        fails_ = ok ? 0 : fails_ + 1;
        return fails_ >= FAILS_FOR_DOWN;
    }
private:
    int fails_ = 0;
};

// Self-healing ladder. A heap so fragmented that TLS can't allocate never recovered by itself
// (README "Design & performance history"), so:
//  - 2 failed polls in a row with WiFi up -> pause the optional downloads;
//  - restart only on memory evidence (memStarved: failed allocations since the last good poll,
//    or the largest block below TLS_NEED) after 180 s without a good poll — a dead zone or a
//    401/429 can't be fixed by rebooting, and must not become a reboot loop;
//  - WiFi down for 15 min straight -> restart (backstop if the WiFi driver can't come back).
class Health {
public:
    enum class Action { None, PauseOptional, Restart };
    static constexpr int FAILS_TO_PAUSE = 2;
    static constexpr uint32_t RESTART_AFTER_MS = 180000;
    static constexpr uint32_t WIFI_DOWN_RESTART_MS = 900000;
    Action onPoll(bool ok, bool wifiUp, bool memStarved, uint32_t nowMs) {
        if (!started_) { started_ = true; lastOk_ = nowMs; }
        if (ok) {
            fails_ = 0; lastOk_ = nowMs; paused_ = false; wifiDown_ = false;
            return Action::None;
        }
        if (!wifiUp) {
            lastOk_ = nowMs;   // the memory clock only runs while WiFi is up
            if (!wifiDown_) { wifiDown_ = true; downSince_ = nowMs; }
            return nowMs - downSince_ >= WIFI_DOWN_RESTART_MS ? Action::Restart : Action::None;
        }
        if (wifiDown_) { wifiDown_ = false; lastOk_ = nowMs; }   // WiFi back: memory clock restarts
        ++fails_;
        if (memStarved && nowMs - lastOk_ >= RESTART_AFTER_MS) return Action::Restart;
        if (fails_ >= FAILS_TO_PAUSE) { paused_ = true; return Action::PauseOptional; }
        return Action::None;
    }
    bool optionalPaused() const { return paused_; }
private:
    int fails_ = 0;
    uint32_t lastOk_ = 0;
    uint32_t downSince_ = 0;
    bool started_ = false;
    bool paused_ = false;
    bool wifiDown_ = false;
};

// True when a successful poll happened (lastOkMs != 0) more than limitMs ago (wrap-safe).
inline bool stale(uint32_t nowMs, uint32_t lastOkMs, uint32_t limitMs) {
    return lastOkMs != 0 && nowMs - lastOkMs > limitMs;
}

enum class Step { None, Walk, Art, Lyrics, Prefetch, Genre };
struct Work { bool walk, art, lyrics, prefetch, genre; };   // true = still to do

// Work list for a new track. walkerReady = a prefetched walker was promoted.
Work freshWork(bool walkerReady);
// Next step in priority order: Art, Lyrics, Genre, Walk, Prefetch; None when all done.
// Art and lyrics are what the listener waits for; the walker is usually prefetched.
// lyricsReady false (waiting to retry): Lyrics is skipped for now.
Step next(const Work& w, bool lyricsReady = true);
void done(Work& w, Step s);

// Memory gate (largest free byte-addressable block, mem::byteLargest()). mbedTLS needs a
// 16.7 KB input buffer plus ~3 KB per handshake, so TLS steps wait below TLS_NEED. Walker
// downloads use the fixed scratch buffer, so they need no more than that. Art is plain HTTP.
constexpr unsigned TLS_NEED = 20000;
bool canRun(Step s, unsigned largest);

// Lyrics retries: a temporary error (HTTP 5xx/429, connect/TLS failure, timeout, stalled read)
// retries after RETRY1_MS, then RETRY2_MS; the third gives up. Other results are final.
// LRCLIB returned 503 on several requests while lyrics went missing (spec 2026-10-07).
class LyricsRetry {
public:
    static constexpr uint32_t RETRY1_MS = 5000;
    static constexpr uint32_t RETRY2_MS = 20000;
    void reset() { attempts_ = 0; waiting_ = false; }   // new track
    // Records one attempt; true when no further attempt will be made for this track.
    bool onResult(lyricstatus::Result r, uint32_t nowMs);
    bool ready(uint32_t nowMs) const { return !waiting_ || nowMs - since_ >= wait_; }   // wrap-safe
    int attempts() const { return attempts_; }
private:
    int attempts_ = 0;
    bool waiting_ = false;
    uint32_t since_ = 0, wait_ = 0;
};
}
