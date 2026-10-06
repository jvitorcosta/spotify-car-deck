#pragma once
#include <cstdint>
// Pixel-art icons for the top strip and dialogue box (the fonts are ASCII-only,
// so these stand in for emoji). PURE: no Arduino/TFT, host-tested.
namespace icons {
enum class Icon : uint8_t { Phone, Laptop, Speaker, Tv, Car, Shuffle, Repeat, Note };
constexpr int SIZE = 12;
// Spotify device "type" (Smartphone, Computer, Speaker, TV, Automobile, ...) -> icon.
// Unknown/empty/null -> Speaker.
Icon forDevice(const char* spotifyType);
// Row y of the 12x12 art as '#'(on)/'.'(off); nullptr if y is out of range.
const char* row(Icon i, int y);
bool pixel(Icon i, int x, int y);
}
