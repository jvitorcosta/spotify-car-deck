#include "jpeg.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>

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

bool cacheAlbumArt(const char* url) {
    if (g_buf) {
        free(g_buf);
        g_buf = nullptr;
        g_len = 0;
    }
    if (!url || !url[0]) return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) return false;

    int rc = https.GET();
    if (rc != 200) {
        https.end();
        return false;
    }

    int len = https.getSize();
    if (len <= 0 || len > 60000) { // guard RAM; no PSRAM on this board
        https.end();
        return false;
    }

    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) {
        https.end();
        return false;
    }

    WiFiClient* stream = https.getStreamPtr();
    int got = 0;
    uint32_t start = millis();
    while (https.connected() && got < len) {
        size_t avail = stream->available();
        if (avail) {
            size_t want = (size_t)(len - got);
            if (avail < want) want = avail;
            got += stream->readBytes(buf + got, want);
        } else {
            if (millis() - start > 8000) break; // stalled download, bail out
            delay(1);
        }
    }
    https.end();

    if (got != len) {
        free(buf);
        return false;
    }

    g_buf = buf;
    g_len = len;
    return true;
}

bool drawAlbumArt(TFT_eSPI& t, int x, int y, int boxW, int boxH) {
    if (!g_buf || g_len <= 0) return false;

    uint16_t jw = 0, jh = 0;
    TJpgDec.getJpgSize(&jw, &jh, g_buf, g_len);
    if (jw == 0 || jh == 0) return false;

    uint8_t scale = 1;
    while ((jw / scale > boxW || jh / scale > boxH) && scale < 8) scale <<= 1;
    TJpgDec.setJpgScale(scale);
    TJpgDec.setCallback(tftOutput);

    g_tft = &t;
    g_ox = x + (boxW - jw / scale) / 2;
    g_oy = y + (boxH - jh / scale) / 2;

    JRESULT r = TJpgDec.drawJpg(0, 0, g_buf, g_len);
    return r == JDR_OK;
}

}
