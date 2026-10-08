#pragma once
#include <cstddef>
#include <cstdint>
// Synced lyrics in one fixed arena (no per-line heap strings). PURE, host-tested.
// Replaced std::vector<std::string>, whose scattered allocations pinned fragments of
// byte-addressable RAM for the whole song (README "Design & performance history").
namespace lyricbuf {
constexpr int MAX_LINES = 192;   // ~5.6 KB arena in all: byte RAM is the scarce resource
constexpr int TEXT_CAP = 4096;
struct Line { uint32_t tMs; uint16_t off; };
// plainTotal: non-blank plain lines seen, including those that did not fit (spreadPlain).
struct Lyrics { int n; int used; int plainTotal; Line lines[MAX_LINES]; char text[TEXT_CAP]; };
// Parses "[mm:ss.xx]text" lines into `out` in time order; lines without a valid tag are
// skipped; once the arena is full the rest is dropped. Returns out.n.
int parse(const char* lrc, Lyrics& out);
// Incremental building (used by parse and by the streaming LRCLIB reader):
void reset(Lyrics& out);
// Adds one LRC line (without its newline); lines without a valid tag are skipped (true).
// False when the arena is full: the caller should stop.
bool addLine(Lyrics& out, const char* line, size_t len);
void finish(Lyrics& out);   // sorts by time
// True when `line` starts with a valid "[mm:ss.xx]" tag.
bool hasTag(const char* line, size_t len);
// Adds one untimed (plain) line, tMs 0. Blank lines (verse breaks) are skipped and not counted.
// Every other line increments out.plainTotal; false when it did not fit (arena full).
bool addPlain(Lyrics& out, const char* line, size_t len);
// Gives plain lines times spread evenly from 10% to 90% of durationMs, as if plainTotal lines
// were spread (stored lines keep their true share when the tail did not fit). No duration
// (0): 4 s apart from the start.
void spreadPlain(Lyrics& out, uint32_t durationMs);
int currentIndex(const Lyrics& l, uint32_t posMs);   // -1 before the first line
const char* lineText(const Lyrics& l, int i);         // "" if out of range
}
