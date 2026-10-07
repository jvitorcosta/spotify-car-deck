#include "accents.h"
#include <cctype>

namespace accents {

const char* row(txt::Mark m, int y) {
    if (y < 0 || y >= H) return nullptr;
    static const char* const ART[][H] = {
        {"...#.", "..#.."},   // Acute
        {".#...", "..#.."},   // Grave
        {"..#..", ".#.#."},   // Circumflex
        {".##.#", "#..#."},   // Tilde
        {".#.#.", "....."},   // Diaeresis
        {"..#..", ".##.."},   // Cedilla (below)
        {".###.", ".#.#."},   // Ring
    };
    switch (m) {
        case txt::Mark::Acute:      return ART[0][y];
        case txt::Mark::Grave:      return ART[1][y];
        case txt::Mark::Circumflex: return ART[2][y];
        case txt::Mark::Tilde:      return ART[3][y];
        case txt::Mark::Diaeresis:  return ART[4][y];
        case txt::Mark::Cedilla:    return ART[5][y];
        case txt::Mark::Ring:       return ART[6][y];
        case txt::Mark::None:       return nullptr;
    }
    return nullptr;
}

int topRow(txt::Mark m, char base) {
    if (m == txt::Mark::Cedilla) return 13;
    return isupper((unsigned char)base) ? 0 : 3;
}

}
