#pragma once
#include <cstddef>
#include <cstdint>
#include "lyricbuf.h"
// Streams the "syncedLyrics" JSON string out of an LRCLIB response byte by byte, unescaping
// it (\n \" \\ \/ \t \uXXXX incl. surrogate pairs -> UTF-8) straight into a lyricbuf arena,
// one line at a time. No JSON document: ArduinoJson grew the string by doubling (up to 16 KB
// contiguous while the TLS session was open). PURE, host-tested.
namespace lrcstream {
constexpr int LINE_CAP = 256;   // longer lines are cut (an LRC line is a timestamp + a verse)
class Extractor {
public:
    explicit Extractor(lyricbuf::Lyrics& out);
    void feed(char c);
    bool done() const { return state_ == State::Done; }   // stop reading the stream
    bool found() const { return found_; }                   // a syncedLyrics string was seen
    bool truncated() const { return truncated_; }           // arena filled up
    int finish();                                           // flush + sort; returns lines
private:
    enum class State { SeekKey, AfterKey, InString, Escape, Unicode, Done };
    void put(char c);
    void putCodepoint(uint32_t cp);
    void endLine();
    lyricbuf::Lyrics& out_;
    State state_ = State::SeekKey;
    int match_ = 0;              // chars of the key pattern matched so far
    char line_[LINE_CAP];
    int len_ = 0;
    uint32_t hex_ = 0;
    int hexDigits_ = 0;
    uint32_t highSurrogate_ = 0;
    bool found_ = false;
    bool truncated_ = false;
};
}
