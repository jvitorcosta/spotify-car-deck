#pragma once
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
// Shared HTTP(S) settings for every request the deck makes (Spotify, LRCLIB, Apple, art,
// walk sheets). Header-only and inline: no extra RAM or stack at the call sites.
namespace netcfg {
// TLS handshake limit. The 120 s default froze polls on a half-dead hotspot.
constexpr unsigned long TLS_HANDSHAKE_S = 8;
// HTTPClient connect/read timeout (ms).
constexpr uint16_t HTTP_TIMEOUT_MS = 8000;
// Our own body-reading loops give up when no byte has arrived for this long (ms).
constexpr uint32_t STALL_MS = 8000;
constexpr const char* USER_AGENT = "PokeDeck/1.0 (ESP32)";

// No certificate validation (public data; see docs/review), bounded handshake.
inline void secure(WiFiClientSecure& c) {
    c.setInsecure();
    c.setHandshakeTimeout(TLS_HANDSHAKE_S);
}
// Plain (non-chunked, HTTP/1.0) body read straight from the stream, bounded waits.
inline void streamed(HTTPClient& h) {
    h.useHTTP10(true);
    h.setTimeout(HTTP_TIMEOUT_MS);
}
}
