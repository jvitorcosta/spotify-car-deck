#pragma once
#include <cstddef>
#include <cstdint>
// Loudness of a 16-bit signed little-endian PCM clip, for the boot scene's lights. PURE,
// host-tested. Reads bytes, so the clip may sit at any address (embedded files are unaligned).
namespace loudness {
// Peak absolute sample in [at, at + window), clamped to the clip, scaled to 0-255
// (32767 -> 255). 0 when `at` is past the end.
uint8_t loudness(const uint8_t* pcm16le, size_t samples, size_t at, size_t window);
}
