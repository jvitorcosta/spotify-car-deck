#include "text.h"
#include <cstring>

namespace txt {

void copyId(const char* src, char* dst, size_t n) {
    if (n == 0) return;
    dst[0] = '\0';
    if (!src) return;
    size_t len = strlen(src);
    if (len < n) { memcpy(dst, src, len + 1); return; }
    if (n < 10) { memcpy(dst, src, n - 1); dst[n - 1] = '\0'; return; }
    uint32_t h = 2166136261u;                       // FNV-1a over the whole id
    for (size_t i = 0; i < len; ++i) { h ^= (uint8_t)src[i]; h *= 16777619u; }
    size_t keep = n - 10;                           // prefix + '#' + 8 hex + NUL
    memcpy(dst, src, keep);
    dst[keep] = '#';
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) dst[keep + 1 + i] = hex[(h >> (28 - 4 * i)) & 0xF];
    dst[n - 1] = '\0';
}

// ASCII base for a Latin-1 code point (U+00C0..U+00FF).
static const char* foldLatin1(unsigned int cp) {
    switch (cp) {
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return "A";
        case 0xC6: return "AE";
        case 0xC7: return "C";
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return "E";
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return "I";
        case 0xD0: return "D";
        case 0xD1: return "N";
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return "O";
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return "U";
        case 0xDD: return "Y";
        case 0xDF: return "ss";
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return "a";
        case 0xE6: return "ae";
        case 0xE7: return "c";
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "e";
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
        case 0xF0: return "d";
        case 0xF1: return "n";
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return "o";
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return "u";
        case 0xFD: case 0xFF: return "y";
        default: return "";
    }
}

// Accent of a Latin-1 letter (U+00C0..U+00FF).
static Mark markLatin1(unsigned int cp) {
    switch (cp) {
        case 0xC0: case 0xC8: case 0xCC: case 0xD2: case 0xD9:
        case 0xE0: case 0xE8: case 0xEC: case 0xF2: case 0xF9: return Mark::Grave;
        case 0xC1: case 0xC9: case 0xCD: case 0xD3: case 0xDA: case 0xDD:
        case 0xE1: case 0xE9: case 0xED: case 0xF3: case 0xFA: case 0xFD: return Mark::Acute;
        case 0xC2: case 0xCA: case 0xCE: case 0xD4: case 0xDB:
        case 0xE2: case 0xEA: case 0xEE: case 0xF4: case 0xFB: return Mark::Circumflex;
        case 0xC3: case 0xD1: case 0xD5: case 0xE3: case 0xF1: case 0xF5: return Mark::Tilde;
        case 0xC4: case 0xCB: case 0xCF: case 0xD6: case 0xDC:
        case 0xE4: case 0xEB: case 0xEF: case 0xF6: case 0xFC: case 0xFF: return Mark::Diaeresis;
        case 0xC5: case 0xE5: return Mark::Ring;
        case 0xC7: case 0xE7: return Mark::Cedilla;
        default: return Mark::None;
    }
}

size_t foldMarks(const char* src, char* dst, Mark* marks, size_t n) {
    size_t o = 0;
    if (n == 0) return 0;
    if (!src) { dst[0] = '\0'; return 0; }
    for (size_t i = 0; src[i] && o + 1 < n; ) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {                         // plain ASCII
            if (marks) marks[o] = Mark::None;
            dst[o++] = (char)c; ++i;
        } else if (c == 0xC3 && src[i + 1]) {   // U+00C0..00FF accented letters
            unsigned int cp = 0xC0 + ((unsigned char)src[i + 1] - 0x80);
            const char* r = foldLatin1(cp);
            Mark m = (r[0] && !r[1]) ? markLatin1(cp) : Mark::None;   // only 1:1 folds keep it
            for (; *r && o + 1 < n; ++r) {
                if (marks) marks[o] = m;
                dst[o++] = *r;
            }
            i += 2;
        } else if (c == 0xC2 && src[i + 1]) {   // U+0080..00BF symbols -> drop
            i += 2;
        } else if ((c & 0xE0) == 0xC0) { i += 2; }   // skip other 2-byte
        else if ((c & 0xF0) == 0xE0) { i += 3; }     // skip 3-byte
        else if ((c & 0xF8) == 0xF0) { i += 4; }     // skip 4-byte
        else { ++i; }
    }
    dst[o] = '\0';
    return o;
}

void asciiFold(const char* src, char* dst, size_t n) { foldMarks(src, dst, nullptr, n); }

}
