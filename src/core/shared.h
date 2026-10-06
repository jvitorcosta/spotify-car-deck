#pragma once
#include <stdint.h>
#include <vector>
#include "app_state.h"
#include "../util/lrc.h"

// State shared between the network task (core 0) and the UI loop (core 1), behind one
// FreeRTOS mutex. Why two tasks: with all HTTPS inline, every Spotify poll froze the
// display for ~1.5 s and a track change for ~19 s (README "Design & performance history").
namespace shared {
void begin();
void lock();
void unlock();
struct Guard { Guard() { lock(); } ~Guard() { unlock(); } };

// AppState: written by the network task after every poll, copied by the UI each frame.
void publish(const AppState& st);
void snapshot(AppState& out);

// Per-track media mailbox. Each result is tagged with the trackGen it was fetched for;
// post* frees it if the published track has already moved on (rapid skipping).
void postArt(uint32_t gen, uint8_t* jpeg, int len);                 // takes ownership
void postLyrics(uint32_t gen, std::vector<lrc::LrcLine>* lines);    // takes ownership
void postWalker(uint32_t gen);                                       // walker promoted
// UI side: take a result for `gen`; ownership moves to the caller. False if none.
bool takeArt(uint32_t gen, uint8_t** jpeg, int* len);
bool takeLyrics(uint32_t gen, std::vector<lrc::LrcLine>** lines);
bool takeWalker(uint32_t gen);
}
