#include "png.h"
#include "cache.h"
#include "fetch.h"

namespace img {

static PNG s_png;
PNG& decoder() { return s_png; }

bool loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                    bool* fromCache) {
    *fromCache = cache::readInto(cache::spritePath(dex), buf, cap, outLen);
    if (*fromCache) return true;
    if (!fetch::httpsGetInto(url, buf, cap, outLen)) return false;
    cache::save(dex, buf, *outLen);
    return true;
}

}
