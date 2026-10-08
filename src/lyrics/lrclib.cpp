#include "lrclib.h"
#include "../util/lrcstream.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../net/http_config.h"
#include "../util/text.h"

namespace lyricsvc {

static lyricbuf::Lyrics s_arena;   // ~5.6 KB, static: allocated once
lyricbuf::Lyrics& arena() { return s_arena; }

// Query value via the shared, bounded encoder (one String instead of one per byte). AppState
// fields are < 96 bytes, so the encoded form (<= 3 bytes each) always fits.
static String urlEncode(const char* s) {
    char buf[300];
    txt::urlEncode(s, buf, sizeof(buf));
    return String(buf);
}

// One request into `out`. *rc: HTTP status (negative: HTTPClient connect/TLS/timeout error).
// *stalled: the body stopped arriving before the reader was done.
static lrcstream::Kind request(const String& url, lrcstream::Mode mode, uint32_t durationMs,
                               lyricbuf::Lyrics& out, int* rc, bool* stalled) {
    *stalled = false;
    lyricbuf::reset(out);
    WiFiClientSecure client;
    netcfg::secure(client);
    HTTPClient https;
    netcfg::streamed(https);   // plain (non-chunked) body: scan straight from the stream
    if (!https.begin(client, url)) { *rc = -1; return lrcstream::Kind::None; }
    https.addHeader("User-Agent", netcfg::USER_AGENT);
    *rc = https.GET();
    if (*rc != 200) { https.end(); return lrcstream::Kind::None; }
    // Stream straight into the arena (no JSON document: ArduinoJson grew the string by
    // doubling, up to 16 KB contiguous while TLS was open).
    lrcstream::Extractor ex(out, mode, durationMs);
    WiFiClient* s = https.getStreamPtr();
    uint8_t chunk[128];
    uint32_t last = millis();
    while (!ex.done() && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (millis() - last > netcfg::STALL_MS) { *stalled = true; break; }
            delay(1);
            continue;
        }
        int r = s->read(chunk, a < (int)sizeof(chunk) ? a : (int)sizeof(chunk));
        for (int i = 0; i < r && !ex.done(); ++i) ex.feed((char)chunk[i]);
        last = millis();
    }
    https.end();
    ex.finish();
    return ex.kind();
}

// 5xx and 429 are the server's problem; negative codes are connect/TLS/timeout failures.
static bool temporary(int rc) { return rc < 0 || rc == 429 || rc >= 500; }

lyricstatus::Result fetchInto(const AppState& st, lyricbuf::Lyrics& out, int attempt) {
    using lyricstatus::Result;
    lyricbuf::reset(out);
    if (!st.trackName[0]) return Result::NotFound;
    String q = "track_name=" + urlEncode(st.trackName) + "&artist_name=" + urlEncode(st.artist);
    int rc = 0;
    bool stalled = false;
    const char* via = "get";
    lrcstream::Kind k = request("https://lrclib.net/api/get?" + q + "&album_name=" + urlEncode(st.album) +
                                    "&duration=" + String(st.durationMs / 1000),
                                lrcstream::Mode::Get, st.durationMs, out, &rc, &stalled);
    if (rc == 404) {   // no exact match: fuzzy search, entry chosen by duration
        via = "search";
        k = request("https://lrclib.net/api/search?" + q, lrcstream::Mode::Search, st.durationMs,
                    out, &rc, &stalled);
    }
    Result r;
    if (temporary(rc) || stalled) r = Result::TempError;
    else if (rc != 200) r = Result::NotFound;
    else if (k == lrcstream::Kind::Synced) r = Result::Synced;
    else if (k == lrcstream::Kind::Plain) r = Result::Plain;
    else if (k == lrcstream::Kind::Instrumental) r = Result::Instrumental;
    else r = Result::NotFound;
    if (r == Result::Plain) lyricbuf::spreadPlain(out, st.durationMs);

    char what[40];
    switch (r) {
        case Result::Synced:       snprintf(what, sizeof(what), "synced %d lines", out.n); break;
        case Result::Plain:        snprintf(what, sizeof(what), "plain %d lines", out.n); break;
        case Result::Instrumental: snprintf(what, sizeof(what), "instrumental"); break;
        case Result::NotFound:     snprintf(what, sizeof(what), "not found"); break;
        case Result::TempError:
            if (stalled) snprintf(what, sizeof(what), "temp error (stalled)");
            else snprintf(what, sizeof(what), "temp error rc=%d", rc);
            break;
    }
    Serial.printf("[lyrics] \"%s\" / %s: %s via %s (try %d)\n", st.trackName, st.artist, what, via, attempt);
    return r;
}

}
