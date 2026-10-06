#pragma once
#include <PNGdec.h>
#include <stddef.h>
#include <stdint.h>

// PokeAPI sprite bytes + the single shared PNGdec instance. PNG objects are large
// (~45 KB of inflate state), so every PNG decode in the firmware uses this one.
namespace img {

PNG& decoder();

// PokeAPI sprite bytes for dex `dex` into the caller's buffer: SD cache if present, else HTTPS
// download of `url` (saved to the cache). *fromCache tells the caller where they came from.
bool loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                    bool* fromCache);

}
