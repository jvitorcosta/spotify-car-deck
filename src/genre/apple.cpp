#include "apple.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../util/genre.h"
#include "../util/text.h"

namespace applegenre {

static char g_body[1024];   // static: off the 10 KB network-task stack; answers are ~300 B

Result lookup(const char* artist, uint32_t* genreId, int* rc) {
    *rc = 0;
    if (!artist || !artist[0]) return Result::None;
    char term[300];
    txt::urlEncode(artist, term, sizeof(term));
    char url[400];
    snprintf(url, sizeof(url),
             "https://itunes.apple.com/search?term=%s&entity=musicArtist&limit=1&country=BR", term);
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient https;
    https.useHTTP10(true);
    https.setTimeout(8000);
    if (!https.begin(client, url)) { *rc = -1; return Result::Error; }
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    *rc = https.GET();
    if (*rc != 200) { https.end(); return Result::Error; }
    WiFiClient* s = https.getStreamPtr();
    size_t len = 0;
    uint32_t last = millis();
    while (len < sizeof(g_body) - 1 && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (millis() - last > 8000) break;   // stalled: parse what arrived
            delay(1);
            continue;
        }
        size_t want = sizeof(g_body) - 1 - len;
        if ((size_t)a < want) want = (size_t)a;
        len += s->readBytes(g_body + len, want);
        last = millis();
    }
    https.end();
    g_body[len] = '\0';
    switch (genre::readAnswer(g_body, genreId)) {
        case genre::Answer::Found:   return Result::Found;
        case genre::Answer::NoMatch: return Result::None;   // complete answer: Apple has no genre
        default:                     *rc = -2; return Result::Error;   // cut off: not cached
    }
}

}
