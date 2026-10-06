#include "png.h"
#include "cache.h"
#include "fetch.h"

namespace img {

static PNG g_png;
PNG& decoder() { return g_png; }

bool loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen) {
    if (cache::readAll(cache::spritePath(dex), 40000, outData, outLen)) return true;
    if (!fetch::httpsGet(url, 40000, outData, outLen)) return false;
    cache::save(dex, *outData, *outLen);
    return true;
}

}
