#pragma once
#include <PNGdec.h>
#include <stddef.h>
#include <stdint.h>

// PokeAPI sprite bytes + the single shared PNGdec instance. PNG objects are large (~45 KB,
// mostly the 32 KB inflate window), so it lives on the heap only while a decode runs: acquire()
// before, release() after; decoder() is valid in between. Static, it kept 45 KB away from the
// TLS handshakes all the time, which then ran out of memory (failed polls, lwIP allocfails).
namespace img {

bool acquire();      // false: no heap for the decoder right now (try again later)
void release();
PNG& decoder();

// PokeAPI sprite bytes for dex `dex` into the caller's buffer: SD cache if present, else HTTPS
// download of `url` (saved to the cache). *fromCache tells the caller where they came from.
bool loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                    bool* fromCache);

}
