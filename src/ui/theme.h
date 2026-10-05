#pragma once
#include <cstdint>

namespace theme {
// RGB565 GBA Ruby/Sapphire palette
constexpr uint16_t GBA_CREAM = 0xF73A;
constexpr uint16_t GBA_NAVY  = 0x218A;
constexpr uint16_t GBA_GOLD  = 0xD605;
constexpr uint16_t POKE_RED  = 0xE006;
// Pokémon HP-bar colors: green when high, yellow mid, red low.
constexpr uint16_t HP_GREEN  = 0x5E8B;  // ~#58D058
constexpr uint16_t HP_YELLOW = 0xFE87;  // ~#F8D038
constexpr uint16_t HP_RED    = 0xFA87;  // ~#F85038

// Returns the HP-bar fill color for a remaining fraction (Pokémon thresholds:
// >50% green, >20% yellow, else red).
constexpr uint16_t hpColor(float frac) {
    return frac > 0.5f ? HP_GREEN : (frac > 0.2f ? HP_YELLOW : HP_RED);
}

// Accent color for a PokéAPI type name (lowercase canonical). Null/unknown -> GBA_NAVY.
uint16_t typeColor(const char* type);
}
