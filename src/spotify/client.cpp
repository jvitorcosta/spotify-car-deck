#include "client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../net/http_config.h"
#include <ArduinoJson.h>
#include <SpotifyArduino.h>
#include "../config.h"
#include "../spotify/auth.h"
#include "../util/ctxcache.h"
#include "../util/text.h"

namespace spclient {
static WiFiClientSecure s_client;
static SpotifyArduino* s_sp = nullptr;
static AppState* s_target = nullptr;

static void copyStr(char* dst, const char* src, size_t n) {
    if (!src) { dst[0] = '\0'; return; }
    txt::copy(dst, src, n);
}

static void onPlaying(CurrentlyPlaying cp) {
    AppState& st = *s_target;
    // Ads (free accounts) and unknown items: SpotifyArduino leaves trackName, trackUri,
    // artists, albumName and the images uninitialised (it only fills them for track/episode),
    // so none of them may be read. Show the "Nothing playing" screen until music resumes; the
    // last song's fields stay as they were, so no new track is detected for the ad.
    if (cp.currentlyPlayingType != track && cp.currentlyPlayingType != episode) {
        st.isPlaying = cp.isPlaying;
        st.lastPollMs = millis();
        st.status = PlaybackStatus::Stopped;
        return;
    }
    // Store ORIGINAL (UTF-8) names so LRCLIB lyric matching works for accented
    // titles; accents are folded to ASCII at display time instead.
    copyStr(st.trackName, cp.trackName, sizeof(st.trackName));
    txt::copyId(cp.trackUri, st.trackUri, sizeof(st.trackUri));   // long local-file URIs stay distinct
    copyStr(st.artist, cp.numArtists > 0 ? cp.artists[0].artistName : nullptr, sizeof(st.artist));
    copyStr(st.album, cp.albumName, sizeof(st.album));
    // choose the ~300px image (index 1 is usually 300px; fall back to 0)
    const char* art = cp.numImages > 1 ? cp.albumImages[1].url : (cp.numImages > 0 ? cp.albumImages[0].url : nullptr);
    copyStr(st.albumArtUrl, art, sizeof(st.albumArtUrl));
    copyStr(st.context, cp.contextUri, sizeof(st.context)); // raw URI; poll() resolves the name
    st.progressMs = cp.progressMs;
    st.durationMs = cp.durationMs;
    st.isPlaying  = cp.isPlaying;
    st.lastPollMs = millis();
    st.status = cp.isPlaying ? PlaybackStatus::Playing : PlaybackStatus::Paused;
}

void begin() {
    netcfg::secure(s_client);
    s_client.setTimeout(8);            // seconds: TCP connect/read (default 30 s)
    String rt = spauth::loadRefreshToken();
    if (rt.isEmpty()) rt = SPOTIFY_REFRESH_TOKEN;   // PC-obtained token from config.h
    // NOTE: SpotifyArduino's ctor calls setRefreshToken(), which reads its _refreshToken
    // member before initializing it. Heap `new` leaves that member as garbage (not NULL),
    // so its `strlen(_refreshToken)` dereferences garbage and crashes. A function-local
    // static has zero-initialized storage, so _refreshToken starts as NULL -> safe.
    static SpotifyArduino instance(s_client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, rt.c_str());
    s_sp = &instance;
    if (s_sp->refreshAccessToken()) Serial.println("[spotify] access token OK");
    else Serial.println("[spotify] refreshAccessToken FAILED");
}

// --- Context (playlist/album/artist) name resolution ------------------------
// getCurrentlyPlaying only returns the context URI; the Web API needs a separate
// authorized call for the playlist name. We keep our own access token (refreshed
// from the refresh token) for these lookups and cache the last resolved name.
// "Bearer xxx" (~250 chars today). A fixed buffer, not a heap String: the String's block
// (~270 B, kept for the whole session) landed between two ~17.7 KB free blocks and capped the
// largest free block at ~17-22 KB instead of ~35 KB, below TLS_NEED (heap dump, 2026-10-08).
static char s_accessToken[512] = "";
static uint32_t s_tokenExpiry = 0;

static bool ensureAccessToken() {
    if (s_accessToken[0] && (int32_t)(s_tokenExpiry - millis()) > 0) return true;
    String rt = spauth::loadRefreshToken();
    if (rt.isEmpty()) rt = SPOTIFY_REFRESH_TOKEN;
    if (rt.isEmpty()) return false;
    WiFiClientSecure c;
    netcfg::secure(c);
    HTTPClient https;
    https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
    if (!https.begin(c, "https://accounts.spotify.com/api/token")) return false;
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=refresh_token&refresh_token=" + rt +
                  "&client_id=" + SPOTIFY_CLIENT_ID +
                  "&client_secret=" + SPOTIFY_CLIENT_SECRET;
    int rc = https.POST(body);
    if (rc != 200) { https.end(); return false; }
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, https.getString());
    https.end();
    if (e || !doc["access_token"].is<const char*>()) return false;
    if (!txt::concat(s_accessToken, sizeof(s_accessToken), "Bearer ", doc["access_token"].as<const char*>())) {
        Serial.println("[spotify] access token too long for its buffer");
        return false;
    }
    s_tokenExpiry = millis() + 50UL * 60 * 1000;   // tokens last ~1h
    return true;
}

// uri like "spotify:playlist:ID" / ":album:" / ":artist:". Returns false when the lookup
// failed for a reason worth retrying (no token, network, 5xx/429); `out` then holds the type
// ("playlist") as a placeholder. 403/404 (private or deleted playlist) are final.
static bool resolveContext(const char* uri, char* out, size_t n) {
    out[0] = '\0';
    if (!uri || !uri[0]) { txt::copy(out, "-", n); return true; }
    String u = uri;
    int p1 = u.indexOf(':'), p2 = u.indexOf(':', p1 + 1);
    if (p1 < 0 || p2 < 0) { txt::copy(out, "-", n); return true; }
    String type = u.substring(p1 + 1, p2), id = u.substring(p2 + 1);

    const char* endpoint = nullptr;
    if (type == "playlist") endpoint = "playlists";
    else if (type == "album") endpoint = "albums";
    else if (type == "artist") endpoint = "artists";
    txt::copy(out, type.c_str(), n);   // placeholder until resolved
    if (!endpoint) return true;                            // e.g. "collection": nothing to look up
    if (!ensureAccessToken()) return false;
    WiFiClientSecure c;
    netcfg::secure(c);
    HTTPClient https;
    https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
    String url = "https://api.spotify.com/v1/" + String(endpoint) + "/" + id + "?fields=name";
    if (!https.begin(c, url)) return false;
    https.addHeader("Authorization", s_accessToken);
    int rc = https.GET();
    // 401: the cached access token was revoked or expired early. Expire it now so the next
    // lookup refreshes it, instead of failing until the 50-minute expiry.
    if (rc == 401) s_tokenExpiry = millis();
    bool ok = rc == 403 || rc == 404;
    if (rc == 200) {
        JsonDocument d;
        if (!deserializeJson(d, https.getString()) && d["name"].is<const char*>()) {
            txt::copy(out, d["name"], n);          // raw; folded at display time
            ok = true;
        }
    }
    https.end();
    return ok;
}

void poll(AppState& st) {
    if (!s_sp) return;
    s_target = &st;
    int code = s_sp->getCurrentlyPlaying(onPlaying, SPOTIFY_MARKET);
    if (code == 200) {
        // st.context holds the raw context URI (set in onPlaying). Resolve it to
        // a human name once per context change (cached), done here — never inside
        // the getCurrentlyPlaying callback (no nested HTTPS during its parse).
        // A failed lookup is retried after ctxcache::RETRY_MS (it used to stick until the
        // context changed, leaving "playlist" in the header).
        static ctxcache::Cache cache;
        static char cachedName[64] = "";
        if (cache.needsLookup(st.context, millis())) {
            bool ok = resolveContext(st.context, cachedName, sizeof(cachedName));
            cache.store(st.context, ok, millis());
            Serial.printf("[spotify] context: %s%s\n", cachedName, ok ? "" : " (lookup failed, will retry)");
        }
        txt::copy(st.context, cachedName, sizeof(st.context));
    } else if (code == 204) {
        st.status = PlaybackStatus::Stopped;
    } else {
        Serial.printf("[spotify] poll HTTP %d\n", code);
        st.status = PlaybackStatus::Offline;
    }
}

static void onPlayer(PlayerDetails pd) {
    AppState& st = *s_target;
    copyStr(st.deviceName, pd.device.name, sizeof(st.deviceName));
    copyStr(st.deviceType, pd.device.type, sizeof(st.deviceType));
    st.volume  = pd.device.volumePercent;
    st.shuffle = pd.shuffleState;
    // Library enum is repeat_track=0, repeat_context=1, repeat_off=2; ours makes the
    // zero-initialised state mean "off" so nothing lights up before the first poll.
    st.repeat  = pd.repeateState == repeat_track   ? 2
               : pd.repeateState == repeat_context ? 1 : 0;
}

void pollPlayerDetails(AppState& st) {
    if (!s_sp) return;
    s_target = &st;
    s_sp->getPlayerDetails(onPlayer, SPOTIFY_MARKET);
}
}
