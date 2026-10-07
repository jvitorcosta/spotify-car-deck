#include "cjkfont.h"
#include <cstring>

namespace cjkfont {

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool rangeFits(uint32_t offset, uint32_t count, uint32_t perGlyph, uint32_t len) {
    if (perGlyph == 0 || offset > len) return false;
    return count <= (len - offset) / perGlyph;   // divide instead of multiply: cannot wrap
}

Font::Range Font::range(int i) const {
    const uint8_t* p = blob_ + 6 + 13 * i;
    return {rd32(p), rd32(p + 4), p[8], rd32(p + 9)};
}

bool Font::open(const uint8_t* blob, size_t len) {
    blob_ = nullptr; len_ = 0; n_ = 0;
    if (!blob || len < 6 || memcmp(blob, "UFNT", 4) != 0) return false;
    int n = blob[4] | (blob[5] << 8);
    if (len < (size_t)(6 + 13 * n)) return false;
    blob_ = blob; len_ = len; n_ = n;
    uint32_t prevLast = 0;
    for (int i = 0; i < n; ++i) {
        Range r = range(i);
        uint32_t per = r.width == 8 ? 16 : 32;
        bool ok = (r.width == 8 || r.width == 16) && r.first <= r.last &&
                  (i == 0 || r.first > prevLast) &&
                  r.last - r.first < 0xFFFFFFFFu &&                // count below fits in 32 bits
                  rangeFits(r.offset, r.last - r.first + 1, per, (uint32_t)len);
        if (!ok) { blob_ = nullptr; len_ = 0; n_ = 0; return false; }
        prevLast = r.last;
    }
    return true;
}

const uint8_t* Font::glyph(uint32_t cp, int* width) const {
    int lo = 0, hi = n_ - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        Range r = range(mid);
        if (cp < r.first) hi = mid - 1;
        else if (cp > r.last) lo = mid + 1;
        else {
            if (width) *width = r.width;
            return blob_ + r.offset + (cp - r.first) * (r.width == 8 ? 16 : 32);
        }
    }
    return nullptr;
}

}
