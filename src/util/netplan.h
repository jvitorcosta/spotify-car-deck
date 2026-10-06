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
