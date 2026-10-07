#include "status_sprite.h"

namespace status_sprite {

#include "status_sprite.inc"

int width() { return SPRITE_W; }
int height() { return SPRITE_H; }

bool pixel(int x, int y, uint16_t* rgb565) {
    if (x < 0 || y < 0 || x >= SPRITE_W || y >= SPRITE_H) return false;
    int i = y * SPRITE_W + x;
    if (!((SPRITE_MASK[i >> 3] >> (7 - (i & 7))) & 1)) return false;
    if (rgb565) *rgb565 = SPRITE_PX[i];
    return true;
}

}
