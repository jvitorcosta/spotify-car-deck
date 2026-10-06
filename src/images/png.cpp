#include "png.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PNGdec.h>
#include "cache.h"
#include "../ui/theme.h"

namespace img {

static PNG png;
static TFT_eSPI* g_tft = nullptr;
static int g_x0 = 0, g_y0 = 0;
static int g_scale = 1;        // 1 = native, 2 = half-size (nearest-neighbor)

// PNGdec draw callback: decodes one scanline at a time, compositing transparent
// pixels against the cream panel the sprite sits on (so there's no visible box
// or halo — it reads as a clean cutout on the panel), then blits the line.
// Colors stay correct because pushImage consumes the big-endian RGB565 directly.
static int pngDraw(PNGDRAW* pDraw) {
    uint16_t lineBuf[256];
    png.getLineAsRGB565(pDraw, lineBuf, PNG_RGB565_BIG_ENDIAN, theme::GBA_CREAM);
    if (g_scale == 2) {
        if (pDraw->y & 1) return 1;                 // drop odd source rows
        int w = pDraw->iWidth / 2;
        for (int i = 0; i < w; ++i) lineBuf[i] = lineBuf[i * 2];
        g_tft->pushImage(g_x0, g_y0 + pDraw->y / 2, w, 1, lineBuf);
    } else {
        g_tft->pushImage(g_x0, g_y0 + pDraw->y, pDraw->iWidth, 1, lineBuf);
    }
    return 1;
}

bool loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen) {
    if (!outData || !outLen) return false;
    *outData = nullptr;
    *outLen = 0;

    if (cache::has(dex)) {
        fs::File f = cache::open(dex);
        if (!f) return false;
        size_t n = f.size();
        uint8_t* data = (uint8_t*)malloc(n);
        if (!data) { f.close(); return false; }
        f.read(data, n);
        f.close();
        *outData = data;
        *outLen = n;
        return true;
    }

    if (!url || !url[0]) return false;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) return false;
    if (https.GET() != 200) { https.end(); return false; }
    int len = https.getSize();
    if (len <= 0 || len > 40000) { https.end(); return false; }

    uint8_t* data = (uint8_t*)malloc(len);
    if (!data) { https.end(); return false; }

    WiFiClient* s = https.getStreamPtr();
    int got = 0;
    uint32_t start = millis();
    while (https.connected() && got < len) {
        size_t a = s->available();
        if (a) {
            size_t want = (size_t)(len - got);
            if (a < want) want = a;
            got += s->readBytes(data + got, want);
        } else {
            if (millis() - start > 8000) break; // stalled download, bail out
            delay(1);
        }
    }
    https.end();

    if (got != len) { free(data); return false; }

    *outData = data;
    *outLen = (size_t)len;
    cache::save(dex, data, (size_t)len);
    return true;
}

bool drawSprite(TFT_eSPI& t, const char* url, int dex, int cx, int cy) {
    uint8_t* data = nullptr;
    size_t n = 0;
    if (!loadSpriteBytes(dex, url, &data, &n)) return false;

    int rc = png.openRAM(data, (int)n, pngDraw);
    if (rc != PNG_SUCCESS) { free(data); return false; }

    g_tft = &t;
    int w = png.getWidth(), h = png.getHeight();
    g_scale = (w > 64 || h > 64) ? 2 : 1;   // shrink big 96px sprites to ~48px
    g_x0 = cx - (w / g_scale) / 2;
    g_y0 = cy - (h / g_scale) / 2;
    png.decode(nullptr, 0);
    png.close();
    free(data);
    return true;
}

// --- walk sprite (persists for the current song) ---
static uint16_t g_walk[WALK_MAX * WALK_MAX];
static uint8_t  g_walkMask[WALK_MAX * WALK_MAX];
static int g_walkW = 0, g_walkH = 0;
static bool g_walkReady = false;

// Full-size decode scratch, heap-allocated only while loading.
static const int FULL_MAX = 128;
static uint16_t* g_full = nullptr;
static uint8_t*  g_fullMask = nullptr;

static int pngFullDraw(PNGDRAW* d) {
    if (d->y >= FULL_MAX) return 1;
    int w = d->iWidth < FULL_MAX ? d->iWidth : FULL_MAX;
    uint16_t line[256];
    uint8_t bits[(256 + 7) / 8];
    memset(bits, 0xff, sizeof(bits));   // formats PNGdec can't mask -> opaque
    png.getLineAsRGB565(d, line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, bits, 128);
    for (int i = 0; i < w; ++i) {
        g_full[d->y * FULL_MAX + i]     = line[i];
        g_fullMask[d->y * FULL_MAX + i] = (bits[i >> 3] >> (7 - (i & 7))) & 1;
    }
    return 1;
}

bool loadWalkSprite(const char* url, int dex, int maxSize) {
    g_walkReady = false;
    if (maxSize > WALK_MAX) maxSize = WALK_MAX;
    uint8_t* data = nullptr;
    size_t n = 0;
    if (!loadSpriteBytes(dex, url, &data, &n)) return false;

    g_full = (uint16_t*)malloc(FULL_MAX * FULL_MAX * sizeof(uint16_t));
    g_fullMask = (uint8_t*)malloc(FULL_MAX * FULL_MAX);
    bool ok = false;
    if (g_fullMask) memset(g_fullMask, 0, FULL_MAX * FULL_MAX);   // undecoded rows = clear
    if (g_full && g_fullMask && png.openRAM(data, (int)n, pngFullDraw) == PNG_SUCCESS) {
        int fw = png.getWidth(), fh = png.getHeight();
        if (fw > FULL_MAX) fw = FULL_MAX;
        if (fh > FULL_MAX) fh = FULL_MAX;
        png.decode(nullptr, 0);
        png.close();

        // No usable alpha (corner reads opaque): key out the corner color instead.
        if (g_fullMask[0]) {
            uint16_t key = g_full[0];
            for (int y = 0; y < fh; ++y)
                for (int x = 0; x < fw; ++x)
                    g_fullMask[y * FULL_MAX + x] = g_full[y * FULL_MAX + x] != key;
        }

        // Crop to the opaque bounding box so the creature fills the walker.
        int x0 = fw, y0 = fh, x1 = -1, y1 = -1;
        for (int y = 0; y < fh; ++y)
            for (int x = 0; x < fw; ++x)
                if (g_fullMask[y * FULL_MAX + x]) {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
        if (x1 >= x0 && y1 >= y0) {
            int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
            int big = bw > bh ? bw : bh;
            int ow = bw * maxSize / big, oh = bh * maxSize / big;
            if (ow < 1) ow = 1;
            if (oh < 1) oh = 1;
            for (int y = 0; y < oh; ++y) {
                int sy = y0 + y * bh / oh;
                for (int x = 0; x < ow; ++x) {
                    int sx = x0 + x * bw / ow;
                    g_walk[y * ow + x]     = g_full[sy * FULL_MAX + sx];
                    g_walkMask[y * ow + x] = g_fullMask[sy * FULL_MAX + sx];
                }
            }
            g_walkW = ow;
            g_walkH = oh;
            g_walkReady = ok = true;
        }
    }
    free(g_full);     g_full = nullptr;
    free(g_fullMask); g_fullMask = nullptr;
    free(data);
    return ok;
}

const uint16_t* walkBuffer() { return g_walk; }
const uint8_t* walkMask() { return g_walkMask; }
int walkW() { return g_walkW; }
int walkH() { return g_walkH; }
bool walkReady() { return g_walkReady; }

}
