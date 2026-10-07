#pragma once
#include <TFT_eSPI.h>
#include <stddef.h>
#include "../util/text.h"
// Text with Portuguese / Latin-1 accents. The fonts are ASCII-only, so each character is
// drawn as its ASCII base and the accent is added as pixel art (ui/accents) — same width,
// so fitting and wrapping are unchanged. Marks are drawn for font 2 only.
namespace ui {
// UTF-8 text, optionally upper-cased, truncated with "..." to maxW; 1 px shadow like
// shadowText. Datums: TL, TC, TR, ML, MC. Returns the drawn width.
int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint8_t font, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper = false);
// Already-folded text plus one mark per character (marks beyond nMarks count as None).
int drawFolded(TFT_eSPI& t, const char* text, const txt::Mark* marks, size_t nMarks, int x,
               int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum);
}
