#pragma once
#include <cstdint>
// 13x13 pixel Poke Ball for the time label in the status box; spins like the top-bar CD.
// frame 0..FRAMES-1 turns the red half 45 deg clockwise per frame (frame 0: red on top).
// PURE, host-tested; drawing lives in screen_now.
namespace pokeball {
constexpr int SIZE = 13;
constexpr int FRAMES = 8;
enum class Px : uint8_t { Clear, Red, White, Dark };
Px pixel(int x, int y, int frame);
}
