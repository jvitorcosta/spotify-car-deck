#include "fetch.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace fetch {

bool httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen, int* httpCode,
                  bool allowPartial) {
    *outLen = 0;
    if (httpCode) *httpCode = 0;
    if (!url || !url[0] || !buf || cap < 2) return false;
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);   // seconds; default 120 s blocks a half-dead link
    HTTPClient https;
    https.useHTTP10(true);           // plain (non-chunked) body so getSize() is the length
    https.setTimeout(8000);
    if (!https.begin(client, url)) return false;
    int code = https.GET();
    if (httpCode) *httpCode = code;
    if (code != 200) {
        Serial.printf("[fetch] http %d\n", code);
        https.end();
        return false;
    }
    int len = https.getSize();
    if (len > 0 && (size_t)len + 1 > cap && allowPartial) {
        len = (int)cap - 1;              // caller only needs the beginning (e.g. AnimData.xml)
    }
    if (len <= 0 || (size_t)len + 1 > cap) {
        Serial.printf("[fetch] bad length %d (cap %u)\n", len, (unsigned)cap);
        https.end();
        return false;
    }
    WiFiClient* s = https.getStreamPtr();
    int got = 0;
    uint32_t last = millis();
    while (got < len && (https.connected() || s->available())) {
        size_t a = s->available();
        if (a) {
            size_t want = (size_t)(len - got);
            if (a < want) want = a;
            got += s->readBytes(buf + got, want);
            last = millis();
        } else {
            if (millis() - last > 8000) break;   // stalled
            delay(1);
        }
    }
    https.end();
    if (got != len) return false;
    buf[len] = 0;
    *outLen = (size_t)len;
    return true;
}

}
