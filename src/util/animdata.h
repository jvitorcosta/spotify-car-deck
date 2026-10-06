#pragma once
#include <cstdint>
// PMDCollab SpriteCollab AnimData.xml -> the "Walk" animation. PURE, host-tested.
namespace animdata {
constexpr int MAX_FRAMES = 16;
struct WalkAnim {
    bool ok;
    int frameW, frameH, frames;
    uint16_t ticks[MAX_FRAMES];   // per-frame duration in game ticks (1/60 s)
};
// Finds <Anim><Name>Walk</Name>..., following <CopyOf> aliases (max 4 hops).
// ok=false when Walk is absent, a CopyOf target is missing/cyclic, or size/durations
// are missing.
WalkAnim parseWalk(const char* xml);
}
