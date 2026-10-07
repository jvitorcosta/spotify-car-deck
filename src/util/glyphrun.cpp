#include "glyphrun.h"
#include <cctype>

namespace glyphrun {

size_t decode(const char* s, Item* out, size_t cap, AsciiWidthFn aw, WideWidthFn ww, void* ctx,
              bool upper) {
    size_t n = 0;
    if (!s) return 0;
    for (size_t i = 0; s[i] && n < cap;) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {                                   // ASCII
            char ch = upper ? (char)toupper(c) : (char)c;
            out[n++] = {Kind::Ascii, ch, txt::Mark::None, c, (uint8_t)aw(ch, ctx)};
            ++i;
            continue;
        }
        int len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        bool complete = true;
        for (int k = 1; k < len; ++k)
            if (((unsigned char)s[i + k] & 0xC0) != 0x80) { complete = false; break; }
        if (!complete || len == 1) { ++i; continue; }     // stray/invalid byte
        if (len == 2) {                                   // Latin-1 letters -> base + mark
            char seq[3] = {s[i], s[i + 1], 0}, base[4];
            txt::Mark marks[4];
            size_t m = txt::foldMarks(seq, base, marks, sizeof(base));
            for (size_t k = 0; k < m && n < cap; ++k) {
                char ch = upper ? (char)toupper((unsigned char)base[k]) : base[k];
                out[n++] = {Kind::Ascii, ch, marks[k], (uint32_t)(unsigned char)base[k],
                            (uint8_t)aw(ch, ctx)};
            }
        } else if (len == 3) {
            uint32_t cp = ((c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) |
                          ((unsigned char)s[i + 2] & 0x3F);
            int w = ww(cp, ctx);
            if (w > 0) out[n++] = {Kind::Wide, 0, txt::Mark::None, cp, (uint8_t)w};
        }                                                  // 4-byte (emoji...) dropped
        i += len;
    }
    return n;
}

int width(const Item* it, size_t n) {
    int w = 0;
    for (size_t i = 0; i < n; ++i) w += it[i].w;
    return w;
}

size_t fit(Item* it, size_t n, size_t cap, int maxW, AsciiWidthFn aw, void* ctx) {
    if (width(it, n) <= maxW) return n;
    int dot = aw('.', ctx);
    while (n > 0 && width(it, n) + 3 * dot > maxW) --n;
    for (int k = 0; k < 3 && n < cap; ++k)
        it[n++] = {Kind::Ascii, '.', txt::Mark::None, '.', (uint8_t)dot};
    return n;
}

// Number of items from `start` that fit in maxW.
static size_t fitCount(const Item* it, size_t start, size_t n, int maxW) {
    int w = 0;
    size_t k = start;
    while (k < n && w + it[k].w <= maxW) w += it[k++].w;
    return k - start;
}

static bool isSpace(const Item& x) { return x.kind == Kind::Ascii && x.ch == ' '; }

Wrap wrapTwo(const Item* it, size_t n, int maxW, int ellipsisW) {
    size_t fitA = fitCount(it, 0, n, maxW);
    if (fitA >= n) return {n, n, n, false};
    size_t cut = fitA, next = fitA;                       // hard split by default
    for (size_t k = fitA; k > 0; --k) {
        if (isSpace(it[k])) { cut = k; next = k + 1; break; }          // break at a space
        if (it[k].kind == Kind::Wide || it[k - 1].kind == Kind::Wide) { cut = next = k; break; }
    }
    if (cut == 0) cut = next = fitA;
    Wrap w{cut, next, n, false};
    if (width(it + next, n - next) > maxW) {
        w.bEllipsis = true;
        w.bEnd = next + fitCount(it, next, n, maxW - ellipsisW);
    }
    return w;
}

}
