#include "art.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "../net/http_config.h"
#include "tjpgd.h"
#include "../util/artmap.h"
#include "../ui/theme.h"

namespace art {

static uint16_t* s_bmp = nullptr;
static uint8_t* s_work = nullptr;
static WiFiClient* s_in = nullptr;
static uint32_t s_deadline = 0;
static artmap::Map s_map;

bool begin() {
    if (!s_bmp) s_bmp = (uint16_t*)malloc(W * H * 2);
    if (!s_work) s_work = (uint8_t*)malloc(TJPGD_WORKSPACE_SIZE);
    if (s_bmp) memset(s_bmp, 0, W * H * 2);
    bool ok = s_bmp && s_work;
    if (!ok) Serial.println("[art] no heap for bitmap");
    return ok;
}

const uint16_t* bitmap() { return s_bmp; }

// tjpgd input: read (or skip, when buf == nullptr) up to len bytes from the HTTP stream.
static size_t jdIn(JDEC*, uint8_t* buf, size_t len) {
    size_t got = 0;
    uint8_t sink[64];
    while (got < len && (int32_t)(s_deadline - millis()) > 0) {
        int a = s_in->available();
        if (a <= 0) {
            if (!s_in->connected()) break;
            delay(1);
            continue;
        }
        size_t want = len - got;
        if ((size_t)a < want) want = (size_t)a;
        if (!buf && want > sizeof(sink)) want = sizeof(sink);
        int r = s_in->read(buf ? buf + got : sink, want);
        if (r <= 0) break;
        got += (size_t)r;
    }
    return got;
}

// tjpgd output: one decoded block (native RGB565); copy the pixels the cover map picks.
static int jdOut(JDEC*, void* block, JRECT* r) {
    const uint16_t* px = (const uint16_t*)block;
    int bw = r->right - r->left + 1;
    for (int oy = 0; oy < H; ++oy) {
        int sy = artmap::srcY(s_map, oy);
        if (sy < r->top || sy > r->bottom) continue;
        for (int ox = 0; ox < W; ++ox) {
            int sx = artmap::srcX(s_map, ox);
            if (sx < r->left || sx > r->right) continue;
            uint16_t c = px[(sy - r->top) * bw + (sx - r->left)];
            s_bmp[oy * W + ox] = theme::be(c);   // store big-endian
        }
    }
    return 1;
}

bool fetch(const char* url) {
    if (!s_bmp || !s_work || !url || !url[0]) return false;
    char plain[200];
    artmap::plainHttpUrl(url, plain, sizeof(plain));
    WiFiClient client;
    HTTPClient http;
    netcfg::streamed(http);
    if (!http.begin(client, plain)) return false;
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[art] http %d\n", code);
        http.end();
        return false;
    }
    s_in = http.getStreamPtr();
    s_deadline = millis() + 10000;
    // This TJpg_Decoder build adds a `swap` field that jd_prepare() preserves; left
    // uninitialised it was random stack garbage, so some covers came out byte-swapped twice
    // (scrambled/"inverted" colours). Native output here; jdOut does the one swap.
    JDEC jd = {};
    jd.swap = 0;
    JRESULT rc = jd_prepare(&jd, jdIn, s_work, TJPGD_WORKSPACE_SIZE, nullptr);
    if (rc == JDR_OK) {
        int s = artmap::pickScale(jd.width, jd.height, W);
        s_map = artmap::cover(jd.width >> s, jd.height >> s, W, H);
        rc = jd_decomp(&jd, jdOut, (uint8_t)s);
    }
    http.end();
    if (rc != JDR_OK) Serial.printf("[art] decode error %d\n", (int)rc);
    return rc == JDR_OK;
}

}
