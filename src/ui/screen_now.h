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
}
