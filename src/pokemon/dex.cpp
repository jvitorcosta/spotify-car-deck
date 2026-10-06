#include "dex.h"
#include <cstdio>

namespace dex {

struct DexEntry { const char* name; uint8_t type; };
#include "dex_data.inc"

const char* name(int n) { return (n >= 1 && n <= COUNT) ? DEX[n - 1].name : ""; }
const char* type(int n) { return (n >= 1 && n <= COUNT) ? TYPE_NAMES[DEX[n - 1].type] : ""; }
int fromRandom(uint32_t r) { return (int)(r % COUNT) + 1; }
void spriteUrl(int n, char* out, size_t len) {
    snprintf(out, len,
             "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png", n);
}

}
