#pragma once
#include <cstdint>
// Synced lyrics in one fixed arena (no per-line heap strings). PURE, host-tested.
// Replaced std::vector<std::string>, whose scattered allocations pinned fragments of
// byte-addressable RAM for the whole song (README "Design & performance history").
namespace lyricbuf {
constexpr int MAX_LINES = 192;   // ~5.6 KB arena in all: byte RAM is the scarce resource
constexpr int TEXT_CAP = 4096;
struct Line { uint32_t tMs; uint16_t off; };
struct Lyrics { int n; Line lines[MAX_LINES]; char text[TEXT_CAP]; };
// Parses "[mm:ss.xx]text" lines into `out` in time order; lines without a valid tag are
// skipped; once the arena is full the rest is dropped. Returns out.n.
int parse(const char* lrc, Lyrics& out);
int currentIndex(const Lyrics& l, uint32_t posMs);   // -1 before the first line
const char* lineText(const Lyrics& l, int i);         // "" if out of range
}
