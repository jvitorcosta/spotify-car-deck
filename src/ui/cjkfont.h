#pragma once
#include <cstddef>
#include <cstdint>
// Reader for the embedded Unifont CJK blob (tools/gen_cjk_font.py). PURE, host-tested.
// The blob stays in flash; lookups are a binary search over its few ranges + arithmetic.
namespace cjkfont {
// True when `count` glyphs of `perGlyph` bytes starting at `offset` lie inside a `len`-byte
// blob. 32-bit arguments on purpose: that is size_t on the ESP32, where the old
// offset + count * perGlyph check wrapped around and accepted a huge range.
bool rangeFits(uint32_t offset, uint32_t count, uint32_t perGlyph, uint32_t len);
class Font {
public:
    // Validates the "UFNT" header, range order and bounds. False leaves the font empty.
    bool open(const uint8_t* blob, size_t len);
    // 16-row bitmap for cp (width 8: 1 B/row, width 16: 2 B/row, MSB = leftmost) or nullptr.
    const uint8_t* glyph(uint32_t cp, int* width) const;
    int rangeCount() const { return n_; }
private:
    struct Range { uint32_t first, last; uint8_t width; uint32_t offset; };
    Range range(int i) const;
    const uint8_t* blob_ = nullptr;
    size_t len_ = 0;
    int n_ = 0;
};
}
