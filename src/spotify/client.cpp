#include "client.h"
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include "../config.h"
#include "../spotify/auth.h"

namespace spclient {
static WiFiClientSecure client;
static SpotifyArduino* sp = nullptr;
static AppState* target = nullptr;
static bool trackChanged = false;

static void copyStr(char* dst, const char* src, size_t n) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1); dst[n - 1] = '\0';
}

static void onPlaying(CurrentlyPlaying cp) {
    AppState& st = *target;
    if (strncmp(st.trackName, cp.trackName, sizeof(st.trackName)) != 0) trackChanged = true;
    copyStr(st.trackName, cp.trackName, sizeof(st.trackName));
    copyStr(st.artist, cp.numArtists > 0 ? cp.artists[0].artistName : nullptr, sizeof(st.artist));
    copyStr(st.album, cp.albumName, sizeof(st.album));
    // choose the ~300px image (index 1 is usually 300px; fall back to 0)
    const char* art = cp.numImages > 1 ? cp.albumImages[1].url : (cp.numImages > 0 ? cp.albumImages[0].url : nullptr);
    copyStr(st.albumArtUrl, art, sizeof(st.albumArtUrl));
    copyStr(st.context, cp.contextUri, sizeof(st.context)); // refined to name in Task 14-opt
    st.progressMs = cp.progressMs;
    st.durationMs = cp.durationMs;
    st.isPlaying  = cp.isPlaying;
    st.lastPollMs = millis();
    st.status = cp.isPlaying ? PlaybackStatus::Playing : PlaybackStatus::Paused;
}

void begin() {
    client.setInsecure();
    String rt = spauth::loadRefreshToken();
    sp = new SpotifyArduino(client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, rt.c_str());
    if (sp->refreshAccessToken()) Serial.println("[spotify] access token OK");
    else Serial.println("[spotify] refreshAccessToken FAILED");
}

bool poll(AppState& st) {
    if (!sp) return false;
    target = &st; trackChanged = false;
    int code = sp->getCurrentlyPlaying(onPlaying, SPOTIFY_MARKET);
    if (code == 200) {
        return trackChanged;
    } else if (code == 204) {
        st.status = PlaybackStatus::Stopped;
    } else {
        Serial.printf("[spotify] poll HTTP %d\n", code);
        st.status = PlaybackStatus::Offline;
    }
    return false;
}

void togglePlay(bool currentlyPlaying) {
    if (!sp) return;
    if (currentlyPlaying) sp->pause(); else sp->play();
}
void next() { if (sp) sp->nextTrack(); }
void prev() { if (sp) sp->previousTrack(); }
void setVolume(int pct) {
    if (!sp) return;
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    sp->setVolume(pct);
}
}
