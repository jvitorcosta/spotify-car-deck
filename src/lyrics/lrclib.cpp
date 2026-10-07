#include "lrclib.h"
#include "../util/lrcstream.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cctype>

namespace lyricsvc {

static lyricbuf::Lyrics g_arena;   // ~5.6 KB, static: allocated once
lyricbuf::Lyrics& arena() { return g_arena; }

static String urlEncode(const char* s) {
    String out;
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) out += (char)c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); out += b; }
    }
    return out;
}

bool fetchInto(const AppState& st, lyricbuf::Lyrics& out) {
    lyricbuf::reset(out);
    if (!st.trackName[0]) return false;
    String url = "https://lrclib.net/api/get?track_name=" + urlEncode(st.trackName) +
                 "&artist_name=" + urlEncode(st.artist) +
                 "&album_name=" + urlEncode(st.album) +
                 "&duration=" + String(st.durationMs / 1000);

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient https;
    https.useHTTP10(true);          // plain (non-chunked) body: scan straight from the stream
    https.setTimeout(8000);
    if (!https.begin(client, url)) return false;
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    int rc = https.GET();
    if (rc != 200) {                 // 404 = no match
        Serial.printf("[lyrics] GET rc=%d\n", rc);
        https.end();
        return false;
    }
    // Stream the syncedLyrics string straight into the arena (no JSON document: ArduinoJson
    // grew it by doubling, up to 16 KB contiguous while TLS was open). Plain lyrics are skipped.
    lrcstream::Extractor ex(out);
    WiFiClient* s = https.getStreamPtr();
    uint8_t chunk[128];
    uint32_t last = millis();
    while (!ex.done() && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (millis() - last > 8000) break;   // stalled
            delay(1);
            continue;
        }
        int r = s->read(chunk, a < (int)sizeof(chunk) ? a : (int)sizeof(chunk));
        for (int i = 0; i < r && !ex.done(); ++i) ex.feed((char)chunk[i]);
        last = millis();
    }
    https.end();
    int n = ex.finish();
    Serial.printf("[lyrics] synced lines=%d%s%s\n", n, ex.found() ? "" : " (none)",
                  ex.truncated() ? " (arena full: tail dropped)" : "");
    return n > 0;
}

}
