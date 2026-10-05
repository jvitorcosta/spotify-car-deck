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

// PNGdec draw callback: decodes one scanline at a time, compositing fully
// transparent pixels against the cream panel background (no alpha mask
// bookkeeping needed), then blits the line straight to the display.
static int pngDraw(PNGDRAW* pDraw) {
    uint16_t lineBuf[256];
    png.getLineAsRGB565(pDraw, lineBuf, PNG_RGB565_BIG_ENDIAN, theme::GBA_CREAM);
    g_tft->pushImage(g_x0, g_y0 + pDraw->y, pDraw->iWidth, 1, lineBuf);
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
    g_x0 = cx - png.getWidth() / 2;
    g_y0 = cy - png.getHeight() / 2;
    png.decode(nullptr, 0);
    png.close();
    free(data);
    return true;
}

}
