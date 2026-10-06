#include "fetch.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace fetch {

bool httpsGet(const char* url, size_t maxLen, uint8_t** out, size_t* outLen, int* httpCode) {
    *out = nullptr;
    *outLen = 0;
    if (httpCode) *httpCode = 0;
    if (!url || !url[0]) return false;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    https.useHTTP10(true);   // plain (non-chunked) body so getSize() is the length
    if (!https.begin(client, url)) return false;
    int code = https.GET();
    if (httpCode) *httpCode = code;
    if (code != 200) { https.end(); return false; }
    int len = https.getSize();
    if (len <= 0 || (size_t)len > maxLen) { https.end(); return false; }

    uint8_t* data = (uint8_t*)malloc((size_t)len + 1);
    if (!data) { https.end(); return false; }
    WiFiClient* s = https.getStreamPtr();
    int got = 0;
    uint32_t last = millis();
    while (got < len && (https.connected() || s->available())) {
        size_t a = s->available();
        if (a) {
            size_t want = (size_t)(len - got);
            if (a < want) want = a;
            got += s->readBytes(data + got, want);
            last = millis();
        } else {
            if (millis() - last > 8000) break;   // stalled
            delay(1);
        }
    }
    https.end();
    if (got != len) { free(data); return false; }
    data[len] = 0;
    *out = data;
    *outLen = (size_t)len;
    return true;
}

}
