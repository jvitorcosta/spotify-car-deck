#include "jpeg.h"
#include <TJpg_Decoder.h>
#include "fetch.h"

namespace img {

// Persistent compressed-JPEG cache. Only the compressed bytes are held in
// RAM; the decoded bitmap is streamed straight to the display in MCU blocks
// by TJpg_Decoder's callback, never materialized in full.
static uint8_t* g_buf = nullptr;
static int g_len = 0;

// Target/offset for the active drawAlbumArt() call, read by the TJpg_Decoder
// callback (which only knows image-local coordinates).
static TFT_eSPI* g_tft = nullptr;
static int g_ox = 0;
static int g_oy = 0;

static bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
    if (!g_tft) return false;
    if (y >= g_tft->height()) return false; // off-screen, stop early
    g_tft->pushImage(g_ox + x, g_oy + y, w, h, bmp);
    return true;
}

// Decode-to-buffer callback used when we resample the cover to exactly fit its
// (square) box so the WHOLE cover shows, filling the box with no crop/margin.
static uint16_t* g_decBuf = nullptr;
static int g_decW = 0, g_decH = 0;
static bool bufOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
    for (int row = 0; row < h; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= g_decH) continue;
        for (int col = 0; col < w; ++col) {
            int dx = x + col;
            if (dx < 0 || dx >= g_decW) continue;
            g_decBuf[dy * g_decW + dx] = bmp[row * w + col];
        }
    }
    return true;
}

bool downloadAlbumArt(const char* url, uint8_t** out, int* len) {
    uint8_t* data = nullptr;
    size_t n = 0;
    if (!fetch::httpsGet(url, 60000, &data, &n)) return false;   // guard RAM; no PSRAM
    *out = data;
    *len = (int)n;
    return true;
}

void setAlbumArt(uint8_t* jpeg, int len) {
    free(g_buf);
    g_buf = jpeg;
    g_len = jpeg ? len : 0;
}

bool drawAlbumArt(TFT_eSPI& t, int x, int y, int boxW, int boxH) {
    if (!g_buf || g_len <= 0) return false;

    uint16_t jw = 0, jh = 0;
    TJpgDec.getJpgSize(&jw, &jh, g_buf, g_len);
    if (jw == 0 || jh == 0) return false;

    // Decode at the smallest scale whose image still covers the box, into a
    // buffer, then nearest-neighbor resample it to EXACTLY fill the box. Covers
    // are square and the box is square, so this shows the whole cover with no
    // crop and no margin.
    // Decode small (<=~104px) so the resample buffer is tiny (~20KB) and malloc
    // reliably succeeds -> every cover fills the box consistently (previously a
    // failed 45KB alloc silently fell back to the smaller fit-with-margin path).
    uint8_t scale = 1;
    while ((jw / scale > 104 || jh / scale > 104) && scale < 8) scale <<= 1;
    int sw = jw / scale, sh = jh / scale;

    bool prevSwap = t.getSwapBytes();
    g_decBuf = (uint16_t*)malloc((size_t)sw * sh * 2);
    if (g_decBuf) {
        g_decW = sw; g_decH = sh;
        for (int i = 0; i < sw * sh; ++i) g_decBuf[i] = 0;
        TJpgDec.setJpgScale(scale);
        TJpgDec.setCallback(bufOutput);
        TJpgDec.drawJpg(0, 0, g_buf, g_len);

        t.setSwapBytes(true);   // TJpg output is little-endian; pushImage needs swap
        uint16_t line[160];
        for (int oy = 0; oy < boxH && oy < 160; ++oy) {
            int syy = oy * sh / boxH;
            for (int ox = 0; ox < boxW && ox < 160; ++ox) {
                int sxx = ox * sw / boxW;
                line[ox] = g_decBuf[syy * sw + sxx];
            }
            t.pushImage(x, y + oy, boxW, 1, line);
        }
        t.setSwapBytes(prevSwap);
        free(g_decBuf);
        g_decBuf = nullptr;
        return true;
    }

    // Fallback (alloc failed): integer-scale to fit inside the box (full cover,
    // small margin), no crop.
    uint8_t fscale = 1;
    while ((jw / fscale > boxW || jh / fscale > boxH) && fscale < 8) fscale <<= 1;
    TJpgDec.setJpgScale(fscale);
    TJpgDec.setCallback(tftOutput);
    g_tft = &t;
    g_ox = x + (boxW - jw / fscale) / 2;
    g_oy = y + (boxH - jh / fscale) / 2;
    t.setSwapBytes(true);
    JRESULT r = TJpgDec.drawJpg(0, 0, g_buf, g_len);
    t.setSwapBytes(prevSwap);
    return r == JDR_OK;
}

}
