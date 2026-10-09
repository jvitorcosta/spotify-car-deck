#include "loudness.h"

namespace loudness {

uint8_t loudness(const uint8_t* pcm16le, size_t samples, size_t at, size_t window) {
    if (at >= samples) return 0;
    const size_t end = (window > samples - at) ? samples : at + window;
    int32_t peak = 0;
    for (size_t i = at; i < end; ++i) {
        const int16_t s = (int16_t)(pcm16le[2 * i] | (pcm16le[2 * i + 1] << 8));
        const int32_t a = s < 0 ? -(int32_t)s : s;
        if (a > peak) peak = a;
    }
    const int32_t v = (peak * 255 + 16383) / 32767;
    return (uint8_t)(v > 255 ? 255 : v);
}

}
