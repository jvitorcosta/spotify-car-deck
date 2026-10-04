#pragma once
#include <Arduino.h>
#include "app_state.h"
namespace spclient {
void begin();
bool poll(AppState& st);   // returns true when the track changed
void togglePlay(bool currentlyPlaying);
void next();
void prev();
void setVolume(int pct);   // 0..100
}
