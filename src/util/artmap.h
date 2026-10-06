#pragma once
#include <cstddef>
// Album-art geometry and URL helpers. PURE, host-tested.
namespace artmap {
// Nearest-neighbour cover-fill: the centred square of side min(srcW,srcH) maps onto outW x outH.
struct Map { int x0, y0, side, outW, outH; };
Map cover(int srcW, int srcH, int outW, int outH);
inline int srcX(const Map& m, int ox) { return m.x0 + ox * m.side / m.outW; }
inline int srcY(const Map& m, int oy) { return m.y0 + oy * m.side / m.outH; }
// Largest tjpgd scale (0..3 = 1/1..1/8) whose shorter side is still >= minSide.
int pickScale(int w, int h, int minSide);
// "https://i.scdn.co/..." -> "http://i.scdn.co/..." (the CDN serves plain HTTP: no TLS needed);
// any other URL is copied unchanged. Always NUL-terminates.
void plainHttpUrl(const char* in, char* out, size_t len);
}
