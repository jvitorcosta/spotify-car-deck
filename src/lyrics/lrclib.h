#pragma once
#include "app_state.h"
#include "../util/lyricbuf.h"
#include "../util/lyricstatus.h"
namespace lyricsvc {
// The one lyrics arena (~5.6 KB, static). Written by the network task, read by the UI through
// shared::lyricView() — never directly.
lyricbuf::Lyrics& arena();
// Fetches lyrics for the current track from LRCLIB into `out`: /api/get (exact match), then
// /api/search on a 404. Synced beats plain (plain lines get spread times); instrumental wins.
// `attempt` (1-based) is only for the log line. Never throws; TempError means "retry later".
lyricstatus::Result fetchInto(const AppState& st, lyricbuf::Lyrics& out, int attempt);
}
