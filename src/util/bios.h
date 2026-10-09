#pragma once
#include <cstdint>
// The boot's BIOS-style check screen: the script (which line shows when) and its text layout.
// PURE, host-tested; bootscene draws it. `t` is ms since the screen started.
namespace bios {
constexpr int COLS = 52;                 // characters per line (6 px font, 320 px wide)
constexpr uint32_t DOTS_MS = 180;        // a check's dot leader fills in over this time
constexpr uint32_t TOTAL_MS = 4900;      // the screen's length

enum Tone : uint8_t { HEADER, PLAIN, OK, WARN, PURPLE };
struct Line {
    uint16_t at;           // ms when the line appears
    const char* label;
    const char* value;     // right-aligned result after a dot leader; nullptr = plain text
    uint8_t tone;          // colour of the value (or of the whole line without one)
    bool live;             // the value comes from the device (WiFi), not the script
};

int count();
const Line& line(int i);
// `label`, a dot leader and `value` right-aligned to COLS, into out (cap >= COLS + 1). Returns
// the value's start column, or -1 for a line without a value (out = label).
int format(const char* label, const char* value, char* out, int cap, int cols = COLS);
// Characters of line i typed at t, out of `len` (its formatted length): the label at once, then
// the dots fill in over DOTS_MS and the value lands at the end.
int shown(int i, uint32_t t, int len);
// Same rule for any line that appeared `dt` ms ago with a label of labelLen characters.
int typed(uint32_t dt, int labelLen, int len);

// The night drive's on-board terminal (top left of the sky): the script above types in there,
// then the scene's own events; lines scroll up and expire.
constexpr int TERM_ROWS = 6, TERM_COLS = 32, TERM_VAL = 22;
struct TermLine {
    const char* label;          // static string; also the line's key
    char value[TERM_VAL];       // "" = plain text line
    uint8_t tone;
    uint32_t at;                // ms when it appeared (types in from there)
    uint32_t changed;           // ms of its last change (expiry counts from here)
    bool pending;               // waiting on something (WiFi): never expires
};
struct Term {
    TermLine rows[TERM_ROWS];
    int n;
    uint32_t gen;               // bumps on every change, so the drawer knows to redraw
    int fed;                    // script lines already fed in
};
// Sets the line keyed by `label`: updates its value in place (not retyped), or appends it at the
// bottom, scrolling the oldest off when full. Returns true when anything changed.
bool termSet(Term& term, const char* label, const char* value, uint8_t tone, uint32_t t,
             bool pending = false);
void termScript(Term& term, uint32_t t);           // feeds script lines due by t, each once (not the live one)
uint32_t liveAt();                                 // when the script's live (WI-FI) line is due
void termExpire(Term& term, uint32_t t, uint32_t ttl);   // drops lines unchanged for ttl (not pending)
}
