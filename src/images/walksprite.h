#pragma once
#include <stdint.h>

// The walker's frames for the current song. Loaded once per track change; the
// per-frame blit needs no network or decode. Two sources:
//  - PMD SpriteCollab walk cycle (Right-facing row), several frames;
//  - fallback: the PokeAPI sprite cropped to one frame (UI adds bob + mirror).
namespace walk {
constexpr int BAND_H     = 32;      // walk band height in the status box
constexpr int MAX_W      = 64;      // widest frame kept
constexpr int CAP_BYTES  = 16384;   // RGB565 + 1-byte mask per pixel, all frames
constexpr int MAX_FRAMES = 16;

struct Info { bool ready; bool pmd; int w, h, frames; };

bool loadPmd(int dex);
bool loadFallback(const char* spriteUrl, int dex);

const Info& info();
const uint16_t* pixels(int frame);   // big-endian RGB565, w*h
const uint8_t*  mask(int frame);     // 1 = opaque, w*h
uint16_t durationMs(int frame);      // 0 for the fallback frame
}
