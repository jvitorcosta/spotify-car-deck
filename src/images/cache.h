#pragma once
#include <Arduino.h>
#include <FS.h>

// SD-card persistent cache (VSPI; pins in include/pins.h). Holds PokeAPI sprite PNGs
// under /sprites and PMD walk sheets under /pmd. If the card never mounted every
// call is a no-op (false/empty), so a missing card degrades to "always download".
namespace cache {

bool begin();

// Path API.
bool hasPath(const String& path);
bool savePath(const String& path, const uint8_t* data, size_t n);
// Reads the whole file (<= maxLen) into a malloc'ed buffer of n+1 bytes, NUL-terminated.
bool readAll(const String& path, size_t maxLen, uint8_t** out, size_t* outLen);
void removePath(const String& path);

// PokeAPI sprite API (dex-keyed, /sprites/<dex>.png).
String spritePath(int dex);
bool has(int dex);
bool save(int dex, const uint8_t* data, size_t n);
// Spelled fs::File because TFT_eSPI.h defines FS_NO_GLOBALS before FS.h.
fs::File open(int dex);

}
