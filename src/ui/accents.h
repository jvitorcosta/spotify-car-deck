#pragma once
#include "../util/text.h"
// Pixel art for accent marks drawn over the ASCII glyphs of the 16-px font 2. PURE, tested.
namespace accents {
constexpr int W = 5, H = 2;
// Row y of the mark as '#'/'.' (W chars); nullptr for Mark::None or y out of range.
const char* row(txt::Mark m, int y);
// First cell row of the mark: 0 over capitals (glyphs start at row 3), 3 over lowercase
// (row 6), 13 for the cedilla (under the row-12 baseline).
int topRow(txt::Mark m, char base);
}
