#include "pokeball.h"
#include <math.h>

namespace pokeball {

Px pixel(int x, int y, int frame) {
    const int c = SIZE / 2;
    int dx = x - c, dy = y - c;
    int d2 = dx * dx + dy * dy;
    if (d2 > 42) return Px::Clear;        // outside r ~6.5
    if (d2 > 30) return Px::Dark;         // outline ring (r 5.5..6.5)
    if (d2 <= 2) return Px::White;        // centre button
    if (d2 <= 6) return Px::Dark;         // ring around the button
    // Unit vector pointing into the red half: up at frame 0, turning 45 deg clockwise per frame.
    int f = ((frame % FRAMES) + FRAMES) % FRAMES;
    float a = f * (float)M_PI / 4.0f;
    float s = dx * sinf(a) - dy * cosf(a);
    if (fabsf(s) < 0.6f) return Px::Dark; // band through the middle, 1 px at every angle
    return s > 0 ? Px::Red : Px::White;
}

}
