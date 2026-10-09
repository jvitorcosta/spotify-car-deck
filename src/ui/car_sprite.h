#pragma once
#include <cstdint>
// The boot scene's car (tools/gen_car_sprite.py): silver Honda City sedan, side view facing
// right, 200 x 66 px with the ground line at the bottom. Wheels are not in the sprite (drawn in
// code so they spin). PURE, host-tested.
namespace car_sprite {
enum Kind : uint8_t { NONE = 0, UPPER = 1, LOWER = 2, OTHER = 3, HEAD = 4, TAIL = 5 };
// UPPER/LOWER: paint above/below the shoulder line (gets the streetlight reflection);
// HEAD/TAIL: lamp pixels (recoloured from the sound); OTHER: glass, trim, liner...
constexpr int REAR_WHEEL_X = 46, FRONT_WHEEL_X = 160, WHEEL_Y = 52;   // wheel centres
int width();
int height();
// False when (x, y) is transparent or outside; then *kind (if given) is NONE. Otherwise
// *rgb565 (if given) is its native RGB565 colour and *kind its Kind.
bool pixel(int x, int y, uint16_t* rgb565, uint8_t* kind);
}
