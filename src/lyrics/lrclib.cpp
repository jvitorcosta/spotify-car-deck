#include "lrclib.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
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
    out.n = 0;
    if (!st.trackName[0]) return false;
    String url = "https://lrclib.net/api/get?track_name=" + urlEncode(st.trackName) +
                 "&artist_name=" + urlEncode(st.artist) +
                 "&album_name=" + urlEncode(st.album) +
                 "&duration=" + String(st.durationMs / 1000);

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient https;
    https.useHTTP10(true);          // plain body: parse straight from the stream
    https.setTimeout(8000);
    if (!https.begin(client, url)) return false;
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    int rc = https.GET();
    if (rc != 200) {                 // 404 = no match
        Serial.printf("[lyrics] GET rc=%d\n", rc);
        https.end();
        return false;
    }
    JsonDocument filter;
    filter["syncedLyrics"] = true;   // plain lyrics can't be timed; don't even buffer them
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getStream(),
                                               DeserializationOption::Filter(filter));
    https.end();
    if (err) { Serial.printf("[lyrics] json %s\n", err.c_str()); return false; }
    int n = lyricbuf::parse(doc["syncedLyrics"] | "", out);
    Serial.printf("[lyrics] synced lines=%d\n", n);
    return n > 0;
}

}
