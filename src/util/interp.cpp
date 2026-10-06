#include "interp.h"
namespace interp {
uint32_t currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs,
                           bool isPlaying, uint32_t msSincePoll) {
    if (!isPlaying) return lastProgressMs;
    uint32_t p = lastProgressMs + msSincePoll;
    return (p > durationMs) ? durationMs : p;
}
}
namespace interp {
int walkX(float frac, int barX, int barW, int spriteW) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int half = spriteW / 2;
    int lo = barX + half;
    int hi = barX + barW - half;
    int x = barX + (int)(frac * barW);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}
}
