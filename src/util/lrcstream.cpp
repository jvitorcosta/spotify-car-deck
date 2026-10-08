#include "lrcstream.h"
#include <cstdlib>
#include <cstring>

namespace lrcstream {

static bool keyIs(const char* k, const char* name) { return strcmp(k, name) == 0; }

Extractor::Extractor(lyricbuf::Lyrics& out, Mode mode, uint32_t durationMs)
    : out_(out), mode_(mode), targetMs_(durationMs),
      objDepth_(mode == Mode::Search ? 2 : 1) {
    lyricbuf::reset(out_);
}

bool Extractor::accepted() const { return mode_ == Mode::Get || objMatch_; }

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
    if (str_ == Str::Synced) {
        if (lyricbuf::hasTag(line_, (size_t)len_)) {          // untagged lines are skipped
            if (syncedLines_ == 0) {                          // synced replaces any plain lines
                lyricbuf::reset(out_);
                truncated_ = false;
            }
            if (lyricbuf::addLine(out_, line_, (size_t)len_)) ++syncedLines_;
            else { truncated_ = true; done_ = true; }        // arena full: rest not needed
        }
    } else if (str_ == Str::Plain) {
        if (!lyricbuf::addPlain(out_, line_, (size_t)len_)) truncated_ = true;   // still counted
    }
    len_ = 0;
}

void Extractor::beginValueString() {
    inStr_ = true;
    esc_ = Esc::None;
    len_ = 0;
    str_ = Str::Skip;
    if (depth_ != objDepth_ || !accepted()) return;
    if (keyIs(curKey_, "syncedLyrics")) {
        str_ = Str::Synced;
    } else if (keyIs(curKey_, "plainLyrics") && syncedLines_ == 0 && !plainTaken_) {
        str_ = Str::Plain;
        plainTaken_ = true;
    }
}

void Extractor::endValueString() {
    if (len_ > 0) endLine();
    Str s = str_;
    inStr_ = false;
    expectValue_ = false;
    str_ = Str::None;
    if (s == Str::Synced && syncedLines_ > 0) done_ = true;   // timed lyrics: nothing else needed
    if (s == Str::Plain && out_.plainTotal == 0) plainTaken_ = false;   // "" counts as absent
}

void Extractor::stringChar(char c) {
    if (str_ == Str::Key || str_ == Str::Skip) {
        if (esc_ != Esc::None) {
            esc_ = Esc::None;
        } else if (c == '\\') {
            esc_ = Esc::Backslash;
            return;
        } else if (c == '"') {
            inStr_ = false;
            if (str_ == Str::Key) key_[keyLen_] = '\0';
            else expectValue_ = false;
            str_ = Str::None;
            return;
        }
        if (str_ == Str::Key && keyLen_ < KEY_CAP - 1) key_[keyLen_++] = c;
        return;
    }
    // Lyric text (Plain / Synced).
    if (esc_ == Esc::Unicode) {
        int v = (c >= '0' && c <= '9') ? c - '0'
              : (c >= 'a' && c <= 'f') ? c - 'a' + 10
              : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) { esc_ = Esc::None; return; }               // malformed escape: dropped
        hex_ = (hex_ << 4) | (uint32_t)v;
        if (++hexDigits_ < 4) return;
        esc_ = Esc::None;
        if (hex_ >= 0xD800 && hex_ <= 0xDBFF) {                 // high surrogate: wait for low
            hi_ = hex_;
        } else if (hex_ >= 0xDC00 && hex_ <= 0xDFFF) {          // low surrogate
            if (hi_) putCodepoint(0x10000 + ((hi_ - 0xD800) << 10) + (hex_ - 0xDC00));
            hi_ = 0;
        } else {
            hi_ = 0;
            putCodepoint(hex_);
        }
        return;
    }
    if (esc_ == Esc::Backslash) {
        esc_ = Esc::None;
        switch (c) {
            case 'n': endLine(); break;
            case 't': put(' '); break;
            case 'r': break;
            case 'u': esc_ = Esc::Unicode; hex_ = 0; hexDigits_ = 0; break;
            default: put(c); break;   // \" \\ \/ (and anything unexpected, literally)
        }
        return;
    }
    if (c == '\\') { esc_ = Esc::Backslash; return; }
    if (c == '"') { endValueString(); return; }
    put(c);                           // raw UTF-8 bytes pass through
}

void Extractor::endLiteral() {
    lit_[litLen_] = '\0';
    expectValue_ = false;
    if (depth_ != objDepth_) return;
    if (keyIs(curKey_, "duration")) {
        double s = strtod(lit_, nullptr);
        uint32_t ms = (uint32_t)(s * 1000.0 + 0.5);
        uint32_t d = ms > targetMs_ ? ms - targetMs_ : targetMs_ - ms;
        objMatch_ = s > 0 && d <= DURATION_TOL_MS;
        return;
    }
    if (keyIs(curKey_, "instrumental") && strcmp(lit_, "true") == 0 && accepted()) {
        instrumental_ = true;
        done_ = true;                 // instrumental wins over any text
    }
}

void Extractor::feed(char c) {
    if (done_) return;
    if (mode_ == Mode::Search && ++bytes_ > SEARCH_CAP) { done_ = true; return; }   // time/data cap
    if (inStr_) { stringChar(c); return; }
    if (inLit_) {
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            inLit_ = false;
            endLiteral();
            if (done_) return;
        } else {
            if (litLen_ < LIT_CAP - 1) lit_[litLen_++] = c;
            return;
        }
    }
    switch (c) {
        case '"':
            if (expectValue_) {
                beginValueString();
            } else {                  // a key (or a string inside a nested value)
                inStr_ = true;
                esc_ = Esc::None;
                str_ = Str::Key;
                keyLen_ = 0;
                key_[0] = '\0';
            }
            break;
        case ':':
            if (depth_ == objDepth_) {
                memcpy(curKey_, key_, sizeof(curKey_));
                expectValue_ = true;
            }
            break;
        case '{':
            ++depth_;
            expectValue_ = false;
            if (depth_ == objDepth_) objMatch_ = false;   // new object: duration not seen yet
            break;
        case '[':
            ++depth_;
            expectValue_ = false;
            break;
        case '}':
        case ']':
            --depth_;
            expectValue_ = false;
            // End of the top-level object/array: the answer is complete. Waiting for the server
            // to close the connection could hit the 8 s stall timer and turn it into an error.
            if (depth_ == 0) done_ = true;
            break;
        case ',':
            expectValue_ = false;
            break;
        case ' ': case '\t': case '\r': case '\n':
            break;
        default:                      // number / true / false / null
            if (expectValue_) {
                inLit_ = true;
                litLen_ = 0;
                lit_[litLen_++] = c;
            }
            break;
    }
}

int Extractor::finish() {
    if (inStr_ && (str_ == Str::Plain || str_ == Str::Synced) && len_ > 0) endLine();   // cut off mid-string
    lyricbuf::finish(out_);
    if (instrumental_) kind_ = Kind::Instrumental;
    else if (syncedLines_ > 0) kind_ = Kind::Synced;
    else if (out_.n > 0) kind_ = Kind::Plain;
    else kind_ = Kind::None;
    return out_.n;
}

}
