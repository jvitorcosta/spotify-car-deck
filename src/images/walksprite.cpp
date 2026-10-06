#include "walksprite.h"
#include <Arduino.h>
#include "png.h"
#include "cache.h"
#include "fetch.h"
#include "../util/animdata.h"
#include "../util/walkanim.h"

namespace walk {

// Store layout: all frames' pixels first (keeps uint16 access aligned), then masks.
// Heap-allocated once on first use and kept: static DRAM is too full for 16 KB.
static uint8_t* g_pool = nullptr;
static bool ensurePool() {
    if (!g_pool) g_pool = (uint8_t*)malloc(CAP_BYTES);   // malloc is 4-byte aligned
    if (!g_pool) Serial.println("[walk] no heap for frame pool");
    return g_pool != nullptr;
}
static Info g_info{false, false, 0, 0, 0};
static uint16_t g_durMs[MAX_FRAMES];

const Info& info() { return g_info; }
static uint16_t* pixelsW(int f) { return (uint16_t*)g_pool + f * g_info.w * g_info.h; }
static uint8_t* maskW(int f) {
    return g_pool + g_info.frames * g_info.w * g_info.h * 2 + f * g_info.w * g_info.h;
}
const uint16_t* pixels(int f) { return pixelsW(f); }
const uint8_t* mask(int f) { return maskW(f); }
uint16_t durationMs(int f) { return (f >= 0 && f < g_info.frames) ? g_durMs[f] : 0; }

static inline bool bitAt(const uint8_t* bits, int x) { return (bits[x >> 3] >> (7 - (x & 7))) & 1; }

// ---------------------------------------------------------------- PMD sheet
static const int DIR_RIGHT = 2;          // PMD row order: Down, DownRight, Right, ...
static const int SHEET_MAX_W = 512;
static int s_fw, s_fh, s_rowY0, s_frames, s_pass, s_k, s_kept;
static walkanim::Box s_box;
static walkanim::Fit s_fit;
static uint16_t s_line[SHEET_MAX_W];     // static: keep the PNGdec callback stack small
static uint8_t s_bits[SHEET_MAX_W / 8];

static int pmdDraw(PNGDRAW* d) {
    int ry = d->y - s_rowY0;
    if (ry < 0 || ry >= s_fh) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    if (s_pass == 1) {   // measure the bounding box shared by every frame of the row
        for (int x = 0; x < s_frames * s_fw; ++x)
            if (bitAt(s_bits, x)) walkanim::include(s_box, x % s_fw, ry);
        return 1;
    }
    int bw = s_box.x1 - s_box.x0 + 1, bh = s_box.y1 - s_box.y0 + 1;
    for (int oy = 0; oy < s_fit.h; ++oy) {
        if (walkanim::srcIndex(oy, s_fit.h, s_box.y0, bh) != ry) continue;
        for (int fi = 0; fi < s_kept; ++fi) {
            int f = fi * s_k;
            uint16_t* px = pixelsW(fi);
            uint8_t* m = maskW(fi);
            for (int ox = 0; ox < s_fit.w; ++ox) {
                int sx = f * s_fw + walkanim::srcIndex(ox, s_fit.w, s_box.x0, bw);
                px[oy * s_fit.w + ox] = s_line[sx];
                m[oy * s_fit.w + ox] = bitAt(s_bits, sx);
            }
        }
    }
    return 1;
}

static bool decodeRow(uint8_t* data, size_t n, const animdata::WalkAnim& a) {
    PNG& png = img::decoder();
    s_fw = a.frameW; s_fh = a.frameH; s_rowY0 = DIR_RIGHT * a.frameH;
    for (s_pass = 1; s_pass <= 2; ++s_pass) {
        if (png.openRAM(data, (int)n, pmdDraw) != PNG_SUCCESS) return false;
        if (s_pass == 1) {
            int sw = png.getWidth();
            if (sw > SHEET_MAX_W || png.getHeight() < s_rowY0 + s_fh) { png.close(); return false; }
            s_frames = sw / s_fw;
            if (a.frames < s_frames) s_frames = a.frames;
            if (s_frames < 1) { png.close(); return false; }
            s_box = walkanim::emptyBox();
        }
        int rc = png.decode(nullptr, 0);
        png.close();
        if (rc != PNG_SUCCESS) return false;
        if (s_pass == 1) {
            if (walkanim::isEmpty(s_box)) return false;
            s_fit = walkanim::fitBand(s_box.x1 - s_box.x0 + 1, s_box.y1 - s_box.y0 + 1, BAND_H, MAX_W);
            s_k = walkanim::keepEvery(s_frames, s_fit.w, s_fit.h, CAP_BYTES);
            if (s_k == 0) return false;
            s_kept = walkanim::keptCount(s_frames, s_k);
            g_info = {false, true, s_fit.w, s_fit.h, s_kept};   // layout for pass 2 writes
        }
    }
    walkanim::mergedDurationsMs(a.ticks, s_frames, s_k, g_durMs);
    g_info.ready = true;
    return true;
}

// Cache-or-download into a NUL-terminated heap buffer.
static bool getCached(const String& path, const String& url, size_t maxLen,
                      uint8_t** out, size_t* n, int* code) {
    *code = 0;
    if (cache::readAll(path, maxLen, out, n)) return true;
    if (!fetch::httpsGet(url.c_str(), maxLen, out, n, code)) return false;
    cache::savePath(path, *out, *n);
    return true;
}

bool loadPmd(int dex) {
    g_info.ready = false;
    if (dex < 1 || !ensurePool()) return false;
    char base[96];
    snprintf(base, sizeof(base),
             "https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/", dex);
    String xmlPath = "/pmd/" + String(dex) + ".xml";
    String pngPath = "/pmd/" + String(dex) + ".png";

    uint8_t* xml = nullptr; size_t xn = 0; int code = 0;
    if (!getCached(xmlPath, String(base) + "AnimData.xml", 32768, &xml, &xn, &code)) {
        Serial.printf("[walk] fallback (xml http %d)\n", code);
        return false;
    }
    animdata::WalkAnim a = animdata::parseWalk((const char*)xml);
    free(xml);
    if (!a.ok) {
        cache::removePath(xmlPath);
        Serial.println("[walk] fallback (no Walk anim)");
        return false;
    }
    uint8_t* png = nullptr; size_t pn = 0;
    if (!getCached(pngPath, String(base) + "Walk-Anim.png", 40000, &png, &pn, &code)) {
        Serial.printf("[walk] fallback (png http %d)\n", code);
        return false;
    }
    bool ok = decodeRow(png, pn, a);
    free(png);
    if (!ok) {   // corrupt or unusable: drop the cache so the next play re-downloads
        cache::removePath(xmlPath);
        cache::removePath(pngPath);
        g_info.ready = false;
        Serial.println("[walk] fallback (sheet decode)");
        return false;
    }
    Serial.printf("[walk] pmd %d frames %dx%d (k=%d)\n", g_info.frames, g_info.w, g_info.h, s_k);
    return true;
}

// ---------------------------------------------------------------- fallback
static const int FULL_MAX = 128;
static uint16_t* f_px = nullptr;
static uint8_t* f_mask = nullptr;
static int f_w = 0, f_h = 0;

static int fullDraw(PNGDRAW* d) {
    if (d->y >= f_h) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    for (int x = 0; x < f_w; ++x) {
        f_px[d->y * f_w + x] = s_line[x];
        f_mask[d->y * f_w + x] = bitAt(s_bits, x);
    }
    return 1;
}

bool loadFallback(const char* spriteUrl, int dex) {
    g_info.ready = false;
    if (!ensurePool()) return false;
    uint8_t* data = nullptr; size_t n = 0;
    if (!img::loadSpriteBytes(dex, spriteUrl, &data, &n)) {
        Serial.println("[walk] no sprite");
        return false;
    }
    PNG& png = img::decoder();
    bool ok = false;
    if (png.openRAM(data, (int)n, fullDraw) == PNG_SUCCESS) {
        f_w = png.getWidth(); f_h = png.getHeight();
        bool alpha = png.hasAlpha();
        if (f_w <= FULL_MAX && f_h <= FULL_MAX) {
            f_px = (uint16_t*)malloc(f_w * f_h * 2);
            f_mask = (uint8_t*)calloc(f_w * f_h, 1);       // undecoded rows = clear
            if (f_px && f_mask && png.decode(nullptr, 0) == PNG_SUCCESS) {
                if (!alpha) {   // no alpha channel: key out the corner colour
                    uint16_t key = f_px[0];
                    for (int i = 0; i < f_w * f_h; ++i) f_mask[i] = f_px[i] != key;
                }
                walkanim::Box b = walkanim::emptyBox();
                for (int y = 0; y < f_h; ++y)
                    for (int x = 0; x < f_w; ++x)
                        if (f_mask[y * f_w + x]) walkanim::include(b, x, y);
                if (!walkanim::isEmpty(b)) {
                    int bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
                    walkanim::Fit fit = walkanim::fitBand(bw, bh, BAND_H, MAX_W);
                    g_info = {false, false, fit.w, fit.h, 1};
                    uint16_t* px = pixelsW(0);
                    uint8_t* m = maskW(0);
                    for (int y = 0; y < fit.h; ++y) {
                        int sy = walkanim::srcIndex(y, fit.h, b.y0, bh);
                        for (int x = 0; x < fit.w; ++x) {
                            int sx = walkanim::srcIndex(x, fit.w, b.x0, bw);
                            px[y * fit.w + x] = f_px[sy * f_w + sx];
                            m[y * fit.w + x] = f_mask[sy * f_w + sx];
                        }
                    }
                    g_durMs[0] = 0;
                    g_info.ready = ok = true;
                }
            } else if (!f_px || !f_mask) {
                Serial.printf("[walk] no heap for %dx%d sprite\n", f_w, f_h);
            }
            free(f_px); f_px = nullptr;
            free(f_mask); f_mask = nullptr;
        }
        png.close();
    }
    free(data);
    if (ok) Serial.printf("[walk] fallback sprite %dx%d\n", g_info.w, g_info.h);
    return ok;
}

}
