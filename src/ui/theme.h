#pragma once
#include <cstdint>

namespace theme {
// RGB565 GBA Ruby/Sapphire palette
constexpr uint16_t GBA_CREAM = 0xF73A;
constexpr uint16_t GBA_NAVY  = 0x218A;
constexpr uint16_t GBA_GOLD  = 0xD605;
constexpr uint16_t HP_GREEN  = 0x4605;
constexpr uint16_t POKE_RED  = 0xE006;

// Accent color for a PokéAPI type name (lowercase canonical). Null/unknown -> GBA_NAVY.
uint16_t typeColor(const char* type);
}
