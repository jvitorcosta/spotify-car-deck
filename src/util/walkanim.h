#pragma once
#include <cstdint>
// Arithmetic for turning a sprite sheet row into walker frames. PURE, host-tested.
namespace walkanim {
struct Box { int x0, y0, x1, y1; };                 // inclusive; empty when x1 < x0
Box emptyBox();
void include(Box& b, int x, int y);
bool isEmpty(const Box& b);

struct Fit { int w, h; };
// Target size for a bw x bh crop in a maxW x maxH slot: native if it fits, else
// nearest-neighbour downscale keeping aspect. Never upscales; sides >= 1.
Fit fitBand(int bw, int bh, int maxH, int maxW);
// Source coordinate for output coordinate o when mapping srcLen -> outLen.
int srcIndex(int o, int outLen, int srcStart, int srcLen);

// Smallest k >= 1 so that keeping every k-th of `frames` (w x h, 3 B/px) fits
// capBytes; 0 if even one frame does not fit.
int keepEvery(int frames, int w, int h, int capBytes);
int keptCount(int frames, int k);
// Kept frame i lasts the sum of source ticks [i*k, min(i*k+k, n)), in ms (60 ticks/s).
void mergedDurationsMs(const uint16_t* ticks, int n, int k, uint16_t* outMs);
// Frame index at elapsedMs in a looping animation with per-frame ms durations.
int frameAt(uint32_t elapsedMs, const uint16_t* durMs, int n);
// True if PNGdec 1.1.6 can decode a `width`-pixel-wide PNG of this colour type (0 gray, 2 RGB,
// 3 indexed, 4 gray+alpha, 6 RGBA) and bit depth: it keeps two lines (+16 B each) in a
// maxBuffered-byte buffer and does not check RGBA widths above ~316 px itself.
bool pngFits(int width, int pixelType, int bpp, int maxBuffered = 2562);
}
