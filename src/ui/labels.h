#pragma once
// User-facing labels. PURE, host-tested.
namespace ui {
// Top-strip title: matches the CD, which spins while playing and freezes while paused.
inline const char* topTitle(bool isPlaying) { return isPlaying ? "NOW PLAYING" : "PAUSED"; }
}
