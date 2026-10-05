#pragma once
#include <TFT_eSPI.h>
#include <stddef.h>
#include <stdint.h>

// Pokémon sprite PNG: SD-cache-or-download byte loader + PNGdec decode/draw.
namespace img {

// Loads the raw PNG bytes for dex `dex`: from the SD cache (images::cache)
// if present, otherwise downloads `url` over HTTPS (guarded to <= 40000
// bytes, no PSRAM on this board) and saves the result to the cache for next
// time. On success *outData is a caller-owned heap buffer (free() it) of
// length *outLen. Shared by drawSprite() and reusable wherever sprite bytes
// are needed without an immediate draw (e.g. prefetching).
bool loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen);

// Loads (via loadSpriteBytes) and PNGdec-decodes the sprite for `dex`,
// drawing it centered at (cx,cy) on `t`. Transparent pixels are composited
// against the cream panel background (theme::GBA_CREAM). Returns false if
// the bytes can't be loaded or the PNG fails to decode.
bool drawSprite(TFT_eSPI& t, const char* url, int dex, int cx, int cy);

}
