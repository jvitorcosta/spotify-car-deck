#pragma once
#include <cstddef>
#include <cstdint>
// The boot scene's photo montage (data/montage.bin, built by tools/gen_montage.py): "MTG1",
// u16 count, u16 fps, count x (u32 offset, u32 size), JPEG frames. PURE, host-tested.
// A missing, empty or corrupt file reads as 0 frames.
namespace montage {
bool parse(const uint8_t* blob, size_t len);   // false (and count 0) when the blob is invalid
int count();
int fps();
bool frame(int i, const uint8_t** jpg, size_t* len);
}
