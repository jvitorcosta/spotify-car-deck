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

enum class Step { None, Walk, Art, Lyrics, Prefetch };
struct Work { bool walk, art, lyrics, prefetch; };   // true = still to do

// Work list for a new track. walkerReady = a prefetched walker was promoted.
Work freshWork(bool walkerReady);
// Next step in priority order: Walk, Art, Lyrics, Prefetch; None when all done.
Step next(const Work& w);
void done(Work& w, Step s);
}
