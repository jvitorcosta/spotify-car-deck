#pragma once
#include "app_state.h"
#include "../util/lyricbuf.h"
namespace lyricsvc {
// The one lyrics arena (~5.6 KB, static). Written by the network task, read by the UI through
// shared::lyricLine() — never directly.
lyricbuf::Lyrics& arena();
// Fetches synced lyrics from LRCLIB for the current track (original accented names) and
// parses them into `out`. True if at least one synced line was found.
bool fetchInto(const AppState& st, lyricbuf::Lyrics& out);
}
