#include "lrcstream.h"

namespace lrcstream {

static const char KEY[] = "\"syncedLyrics\"";
static const int KEY_LEN = sizeof(KEY) - 1;

Extractor::Extractor(lyricbuf::Lyrics& out) : out_(out) { lyricbuf::reset(out_); }

void Extractor::put(char c) {
    if (len_ < LINE_CAP - 1) line_[len_++] = c;   // overlong line: cut, never overflow
}

void Extractor::putCodepoint(uint32_t cp) {
    if (cp < 0x80) {
        put((char)cp);
    } else if (cp < 0x800) {
        put((char)(0xC0 | (cp >> 6)));
        put((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        put((char)(0xE0 | (cp >> 12)));
        put((char)(0x80 | ((cp >> 6) & 0x3F)));
        put((char)(0x80 | (cp & 0x3F)));
    } else {
        put((char)(0xF0 | (cp >> 18)));
        put((char)(0x80 | ((cp >> 12) & 0x3F)));
        put((char)(0x80 | ((cp >> 6) & 0x3F)));
        put((char)(0x80 | (cp & 0x3F)));
    }
}

void Extractor::endLine() {
    if (!truncated_ && !lyricbuf::addLine(out_, line_, (size_t)len_)) {
        truncated_ = true;
        state_ = State::Done;   // arena full: the rest of the response is not needed
    }
    len_ = 0;
}

void Extractor::feed(char c) {
    switch (state_) {
        case State::SeekKey:
            if (c == KEY[match_]) {
                if (++match_ == KEY_LEN) { state_ = State::AfterKey; match_ = 0; }
            } else {
                match_ = (c == KEY[0]) ? 1 : 0;
            }
            break;
        case State::AfterKey:
            if (c == ' ' || c == ':' || c == '\t' || c == '\r' || c == '\n') break;
            if (c == '"') { found_ = true; state_ = State::InString; }
            else state_ = State::Done;   // null (or anything else): no synced lyrics
            break;
        case State::InString:
            if (c == '"') { endLine(); state_ = State::Done; }
            else if (c == '\\') state_ = State::Escape;
            else put(c);                 // raw UTF-8 bytes pass through
            break;
        case State::Escape:
            state_ = State::InString;
            switch (c) {
                case 'n': endLine(); break;
                case 't': put(' '); break;
                case 'r': break;
                case 'u': hex_ = 0; hexDigits_ = 0; state_ = State::Unicode; break;
                default: put(c); break;  // \" \\ \/ (and anything unexpected, literally)
            }
            break;
        case State::Unicode: {
            int v = (c >= '0' && c <= '9') ? c - '0'
                  : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                  : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
            if (v < 0) { state_ = State::InString; break; }
            hex_ = (hex_ << 4) | (uint32_t)v;
            if (++hexDigits_ < 4) break;
            state_ = State::InString;
            if (hex_ >= 0xD800 && hex_ <= 0xDBFF) {              // high surrogate: wait for low
                highSurrogate_ = hex_;
            } else if (hex_ >= 0xDC00 && hex_ <= 0xDFFF) {        // low surrogate
                if (highSurrogate_)
                    putCodepoint(0x10000 + ((highSurrogate_ - 0xD800) << 10) + (hex_ - 0xDC00));
                highSurrogate_ = 0;
            } else {
                highSurrogate_ = 0;
                putCodepoint(hex_);
            }
            break;
        }
        case State::Done:
            break;
    }
}

int Extractor::finish() {
    if (state_ != State::Done && found_ && len_ > 0) endLine();   // stream ended mid-string
    lyricbuf::finish(out_);
    return out_.n;
}

}
