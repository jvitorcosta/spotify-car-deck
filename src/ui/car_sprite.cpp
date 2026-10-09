#include "car_sprite.h"

namespace car_sprite {

#include "car_sprite.inc"

int width() { return CAR_W; }
int height() { return CAR_H; }

bool pixel(int x, int y, uint16_t* rgb565, uint8_t* kind) {
    uint8_t k = NONE;
    if (x >= 0 && y >= 0 && x < CAR_W && y < CAR_H) {
        const int i = y * CAR_W + x;
        k = (i & 1) ? (CAR_KIND[i >> 1] & 0x0F) : (CAR_KIND[i >> 1] >> 4);
        if (k != NONE && rgb565) *rgb565 = CAR_PX[i];
    }
    if (kind) *kind = k;
    return k != NONE;
}

}
