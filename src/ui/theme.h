#pragma once
#include <cstdint>

namespace theme {
// RGB565 GBA Ruby/Sapphire palette
constexpr uint16_t GBA_NAVY  = 0x218A;
constexpr uint16_t POKE_RED  = 0xE006;
// 8-bit-per-channel colour -> RGB565 (what TFT_eSPI fill/draw calls take).
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Gen-3 (FRLG/Emerald) battle palette. Approximations of the GBA games; tune on
// the real panel, which renders colours differently from a PC screen.
constexpr uint16_t SKY         = rgb(0xA8, 0xD8, 0xF8);
constexpr uint16_t GRASS       = rgb(0x88, 0xC8, 0x78);
constexpr uint16_t HORIZON     = rgb(0x60, 0xA8, 0x58);
constexpr uint16_t BOX_FILL    = rgb(0xF8, 0xF8, 0xD8);
constexpr uint16_t BOX_BORDER  = rgb(0x40, 0x48, 0x48);
constexpr uint16_t BOX_SHADOW  = rgb(0x58, 0x70, 0x60);
constexpr uint16_t TEXT        = rgb(0x40, 0x40, 0x40);
constexpr uint16_t TEXT_SHADOW = rgb(0xD8, 0xD0, 0xB0);
constexpr uint16_t HP_TAG      = rgb(0x48, 0x48, 0x48);
constexpr uint16_t HP_TAG_TEXT = rgb(0xF8, 0xB8, 0x00);
// Darker than first tuned (0x506058): red HP was 2.1:1 against it, hard to see in a car.
constexpr uint16_t HP_EMPTY    = rgb(0x28, 0x30, 0x28);
constexpr uint16_t EXP_BLUE    = rgb(0x40, 0xC8, 0xF8);
constexpr uint16_t DLG_FRAME   = rgb(0x28, 0x48, 0x60);
constexpr uint16_t DLG_LINE    = rgb(0x68, 0xA0, 0xB8);
constexpr uint16_t DLG_FILL    = rgb(0xF8, 0xF8, 0xF8);
constexpr uint16_t DLG_SHADOW  = rgb(0xD0, 0xD0, 0xD0);
constexpr uint16_t TOP_DARK    = rgb(0x28, 0x30, 0x38);
// Was 0x606870 (2.4:1 on the top strip): "off" icons vanished in daylight.
constexpr uint16_t ICON_OFF    = rgb(0x80, 0x88, 0x90);
constexpr uint16_t CD_SILVER   = rgb(0xC0, 0xC0, 0xC8);

// Pokémon HP-bar colours (fill + lighter 2 px shine line on top).
constexpr uint16_t HP_GREEN        = rgb(0x58, 0xD0, 0x80);
constexpr uint16_t HP_GREEN_SHINE  = rgb(0x90, 0xF8, 0xB8);
constexpr uint16_t HP_YELLOW       = rgb(0xF8, 0xC8, 0x00);
constexpr uint16_t HP_YELLOW_SHINE = rgb(0xF8, 0xE8, 0x70);
constexpr uint16_t HP_RED          = rgb(0xF8, 0x58, 0x38);
constexpr uint16_t HP_RED_SHINE    = rgb(0xF8, 0xA0, 0x88);

// HP-bar fill colour for a remaining fraction (Pokémon thresholds:
// >50% green, >20% yellow, else red).
constexpr uint16_t hpColor(float frac) {
    return frac > 0.5f ? HP_GREEN : (frac > 0.2f ? HP_YELLOW : HP_RED);
}
constexpr uint16_t hpShine(float frac) {
    return frac > 0.5f ? HP_GREEN_SHINE : (frac > 0.2f ? HP_YELLOW_SHINE : HP_RED_SHINE);
}

// Same hue at half brightness (type-badge border).
constexpr uint16_t darken(uint16_t c) {
    return (uint16_t)(((c >> 1) & 0x7800) | ((c >> 1) & 0x03E0) | ((c >> 1) & 0x000F));
}

// RGB565 with the bytes swapped: TFT_eSPI pushImage() buffers are big-endian.
constexpr uint16_t be(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

// The 18 Pokemon types (lowercase PokéAPI names), index 0..typeCount()-1; "" out of range.
int typeCount();
const char* typeName(int i);

// Accent color for a PokéAPI type name (lowercase canonical). Null/unknown -> GBA_NAVY.
uint16_t typeColor(const char* type);
}
