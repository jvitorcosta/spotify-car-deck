#include "auth.h"
#include <Preferences.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../net/wifi.h"

namespace spauth {
static Preferences prefs;

String loadRefreshToken() {
    prefs.begin("spotify", true);
    String t = prefs.getString("rtoken", "");
    prefs.end();
    return t;
}
void saveRefreshToken(const String& token) {
    prefs.begin("spotify", false);
    prefs.putString("rtoken", token);
    prefs.end();
}

static String authorizeUrl() {
    String redirect = "http://" + net::deviceIp() + "/callback";
    String scope = "user-read-playback-state%20user-modify-playback-state%20user-read-currently-playing";
    return String("https://accounts.spotify.com/authorize?response_type=code&client_id=")
        + SPOTIFY_CLIENT_ID + "&scope=" + scope + "&redirect_uri=" + redirect;
}

// Exchange authorization code for tokens; store refresh token. Returns true on success.
static bool exchangeCode(const String& code) {
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    String redirect = "http://" + net::deviceIp() + "/callback";
    https.begin(client, "https://accounts.spotify.com/api/token");
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=authorization_code&code=" + code +
                  "&redirect_uri=" + redirect +
                  "&client_id=" + String(SPOTIFY_CLIENT_ID) +
                  "&client_secret=" + String(SPOTIFY_CLIENT_SECRET);
    int rc = https.POST(body);
    if (rc != 200) { Serial.printf("[auth] token exchange HTTP %d\n", rc); https.end(); return false; }
    JsonDocument doc;
    deserializeJson(doc, https.getString());
    https.end();
    String rt = doc["refresh_token"] | "";
    if (rt.isEmpty()) { Serial.println("[auth] no refresh_token in response"); return false; }
    saveRefreshToken(rt);
    Serial.println("[auth] refresh token stored in NVS");
    return true;
}

bool runSetupPortalIfNeeded() {
    if (!loadRefreshToken().isEmpty()) return true;
    // A PC-obtained refresh token in config.h (Spotify's loopback-only redirect rule)
    // makes the on-device portal unnecessary — skip it.
    if (strlen(SPOTIFY_REFRESH_TOKEN) > 0) return true;

    WebServer server(80);
    bool done = false;
    server.on("/", [&]() {
        String html = "<h2>PokeDeck setup</h2><p>Register this redirect URI in your "
            "Spotify app dashboard first:</p><code>http://" + net::deviceIp() +
            "/callback</code><p><a href='" + authorizeUrl() +
            "'>Log in with Spotify</a></p>";
        server.send(200, "text/html", html);
    });
    server.on("/callback", [&]() {
        if (!server.hasArg("code")) { server.send(400, "text/plain", "missing code"); return; }
        if (exchangeCode(server.arg("code"))) {
            server.send(200, "text/html", "<h2>Done! You can unplug + reboot.</h2>");
            done = true;
        } else {
            server.send(500, "text/plain", "token exchange failed");
        }
    });
    server.begin();
    Serial.printf("[auth] SETUP NEEDED -> open http://%s/ in a browser on the same network\n",
                  net::deviceIp().c_str());
    while (!done) { server.handleClient(); delay(5); }
    server.stop();
    return true;
}
}
