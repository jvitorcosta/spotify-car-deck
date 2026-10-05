#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
#include "widgets.h"
namespace ui {
// Control button layout (shared with input hit-testing in Task 13).
struct NowButtons { Button prev, play, next, vol, lyrics; };
NowButtons nowButtons();
void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent);
// Repaints just the HP/time panel. Called by drawNow and by the live redraw
// between polls so the two never drift apart.
void drawProgressRegion(TFT_eSPI& t, const AppState& st);
// Draws the current synced lyric line in the area under the HP bar (empty if
// there is no synced line). Called each redraw with the line for the moment.
void drawLyricArea(TFT_eSPI& t, const char* currentLine);
// Full-screen in-theme status message (e.g. "No signal...", "Nothing playing").
void drawOffline(TFT_eSPI& t, const char* msg);
}
