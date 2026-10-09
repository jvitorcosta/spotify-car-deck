#pragma once
#include <TFT_eSPI.h>
// Boot scene: a silver Honda City drives along a night road while the greeting plays and WiFi
// connects (docs/superpowers/specs/2026-10-09-boot-scene-design.md). Runs in its own task; while
// active() it is the only code drawing on the display.
namespace bootscene {
// Starts the scene task. `online` is polled each frame: the car leaves once it has been shown
// for 5 s, the greeting has ended and online() is true.
void start(TFT_eSPI& tft, bool (*online)());
// Static string. A BIOS-style log types into an on-board terminal in the sky while the car
// drives; a caption containing "retry" turns its WI-FI line to RETRYING... (the caption text
// itself only shows when the scene couldn't get its strip buffer).
void setCaption(const char* text);
bool active();                       // true until the car has left and the task has ended
void waitDone();                     // blocks until !active()
}
