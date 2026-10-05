#pragma once
// Copy to src/config.h and fill in. src/config.h is git-ignored.

// Up to 3 WiFi networks tried in order (home for dev, phone hotspot for car).
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_NETWORKS[] = {
    {"YOUR_HOME_SSID", "YOUR_HOME_PASSWORD"},
    {"YOUR_IPHONE_HOTSPOT", "YOUR_HOTSPOT_PASSWORD"},
};
static const int WIFI_NETWORK_COUNT = 2;

// Spotify developer app (https://developer.spotify.com/dashboard)
#define SPOTIFY_CLIENT_ID     "your_client_id"
#define SPOTIFY_CLIENT_SECRET "your_client_secret"
// Spotify requires http only for the loopback address. Register this EXACT URI in
// your app dashboard (Settings -> Redirect URIs). Used by the PC auth helper.
#define SPOTIFY_REDIRECT_URI  "http://127.0.0.1:8888/callback"
// One-time refresh token, obtained on the PC via .devtools/spotify_auth.py.
#define SPOTIFY_REFRESH_TOKEN ""
#define SPOTIFY_MARKET        "BR"
