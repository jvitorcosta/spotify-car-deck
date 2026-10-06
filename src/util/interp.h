#pragma once
#include <cstdint>
namespace interp {
uint32_t currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs,
                           bool isPlaying, uint32_t msSincePoll);
// Center x for a sprite of width spriteW walking along a bar at fraction frac,
// clamped so the whole sprite stays within [barX, barX+barW].
int walkX(float frac, int barX, int barW, int spriteW);
}
