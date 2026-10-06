#pragma once
#include <stdint.h>
#include "app_state.h"

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

// Album art lives in art::bitmap(). The network task calls artInvalidate() before writing it
// and postArt(gen) after; the UI pushes it only while it is valid for the track on screen.
void artInvalidate();
void postArt(uint32_t gen);
bool takeArt(uint32_t gen);            // true once per arrival (UI)
bool artValidLocked(uint32_t gen);     // caller holds the lock (UI redraw + push)

// Lyrics live in lyricsvc::arena(). The network task calls lyricsInvalidate() before parsing
// into it and postLyrics(gen) after; the UI copies the current line under the lock.
void lyricsInvalidate();
void postLyrics(uint32_t gen);
// Copies the synced line for posMs into out ("" if none / not valid for gen).
void lyricLine(uint32_t gen, uint32_t posMs, char* out, size_t len);

// Walker: posted once the new walker is promoted for the track generation.
void postWalker(uint32_t gen);
bool takeWalker(uint32_t gen);
}
