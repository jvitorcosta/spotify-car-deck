#include "lrclib.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cctype>

namespace lyricsvc {

static String urlEncode(const char* s) {
    String out;
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) out += (char)c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); out += b; }
    }
    return out;
}

Result fetch(const AppState& st) {
    Result r{Kind::None, ""};
    if (!st.trackName[0]) return r;

    String url = "https://lrclib.net/api/get?track_name=" + urlEncode(st.trackName) +
                 "&artist_name=" + urlEncode(st.artist) +
                 "&album_name=" + urlEncode(st.album) +
                 "&duration=" + String(st.durationMs / 1000);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) return r;
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    int rc = https.GET();
    if (rc != 200) { Serial.printf("[lyrics] GET rc=%d\n", rc); https.end(); return r; }  // 404 = no match

    String body = https.getString();   // de-chunk before parsing
    https.end();
    JsonDocument filter;
    filter["syncedLyrics"] = true;
    filter["plainLyrics"] = true;
    JsonDocument doc;
    deserializeJson(doc, body, DeserializationOption::Filter(filter));

    const char* synced = doc["syncedLyrics"] | "";
    const char* plain  = doc["plainLyrics"]  | "";
    if (synced && synced[0]) { r.kind = Kind::Synced; r.text = synced; }
    else if (plain && plain[0]) { r.kind = Kind::Plain; r.text = plain; }
    return r;
}

}
