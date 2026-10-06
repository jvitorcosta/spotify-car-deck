#pragma once
// The network task (core 0): Spotify polls, per-track downloads, walker loading.
// It never draws; results reach the UI through core/shared.
namespace nettask {
// Starts the task. spotifyReady = setup() already connected WiFi and ran spclient::begin();
// otherwise the task connects WiFi itself (hotspot that comes up after the deck) first.
void start(bool spotifyReady);
}
