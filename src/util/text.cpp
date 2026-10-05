#include "text.h"

namespace txt {

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

void asciiFold(const char* src, char* dst, size_t n) {
    size_t o = 0;
    if (n == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    for (size_t i = 0; src[i] && o + 1 < n; ) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {                         // plain ASCII
            dst[o++] = (char)c; ++i;
        } else if (c == 0xC3 && src[i + 1]) {   // U+00C0..00FF accented letters
            unsigned int cp = 0xC0 + ((unsigned char)src[i + 1] - 0x80);
            for (const char* r = foldLatin1(cp); *r && o + 1 < n; ++r) dst[o++] = *r;
            i += 2;
        } else if (c == 0xC2 && src[i + 1]) {   // U+0080..00BF symbols -> drop
            i += 2;
        } else if ((c & 0xE0) == 0xC0) { i += 2; }   // skip other 2-byte
        else if ((c & 0xF0) == 0xE0) { i += 3; }     // skip 3-byte
        else if ((c & 0xF8) == 0xF0) { i += 4; }     // skip 4-byte
        else { ++i; }
    }
    dst[o] = '\0';
}

}
