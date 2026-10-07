#pragma once
#include <cstddef>
// Gen-3 summary-screen type badge: rounded box in the type colour, darker border, white
// uppercase label in font 1. PURE label logic here (host-tested); drawing in screen_now.
namespace typebadge {
constexpr int W = 52, H = 12;   // fits "FIGHTING"/"ELECTRIC" (8 x 6 px - 1) with 2 px padding
constexpr int CHAR_W = 6;       // TFT_eSPI font 1 advance
// Uppercase label for one of the 18 types (case-insensitive); false (out = "") otherwise.
bool label(const char* type, char* out, size_t n);
}
