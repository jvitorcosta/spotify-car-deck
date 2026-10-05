#pragma once
#include <string>
#include "app_state.h"
namespace lyricsvc {
enum class Kind { None, Synced, Plain };
struct Result { Kind kind; std::string text; };
// Fetch lyrics from LRCLIB for the current track (uses original accented names).
Result fetch(const AppState& st);
}
