#include "png.h"
#include <new>
#include "cache.h"
#include "fetch.h"

namespace img {

static PNG* s_png = nullptr;

bool acquire() {
    if (s_png) return true;
    void* m = malloc(sizeof(PNG));
    if (!m) return false;
    s_png = new (m) PNG();
    return true;
}

void release() {
    if (!s_png) return;
    s_png->~PNG();
    free(s_png);
    s_png = nullptr;
}

PNG& decoder() { return *s_png; }

bool loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                    bool* fromCache) {
    *fromCache = cache::readInto(cache::spritePath(dex), buf, cap, outLen);
    if (*fromCache) return true;
    if (!fetch::httpsGetInto(url, buf, cap, outLen)) return false;
    cache::save(dex, buf, *outLen);
    return true;
}

}
