#pragma once
#include <TFT_eSPI.h>

// Album-art fetch + decode, split into a network step and a draw step so the
// deck only hits the network when the track changes, not on every redraw.
namespace img {

// Downloads the JPEG at `url` into a persistent heap buffer, replacing any
// previously cached buffer. No decoding or drawing happens here. Guards the
// download to <= 60000 bytes (no PSRAM on this board). Returns true if a
// buffer is cached and ready for drawAlbumArt().
bool cacheAlbumArt(const char* url);

// Decodes the buffer cached by cacheAlbumArt() with TJpg_Decoder, scaling by
// an integer factor (1/2/4/8) so it fits within boxW x boxH, centers it in
// that box, and pushes it to the display at (x, y). Does no network I/O.
// Returns false if there is no cached buffer or the decode fails.
bool drawAlbumArt(TFT_eSPI& t, int x, int y, int boxW, int boxH);

}
