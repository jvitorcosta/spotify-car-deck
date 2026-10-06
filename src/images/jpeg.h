#pragma once
#include <TFT_eSPI.h>

// Album-art fetch + decode, split into a network step and a draw step so the
// deck only hits the network when the track changes, not on every redraw.
namespace img {

// Network side: downloads the JPEG at `url` (<= 60000 bytes) into a malloc'ed buffer.
bool downloadAlbumArt(const char* url, uint8_t** out, int* len);

// UI side: takes ownership of JPEG bytes for drawAlbumArt() (frees the previous ones).
// Pass nullptr to clear (new track, nothing to draw yet).
void setAlbumArt(uint8_t* jpeg, int len);

// Decodes the buffer set by setAlbumArt() with TJpg_Decoder, scaling by
// an integer factor (1/2/4/8) so it fits within boxW x boxH, centers it in
// that box, and pushes it to the display at (x, y). Does no network I/O.
// Returns false if there is no cached buffer or the decode fails.
bool drawAlbumArt(TFT_eSPI& t, int x, int y, int boxW, int boxH);

}
