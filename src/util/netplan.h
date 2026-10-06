#pragma once
#include <cstdint>
// What the network task does next. PURE, host-tested.
// The task polls Spotify on a fixed cadence and, between polls, does ONE per-track
// work step at a time so a slow download never delays the next poll by more than
// one step (see README "Design & performance history").
namespace netplan {

// Track-change detection from successive now-playing names.
class TrackGen {
public:
    // True (and the generation increments) when `track` is non-empty and differs
    // from the last non-empty name seen.
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
// (README "Design & performance history"), so: 2 failed polls in a row with WiFi up pause the
// optional downloads; 180 s without a good poll while WiFi is up -> restart. WiFi down is the
// reconnect logic's job and restarts the 180 s clock.
class Health {
public:
    enum class Action { None, PauseOptional, Restart };
    static constexpr int FAILS_TO_PAUSE = 2;
    static constexpr uint32_t RESTART_AFTER_MS = 180000;
    Action onPoll(bool ok, bool wifiUp, uint32_t nowMs) {
        if (!started_) { started_ = true; lastOk_ = nowMs; }
        if (ok) { fails_ = 0; lastOk_ = nowMs; paused_ = false; return Action::None; }
        ++fails_;
        if (!wifiUp) { lastOk_ = nowMs; return Action::None; }
        if (nowMs - lastOk_ >= RESTART_AFTER_MS) return Action::Restart;
        if (fails_ >= FAILS_TO_PAUSE) { paused_ = true; return Action::PauseOptional; }
        return Action::None;
    }
    bool optionalPaused() const { return paused_; }
private:
    int fails_ = 0;
    uint32_t lastOk_ = 0;
    bool started_ = false;
    bool paused_ = false;
};

enum class Step { None, Walk, Art, Lyrics, Prefetch };
struct Work { bool walk, art, lyrics, prefetch; };   // true = still to do

// Work list for a new track. walkerReady = a prefetched walker was promoted.
Work freshWork(bool walkerReady);
// Next step in priority order: Art, Lyrics, Walk, Prefetch; None when all done.
// Art and lyrics are what the listener waits for; the walker is usually prefetched.
Step next(const Work& w);
void done(Work& w, Step s);

// Memory gate (largest free byte-addressable block, mem::byteLargest()). mbedTLS needs a
// 16.7 KB input buffer plus ~3 KB per handshake, so TLS steps wait below TLS_NEED. Walker
// downloads use the fixed scratch buffer, so they need no more than that. Art is plain HTTP.
constexpr unsigned TLS_NEED = 20000;
bool canRun(Step s, unsigned largest);
}
