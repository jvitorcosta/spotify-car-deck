#pragma once
#include <Arduino.h>
#include <FS.h>

// SD-card persistent cache for Pokémon sprite PNGs, keyed by national-dex
// number. Lets a re-encountered dex number load instantly with no network
// round-trip. Uses the VSPI bus (see include/pins.h for SD_CS/SCK/MISO/MOSI).
namespace cache {

// Mounts the SD card and ensures /sprites exists. Safe to call once in
// setup(); all other functions are no-ops (return false/empty) if the card
// never mounted, so a missing SD card degrades to "always download".
bool begin();

String spritePath(int dex);
bool has(int dex);
bool save(int dex, const uint8_t* data, size_t n);
// Read handle; caller checks validity. Spelled fs::File (not the global
// `File` alias) because TFT_eSPI.h defines FS_NO_GLOBALS before including
// FS.h, which suppresses that alias in any translation unit that includes
// TFT_eSPI.h ahead of this header.
fs::File open(int dex);

}
