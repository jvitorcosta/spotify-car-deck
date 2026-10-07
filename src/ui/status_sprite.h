#pragma once
#include <cstdint>
// The Pokemon shown on the status screens, bundled in flash (tools/gen_status_sprite.py):
// those screens appear exactly when nothing can be downloaded. PURE, host-tested.
namespace status_sprite {
int width();
int height();
// True when (x, y) is opaque; then *rgb565 (if given) is its native RGB565 colour.
bool pixel(int x, int y, uint16_t* rgb565);
}
