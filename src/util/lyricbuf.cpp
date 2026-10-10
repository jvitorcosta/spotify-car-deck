#include "lyricbuf.h"
#include <cstdlib>
#include <cstring>

namespace lyricbuf {

// Parses the tag at the start of [p, end); on success sets tMs and the text start.
static bool parseTag(const char* p, const char* end, uint32_t* tMs, const char** text) {
    if (end - p < 10 || p[0] != '[') return false;
    const char* close = (const char*)memchr(p, ']', end - p);
    if (!close) return false;
    const char* colon = (const char*)memchr(p, ':', close - p);
    if (!colon) return false;
    char* stop = nullptr;
    long mm = strtol(p + 1, &stop, 10);
    if (stop != colon) return false;
    long ss = strtol(colon + 1, &stop, 10);
    // The text comes from LRCLIB: bound every field before multiplying (no signed overflow).
    if (mm < 0 || mm > 999 || ss < 0 || ss > 59) return false;
    uint32_t frac = 0;                    // the fraction in ms: .5 = 500, .50 = 500, .345 = 345
    if (stop < close && *stop == '.') {
        uint32_t scale = 100;
        for (const char* d = stop + 1; d < close && *d >= '0' && *d <= '9' && scale; ++d, scale /= 10)
            frac += (uint32_t)(*d - '0') * scale;
    }
    *tMs = (uint32_t)mm * 60000u + (uint32_t)ss * 1000u + frac;
    *text = close + 1;
    return true;
}

void reset(Lyrics& out) {
    out.n = 0;
    out.used = 1;                         // text[0] is the shared "" for out-of-range lookups
    out.plainTotal = 0;
    out.text[0] = '\0';
}

bool addLine(Lyrics& out, const char* p, size_t len) {
    const char* end = p + len;
    uint32_t t;
    const char* txt;
    if (!parseTag(p, end, &t, &txt)) return true;   // not a timed line: skip
    if (out.n >= MAX_LINES) return false;
    const char* tend = end;
    while (tend > txt && (tend[-1] == '\r' || tend[-1] == '\n')) --tend;
    int n = (int)(tend - txt);
    if (out.used + n + 1 > TEXT_CAP) return false;  // arena full: drop the rest
    memcpy(out.text + out.used, txt, n);
    out.text[out.used + n] = '\0';
    out.lines[out.n++] = {t, (uint16_t)out.used};
    out.used += n + 1;
    return true;
}

void finish(Lyrics& out) {
    for (int i = 1; i < out.n; ++i) {     // insertion sort by time (input is nearly sorted)
        Line v = out.lines[i];
        int j = i - 1;
        while (j >= 0 && out.lines[j].tMs > v.tMs) { out.lines[j + 1] = out.lines[j]; --j; }
        out.lines[j + 1] = v;
    }
}

bool hasTag(const char* line, size_t len) {
    uint32_t t;
    const char* txt;
    return parseTag(line, line + len, &t, &txt);
}

bool addPlain(Lyrics& out, const char* p, size_t len) {
    const char* end = p + len;
    while (end > p && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) --end;
    const char* b = p;
    while (b < end && (*b == ' ' || *b == '\t')) ++b;
    if (b == end) return true;            // blank: verse break, not a line
    ++out.plainTotal;
    int n = (int)(end - p);
    if (out.n >= MAX_LINES || out.used + n + 1 > TEXT_CAP) return false;
    memcpy(out.text + out.used, p, n);
    out.text[out.used + n] = '\0';
    out.lines[out.n++] = {0, (uint16_t)out.used};
    out.used += n + 1;
    return true;
}

void spreadPlain(Lyrics& out, uint32_t durationMs) {
    int total = out.plainTotal > out.n ? out.plainTotal : out.n;
    if (total <= 0) return;
    uint64_t start = (uint64_t)durationMs / 10, span = (uint64_t)durationMs * 8 / 10;
    for (int i = 0; i < out.n; ++i)
        out.lines[i].tMs = durationMs ? (uint32_t)(start + span * (uint64_t)i / (uint64_t)total)
                                      : (uint32_t)i * 4000u;
}

int parse(const char* lrc, Lyrics& out) {
    reset(out);
    if (!lrc) return 0;
    for (const char* p = lrc; *p;) {
        const char* eol = strchr(p, '\n');
        const char* end = eol ? eol : p + strlen(p);
        if (!addLine(out, p, (size_t)(end - p))) break;
        if (!eol) break;
        p = eol + 1;
    }
    finish(out);
    return out.n;
}

int currentIndex(const Lyrics& l, uint32_t posMs) {
    int idx = -1;
    for (int i = 0; i < l.n; ++i) {
        if (l.lines[i].tMs <= posMs) idx = i; else break;
    }
    return idx;
}

const char* lineText(const Lyrics& l, int i) {
    return (i >= 0 && i < l.n) ? l.text + l.lines[i].off : "";
}

}
