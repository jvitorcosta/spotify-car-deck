#include "bootanim.h"
#include <cmath>

namespace bootanim {

static constexpr float TWO_PI = 6.28318530718f;

int cruiseX(uint32_t t) {
    const uint32_t p = (t - ENTER_MS) % SWAY_MS;
    return CRUISE_X + (int)lroundf(SWAY_PX * sinf(TWO_PI * (float)p / (float)SWAY_MS));
}

int carX(uint32_t t, uint32_t tExit) {
    if (tExit != NO_EXIT) {
        const uint32_t dt = t - tExit;
        if (dt >= EXIT_MS) return END_X;
        const float q = (float)dt / (float)EXIT_MS;
        const int xc = cruiseX(tExit);
        return xc + (int)lroundf((float)(END_X - xc) * q * q);
    }
    if (t < ENTER_MS) {
        const float q = (float)t / (float)ENTER_MS;
        return START_X + (int)lroundf((float)(CRUISE_X - START_X) * (1.0f - (1.0f - q) * (1.0f - q)));
    }
    return cruiseX(t);
}

bool exitDone(uint32_t t, uint32_t tExit) { return tExit != NO_EXIT && t - tExit >= EXIT_MS; }

bool mayExit(uint32_t t, bool soundDone, bool online) {
    return t >= MIN_SHOW_MS && soundDone && online;
}

int bob(uint32_t t) { return ((uint64_t)t * 6 / 1000) % 5 == 0 ? 1 : 0; }

float spokeAngle(uint32_t t) { return (float)t * 9.0f / 1000.0f; }

int scroll(uint32_t t, int pxPerS, int period) {
    return (int)(((uint64_t)t * (uint64_t)pxPerS / 1000) % (uint64_t)period);
}

bool starHidden(uint32_t t, int i) { return ((uint64_t)t * 3 / 1000 + (uint64_t)i) % 7 == 0; }

uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
    const uint32_t a = alpha, na = 255 - alpha;
    const uint32_t r = (((fg >> 11) & 31) * a + ((bg >> 11) & 31) * na + 127) / 255;
    const uint32_t g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * na + 127) / 255;
    const uint32_t b = ((fg & 31) * a + (bg & 31) * na + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint16_t scale565(uint16_t c, uint8_t level) { return blend565(c, 0x0000, level); }

uint16_t hue565(uint32_t t) {
    const float h = (float)(((uint64_t)t * 120 / 1000) % 360) / 360.0f;
    const float l = 0.58f, q = 1.0f, p = 2.0f * l - q;   // saturation 100 %
    auto f = [&](float x) {
        x -= floorf(x);
        if (x < 1.0f / 6) return p + (q - p) * 6 * x;
        if (x < 0.5f) return q;
        if (x < 2.0f / 3) return p + (q - p) * (2.0f / 3 - x) * 6;
        return p;
    };
    return rgb565((uint8_t)lroundf(f(h + 1.0f / 3) * 255), (uint8_t)lroundf(f(h) * 255),
                  (uint8_t)lroundf(f(h - 1.0f / 3) * 255));
}

uint16_t headColor(uint8_t e) {
    const uint8_t l = (uint8_t)(160 + 95 * e / 255);
    return rgb565(l, l, (uint8_t)(l * 4 / 5));
}
uint8_t tailLevel(uint8_t e) { return (uint8_t)(140 + 115 * e / 255); }
uint8_t tailGlowAlpha(uint8_t e) { return e > 12 ? (uint8_t)(128 * e / 255) : 0; }
uint8_t beamAlpha(uint8_t e) { return (uint8_t)(40 + 56 * e / 255); }
uint8_t glowAlpha(uint8_t e) { return (uint8_t)(115 + 102 * e / 255); }

uint8_t reflectAlpha(int d, bool upper) {
    if (d < 0) d = -d;
    const int reach = upper ? 26 : 18, peak = upper ? 140 : 77;
    return d >= reach ? 0 : (uint8_t)(peak * (reach - d) / reach);
}

bool clip(int& a, int& b, int lo, int hi) {
    if (a < lo) a = lo;
    if (b > hi) b = hi;
    return a < b;
}


// ---- ending: brake (v2), then the photo montage (v3) ----
int brakeX(int xc, uint32_t u) {
    const float q = u >= BRAKE_MS ? 1.0f : (float)u / BRAKE_MS;
    return xc + (int)lroundf(40.0f * (1.0f - (1.0f - q) * (1.0f - q)));
}

uint32_t brakeTime(uint32_t tHero, uint32_t u) {
    const uint32_t uc = u < BRAKE_MS ? u : BRAKE_MS;
    const float q = (float)uc / BRAKE_MS;
    return tHero + (uint32_t)lroundf((float)uc * (1.0f - q / 2.0f));   // integral of (1 - q)
}

uint8_t brakeLevel(uint32_t u) {
    if (u >= BRAKE_MS) return 0;
    return (uint8_t)lroundf(255.0f * (1.0f - (float)u / BRAKE_MS));
}

int montageFrame(uint32_t u, int fps, int count) {
    if (u < BRAKE_MS) return -1;
    if (count <= 0 || fps <= 0) return count > 0 ? count : 0;
    const uint64_t i = (uint64_t)(u - BRAKE_MS) * (uint64_t)fps / 1000;
    return i >= (uint64_t)count ? count : (int)i;
}

int finaleStartFrame(int count, int fps, uint32_t finaleMs) {
    if (count <= 0 || fps <= 0 || finaleMs == 0) return -1;
    const int frames = (int)(((uint64_t)finaleMs * (uint64_t)fps + 999) / 1000);   // rounded up
    return frames >= count ? 0 : count - frames;
}

bool openerFits(int finaleAt, int fps, uint32_t openerMs) {
    if (openerMs == 0 || fps <= 0) return false;
    if (finaleAt < 0) return true;
    const int frames = (int)(((uint64_t)openerMs * (uint64_t)fps + 999) / 1000);   // rounded up
    return frames <= finaleAt;
}
}
