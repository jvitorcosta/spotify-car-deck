#pragma once
#include <cstddef>
#include <cstdint>
#include "../util/lyricstatus.h"
// Dialogue-box text for the lyrics status: Pokemon battle-text style messages while searching
// or retrying, in the intro, and when there are no lyrics, so the box is never empty.
// Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md. PURE, host-tested.
namespace lyricmsg {
constexpr uint32_t FAIL_SHOW_MS = 3000;   // FAIL / INSTRUMENTAL line, then the IDLE line
// FOUND is a brief confirmation when lyrics arrive in the intro; then the box shows the first
// (upcoming) line. After a skip the lyrics should take over at once (user feedback 2026-10-08).
constexpr uint32_t FOUND_SHOW_MS = 1500;
constexpr size_t TEXT_CAP = 160;
struct In {
    lyricstatus::Status status;
    uint32_t statusAgeMs;   // since the UI first saw this status for the track
    uint32_t posMs;         // track position
    uint32_t firstLineMs;   // time of the first lyric line (Synced / Plain)
    const char* line;       // current lyric line, or the first one in the intro; may be ""
    const char* pokeName;   // as stored; written in capitals
    uint32_t seed;          // track generation: one stable pick per song
};
struct Out {
    char text[TEXT_CAP];
    bool notes;             // draw the note icons (false for approximate plain timing)
    bool dance;             // a real timed lyric line is shown: the note icons may bob
};
void compose(const In& in, Out& out);
// Copies tmpl into out (n bytes incl. NUL), replacing each "{P}" with the name in capitals
// ("POKéMON" if empty). Never cuts a UTF-8 character.
void fill(const char* tmpl, const char* name, char* out, size_t n);

constexpr int SEARCH_N = 10, RETRY_N = 3, FAIL_N = 4, INSTRUMENTAL_N = 3, FOUND_N = 2;
extern const char* const SEARCH[SEARCH_N];
extern const char* const RETRY[RETRY_N];
extern const char* const FAIL[FAIL_N];
extern const char* const INSTRUMENTAL[INSTRUMENTAL_N];
extern const char* const FOUND[FOUND_N];
extern const char* const IDLE;
}
