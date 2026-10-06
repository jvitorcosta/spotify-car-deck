#include "walksprite.h"
#include <Arduino.h>
#include "png.h"
#include "cache.h"
#include "fetch.h"
#include "../util/animdata.h"
#include "../util/walkanim.h"

namespace walk {

// Slot layout: all frames' pixels first (keeps uint16 access aligned), then masks.
// Pools and the download scratch are allocated once in begin(), before WiFi; loaders never
// malloc (README "Design & performance history": per-download mallocs with TLS open
// exhausted byte-addressable RAM).
static uint8_t* g_pool[2] = {nullptr, nullptr};
static uint8_t* g_scratch = nullptr;
static Info g_info[2] = {};
static uint16_t g_dur[2][MAX_FRAMES];
static int g_dex[2] = {0, 0};
static int g_active = 0;
static inline int staged() { return 1 - g_active; }

bool begin() {
    for (int i = 0; i < 2; ++i)
        if (!g_pool[i]) g_pool[i] = (uint8_t*)malloc(CAP_BYTES);   // malloc is 4-byte aligned
    if (!g_scratch) g_scratch = (uint8_t*)malloc(SCRATCH);
    bool ok = g_pool[0] && g_pool[1] && g_scratch;
    if (!ok) Serial.println("[walk] no heap for frame pools");
    return ok;
}

static uint16_t* pixelsAt(int s, int f) {
    return (uint16_t*)g_pool[s] + f * g_info[s].w * g_info[s].h;
}
static uint8_t* maskAt(int s, int f) {
    return g_pool[s] + g_info[s].frames * g_info[s].w * g_info[s].h * 2 + f * g_info[s].w * g_info[s].h;
}

const Info& info() { return g_info[g_active]; }
const uint16_t* pixels(int f) { return pixelsAt(g_active, f); }
const uint8_t* mask(int f) { return maskAt(g_active, f); }
uint16_t durationMs(int f) {
    return (f >= 0 && f < g_info[g_active].frames) ? g_dur[g_active][f] : 0;
}
int stagedDex() { return g_info[staged()].ready ? g_dex[staged()] : 0; }
void promote() { g_active = staged(); }

static inline bool bitAt(const uint8_t* bits, int x) { return (bits[x >> 3] >> (7 - (x & 7))) & 1; }

// ------------------------------------------------- two-pass region decode (PMD + fallback)
// Decodes rows [rowY0, rowY0+rowH) of a sheet holding `frames` frames of frameW pixels side
// by side. Pass 1 finds the opaque bounding box shared by all frames; pass 2 writes the
// cropped, band-fitted frames straight into the staged slot. No full-size copy.
enum class Res { Ok, Unsupported, Corrupt };
static const int SHEET_MAX_W = 512;
static int s_frameW, s_rowH, s_rowY0, s_frames, s_pass, s_k, s_kept, s_slot;
static bool s_keyMode;                   // no alpha channel: top-left colour is transparent
static uint16_t s_key;
static walkanim::Box s_box;
static walkanim::Fit s_fit;
static uint16_t s_line[SHEET_MAX_W];     // static: keep the PNGdec callback stack small
static uint8_t s_bits[SHEET_MAX_W / 8];

static inline bool opaqueAt(int x) {
    return s_keyMode ? s_line[x] != s_key : bitAt(s_bits, x);
}

static int regionDraw(PNGDRAW* d) {
    int ry = d->y - s_rowY0;
    if (ry < 0 || ry >= s_rowH) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    if (s_pass == 1) {
        if (s_keyMode && ry == 0) s_key = s_line[0];
        for (int x = 0; x < s_frames * s_frameW; ++x)
            if (opaqueAt(x)) walkanim::include(s_box, x % s_frameW, ry);
        return 1;
    }
    int bw = s_box.x1 - s_box.x0 + 1, bh = s_box.y1 - s_box.y0 + 1;
    for (int oy = 0; oy < s_fit.h; ++oy) {
        if (walkanim::srcIndex(oy, s_fit.h, s_box.y0, bh) != ry) continue;
        for (int fi = 0; fi < s_kept; ++fi) {
            int f = fi * s_k;
            uint16_t* px = pixelsAt(s_slot, fi);
            uint8_t* m = maskAt(s_slot, fi);
            for (int ox = 0; ox < s_fit.w; ++ox) {
                int sx = f * s_frameW + walkanim::srcIndex(ox, s_fit.w, s_box.x0, bw);
                px[oy * s_fit.w + ox] = s_line[sx];
                m[oy * s_fit.w + ox] = opaqueAt(sx);
            }
        }
    }
    return 1;
}

// frameW <= 0 means "the whole image width is one frame" (fallback sprite).
static Res decodeRegion(uint8_t* data, size_t n, int rowY0, int rowH, int frameW, int maxFrames,
                        bool pmd) {
    PNG& png = img::decoder();
    s_rowY0 = rowY0;
    for (s_pass = 1; s_pass <= 2; ++s_pass) {
        if (png.openRAM(data, (int)n, regionDraw) != PNG_SUCCESS) return Res::Corrupt;
        if (s_pass == 1) {
            int w = png.getWidth(), h = png.getHeight();
            if (w > SHEET_MAX_W || !walkanim::pngFits(w, png.getPixelType(), png.getBpp())) {
                png.close();
                return Res::Unsupported;
            }
            s_frameW = frameW > 0 ? frameW : w;
            s_rowH = rowH > 0 ? rowH : h;
            if (h < s_rowY0 + s_rowH) { png.close(); return Res::Corrupt; }
            s_frames = w / s_frameW;
            if (maxFrames < s_frames) s_frames = maxFrames;
            if (s_frames < 1) { png.close(); return Res::Corrupt; }
            s_keyMode = !pmd && !png.hasAlpha();
            s_box = walkanim::emptyBox();
        }
        int rc = png.decode(nullptr, 0);
        png.close();
        if (rc != PNG_SUCCESS) return Res::Corrupt;
        if (s_pass == 1) {
            if (walkanim::isEmpty(s_box)) return Res::Corrupt;
            s_fit = walkanim::fitBand(s_box.x1 - s_box.x0 + 1, s_box.y1 - s_box.y0 + 1, BAND_H, MAX_W);
            s_k = walkanim::keepEvery(s_frames, s_fit.w, s_fit.h, CAP_BYTES);
            if (s_k == 0) return Res::Unsupported;
            s_kept = walkanim::keptCount(s_frames, s_k);
            g_info[s_slot] = {false, pmd, s_fit.w, s_fit.h, s_kept};   // layout for pass 2
        }
    }
    return Res::Ok;
}

// ---------------------------------------------------------------- PMD sheet
static const int DIR_RIGHT = 2;          // PMD row order: Down, DownRight, Right, ...

// Cache-or-download into the scratch buffer (NUL-terminated).
// partial: the beginning is enough (AnimData.xml: Walk is anim index 0, near the top).
static bool getCached(const String& path, const String& url, size_t* n, int* code,
                      bool partial = false) {
    *code = 0;
    if (cache::readInto(path, g_scratch, SCRATCH, n)) return true;
    if (!fetch::httpsGetInto(url.c_str(), g_scratch, SCRATCH, n, code, partial)) return false;
    cache::savePath(path, g_scratch, *n);
    return true;
}

bool loadPmd(int dex) {
    s_slot = staged();
    g_info[s_slot].ready = false;
    g_dex[s_slot] = 0;
    if (dex < 1 || !g_pool[s_slot] || !g_scratch) return false;
    char base[96];
    snprintf(base, sizeof(base),
             "https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/", dex);
    String xmlPath = "/pmd/" + String(dex) + ".xml";
    String pngPath = "/pmd/" + String(dex) + ".png";

    size_t n = 0; int code = 0;
    if (!getCached(xmlPath, String(base) + "AnimData.xml", &n, &code, true)) {
        Serial.printf("[walk] fallback (xml http %d)\n", code);
        return false;
    }
    animdata::WalkAnim a = animdata::parseWalk((const char*)g_scratch);   // scratch is free after this
    if (!a.ok) {
        cache::removePath(xmlPath);
        Serial.println("[walk] fallback (no Walk anim)");
        return false;
    }
    if (!getCached(pngPath, String(base) + "Walk-Anim.png", &n, &code)) {
        Serial.printf("[walk] fallback (png http %d or > %d B)\n", code, SCRATCH);
        return false;
    }
    Res r = decodeRegion(g_scratch, n, DIR_RIGHT * a.frameH, a.frameH, a.frameW, a.frames, true);
    if (r != Res::Ok) {
        g_info[s_slot].ready = false;
        if (r == Res::Corrupt) {   // corrupt: drop the cache so the next play re-downloads
            cache::removePath(xmlPath);
            cache::removePath(pngPath);
        }
        Serial.printf("[walk] fallback (sheet %s)\n", r == Res::Corrupt ? "corrupt" : "unsupported");
        return false;
    }
    walkanim::mergedDurationsMs(a.ticks, s_frames, s_k, g_dur[s_slot]);
    g_info[s_slot].ready = true;
    g_dex[s_slot] = dex;
    Serial.printf("[walk] pmd #%d %d frames %dx%d (k=%d)\n", dex, g_info[s_slot].frames,
                  g_info[s_slot].w, g_info[s_slot].h, s_k);
    return true;
}

// ---------------------------------------------------------------- fallback
bool loadFallback(const char* spriteUrl, int dex) {
    s_slot = staged();
    g_info[s_slot].ready = false;
    g_dex[s_slot] = 0;
    if (!g_pool[s_slot] || !g_scratch) return false;
    size_t n = 0;
    bool fromCache = false;
    if (!img::loadSpriteInto(dex, spriteUrl, g_scratch, SCRATCH, &n, &fromCache)) {
        Serial.println("[walk] no sprite");
        return false;
    }
    Res r = decodeRegion(g_scratch, n, 0, 0, 0, 1, false);
    if (r != Res::Ok) {
        g_info[s_slot].ready = false;
        if (r == Res::Corrupt && fromCache) cache::removePath(cache::spritePath(dex));
        Serial.printf("[walk] no walker (sprite %s)\n", r == Res::Corrupt ? "corrupt" : "unsupported");
        return false;
    }
    g_dur[s_slot][0] = 0;
    g_info[s_slot].ready = true;
    g_dex[s_slot] = dex;
    Serial.printf("[walk] fallback #%d sprite %dx%d\n", dex, g_info[s_slot].w, g_info[s_slot].h);
    return true;
}

}
