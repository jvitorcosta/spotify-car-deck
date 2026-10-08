#pragma once
#include <cstddef>
#include <cstdint>
#include "lyricbuf.h"
// Streams lyrics out of an LRCLIB JSON response byte by byte, without a JSON document
// (ArduinoJson grew strings by doubling, up to 16 KB contiguous while TLS was open).
// Reads "instrumental", "plainLyrics" and "syncedLyrics" (plus "duration" in search mode),
// unescaping strings (\n \" \\ \/ \t \uXXXX incl. surrogate pairs -> UTF-8) straight into a
// lyricbuf arena one line at a time. Synced lines replace plain ones; instrumental wins.
// Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md. PURE, host-tested.
namespace lrcstream {
constexpr int LINE_CAP = 256;               // longer lines are cut
constexpr size_t SEARCH_CAP = 48 * 1024;    // search mode: stop after this many bytes
constexpr uint32_t DURATION_TOL_MS = 3000;  // search mode: accepted |duration - track|
enum class Mode { Get, Search };            // /api/get object, /api/search array of objects
enum class Kind { None, Synced, Plain, Instrumental };
class Extractor {
public:
    // Search mode skips objects whose duration (listed before their lyrics) is more than
    // DURATION_TOL_MS away from durationMs.
    explicit Extractor(lyricbuf::Lyrics& out, Mode mode = Mode::Get, uint32_t durationMs = 0);
    void feed(char c);
    bool done() const { return done_; }               // stop reading the stream
    bool found() const { return syncedLines_ > 0; }   // synced lines were stored
    bool truncated() const { return truncated_; }     // arena filled up
    int finish();                                      // flush + sort; returns stored lines
    Kind kind() const { return kind_; }                // valid after finish()
private:
    enum class Str { None, Key, Skip, Plain, Synced };
    enum class Esc { None, Backslash, Unicode };
    static constexpr int KEY_CAP = 16, LIT_CAP = 24;
    bool accepted() const;
    void stringChar(char c);
    void beginValueString();
    void endValueString();
    void endLiteral();
    void put(char c);
    void putCodepoint(uint32_t cp);
    void endLine();
    lyricbuf::Lyrics& out_;
    Mode mode_;
    uint32_t targetMs_;
    int objDepth_;                 // nesting level whose keys we read (get: 1, search: 2)
    int depth_ = 0;
    bool inStr_ = false, inLit_ = false, expectValue_ = false;
    Str str_ = Str::None;
    Esc esc_ = Esc::None;
    char key_[KEY_CAP] = {0}, curKey_[KEY_CAP] = {0};
    int keyLen_ = 0;
    char lit_[LIT_CAP] = {0};
    int litLen_ = 0;
    char line_[LINE_CAP];
    int len_ = 0;
    uint32_t hex_ = 0, hi_ = 0;
    int hexDigits_ = 0;
    bool objMatch_ = false;        // search: this object's duration matched
    bool plainTaken_ = false;      // the first accepted plain string was read
    bool instrumental_ = false, truncated_ = false, done_ = false;
    int syncedLines_ = 0;
    size_t bytes_ = 0;
    Kind kind_ = Kind::None;
};
}
