#pragma once
#include <PNGdec.h>
#include <stddef.h>
#include <stdint.h>

// PokeAPI sprite bytes + the single shared PNGdec instance. PNG objects are large
// (~45 KB of inflate state), so every PNG decode in the firmware uses this one.
namespace img {

PNG& decoder();

// Raw PNG bytes for dex `dex`: SD cache if present, else HTTPS download of `url`
// (<= 40000 bytes) saved to the cache. *outData is malloc'ed (free() it).
bool loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen);

}
