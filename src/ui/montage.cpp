#include "montage.h"
#include <cstring>

namespace montage {

static const uint8_t* s_blob = nullptr;
static int s_count = 0, s_fps = 0;

static uint32_t rd16(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool parse(const uint8_t* blob, size_t len) {
    s_blob = nullptr;
    s_count = s_fps = 0;
    if (!blob || len < 8 || memcmp(blob, "MTG1", 4) != 0) return false;
    const uint32_t n = rd16(blob + 4), fps = rd16(blob + 6);
    const uint64_t indexEnd = 8 + 8 * (uint64_t)n;
    if (fps == 0 || indexEnd > len) return false;
    for (uint32_t i = 0; i < n; ++i) {
        const uint64_t off = rd32(blob + 8 + 8 * i), size = rd32(blob + 12 + 8 * i);
        if (off < indexEnd || off + size > len) return false;
    }
    s_blob = blob;
    s_count = (int)n;
    s_fps = (int)fps;
    return true;
}

int count() { return s_count; }
int fps() { return s_fps; }

bool frame(int i, const uint8_t** jpg, size_t* len) {
    if (i < 0 || i >= s_count) return false;
    *jpg = s_blob + rd32(s_blob + 8 + 8 * i);
    *len = rd32(s_blob + 12 + 8 * i);
    return true;
}

}
