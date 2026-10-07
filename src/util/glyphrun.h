#pragma once
#include <cstddef>
#include <cstdint>
#include "text.h"
// UTF-8 text as a run of drawable items: ASCII glyphs of font 2 (with an optional accent mark)
// and Wide glyphs from the CJK font. PURE, host-tested; widths come from caller functions.
namespace glyphrun {
enum class Kind : uint8_t { Ascii, Wide };
struct Item { Kind kind; char ch; txt::Mark mark; uint32_t cp; uint8_t w; };
using AsciiWidthFn = int (*)(char c, void* ctx);
using WideWidthFn = int (*)(uint32_t cp, void* ctx);   // 0 = not covered by the font
// Decodes UTF-8: ASCII and Latin-1 letters -> Ascii (+mark, txt::foldMarks rules); codepoints
// the wide font covers -> Wide; everything else is dropped. upper: upper-case ASCII only.
size_t decode(const char* utf8, Item* out, size_t cap, AsciiWidthFn aw, WideWidthFn ww,
              void* ctx, bool upper = false);
int width(const Item* it, size_t n);
// If the run is wider than maxW: drop items from the end and append "..." (3 Ascii '.') so it
// fits. Returns the new count (<= cap).
size_t fit(Item* it, size_t n, size_t cap, int maxW, AsciiWidthFn aw, void* ctx);
// Up to two lines of maxW: line a = [0, aEnd), line b = [bStart, bEnd) (+ "..." if bEllipsis).
// Breaks at the last fitting space, or between characters next to a Wide item (CJK has no
// spaces), else hard. ellipsisW = width of "...".
struct Wrap { size_t aEnd, bStart, bEnd; bool bEllipsis; };
Wrap wrapTwo(const Item* it, size_t n, int maxW, int ellipsisW);
}
