#include "walkanim.h"

namespace walkanim {

Box emptyBox() { return {1 << 30, 1 << 30, -1, -1}; }

void include(Box& b, int x, int y) {
    if (x < b.x0) b.x0 = x;
    if (x > b.x1) b.x1 = x;
    if (y < b.y0) b.y0 = y;
    if (y > b.y1) b.y1 = y;
}

bool isEmpty(const Box& b) { return b.x1 < b.x0 || b.y1 < b.y0; }

Fit fitBand(int bw, int bh, int maxH, int maxW) {
    if (bw <= maxW && bh <= maxH) return {bw, bh};
    int w = bw * maxH / bh, h = maxH;
    if (w > maxW) { w = maxW; h = bh * maxW / bw; }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return {w, h};
}

int srcIndex(int o, int outLen, int srcStart, int srcLen) {
    return srcStart + o * srcLen / outLen;
}

int keptCount(int frames, int k) { return (frames + k - 1) / k; }

int keepEvery(int frames, int w, int h, int capBytes) {
    int per = w * h * 3;
    if (frames < 1 || per > capBytes) return 0;
    for (int k = 1; k <= frames; ++k)
        if (keptCount(frames, k) * per <= capBytes) return k;
    return frames;
}

void mergedDurationsMs(const uint16_t* ticks, int n, int k, uint16_t* outMs) {
    for (int i = 0; i * k < n; ++i) {
        uint32_t t = 0;
        for (int j = i * k; j < i * k + k && j < n; ++j) t += ticks[j];
        outMs[i] = (uint16_t)(t * 1000 / 60);
    }
}

int frameAt(uint32_t elapsedMs, const uint16_t* durMs, int n) {
    if (n <= 0) return 0;
    uint32_t total = 0;
    for (int i = 0; i < n; ++i) total += durMs[i];
    if (total == 0) return 0;
    uint32_t t = elapsedMs % total;
    for (int i = 0; i < n; ++i) {
        if (t < durMs[i]) return i;
        t -= durMs[i];
    }
    return n - 1;
}

bool pngFits(int width, int pixelType, int bpp, int maxBuffered) {
    int ch = 4;
    switch (pixelType) {
        case 0: ch = 1; break;
        case 2: ch = 3; break;
        case 3: ch = 1; break;
        case 4: ch = 2; break;
        case 6: ch = 4; break;
    }
    int pitch = (width * ch * bpp + 7) / 8;
    return 2 * (pitch + 16) <= maxBuffered;
}

}
