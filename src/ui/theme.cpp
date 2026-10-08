#include "theme.h"
#include "../util/text.h"

namespace theme {
struct TypeColor { const char* name; uint16_t color; };
// Classic type colours (Gen 3-era summary badges). The first version of this table was
// hand-encoded RGB565 and mostly wrong (grass rust red, water grey, ground pink).
static const TypeColor TABLE[] = {
    {"normal",   rgb(0xA8, 0xA8, 0x78)}, {"fire",    rgb(0xF0, 0x80, 0x30)},
    {"water",    rgb(0x68, 0x90, 0xF0)}, {"electric", rgb(0xF8, 0xD0, 0x30)},
    {"grass",    rgb(0x78, 0xC8, 0x50)}, {"ice",     rgb(0x98, 0xD8, 0xD8)},
    {"fighting", rgb(0xC0, 0x30, 0x28)}, {"poison",  rgb(0xA0, 0x40, 0xA0)},
    {"ground",   rgb(0xE0, 0xC0, 0x68)}, {"flying",  rgb(0xA8, 0x90, 0xF0)},
    {"psychic",  rgb(0xF8, 0x58, 0x88)}, {"bug",     rgb(0xA8, 0xB8, 0x20)},
    {"rock",     rgb(0xB8, 0xA0, 0x38)}, {"ghost",   rgb(0x70, 0x58, 0x98)},
    {"dragon",   rgb(0x70, 0x38, 0xF8)}, {"dark",    rgb(0x70, 0x58, 0x48)},
    {"steel",    rgb(0xB8, 0xB8, 0xD0)}, {"fairy",   rgb(0xEE, 0x99, 0xAC)},
};
static constexpr int TYPE_N = (int)(sizeof(TABLE) / sizeof(TABLE[0]));

int typeCount() { return TYPE_N; }
const char* typeName(int i) { return (i >= 0 && i < TYPE_N) ? TABLE[i].name : ""; }

uint16_t typeColor(const char* type) {
    for (const auto& e : TABLE)
        if (txt::equalsIgnoreCase(type, e.name)) return e.color;
    return GBA_NAVY;
}
}
