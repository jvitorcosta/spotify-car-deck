#pragma once
#include <stdint.h>
// Artist genre from Apple's iTunes Search API (no key): one ~300-byte HTTPS request,
// https://itunes.apple.com/search?term=<artist>&entity=musicArtist&limit=1&country=BR.
// Spotify no longer returns artist genres to this app (spec 2026-10-08-genre-badge-design.md).
namespace applegenre {
enum class Result { Found, None, Error };   // None: Apple has no match/genre; Error: try later
// First artist only. rc: HTTP status (negative: connect/TLS/timeout) for the log.
Result lookup(const char* artist, uint32_t* genreId, int* rc);
}
