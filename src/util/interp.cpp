#include "interp.h"
namespace interp {
uint32_t currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs,
                           bool isPlaying, uint32_t msSincePoll) {
    if (!isPlaying) return lastProgressMs;
    uint32_t p = lastProgressMs + msSincePoll;
    return (p > durationMs) ? durationMs : p;
}
}
