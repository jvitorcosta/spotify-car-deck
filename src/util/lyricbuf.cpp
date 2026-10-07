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
    long cs = 0;
    if (stop < close && *stop == '.') cs = strtol(stop + 1, nullptr, 10);
    if (mm < 0 || ss < 0 || ss > 59) return false;
    *tMs = (uint32_t)(mm * 60000 + ss * 1000 + cs * 10);
    *text = close + 1;
    return true;
}

void reset(Lyrics& out) {
    out.n = 0;
    out.used = 1;                         // text[0] is the shared "" for out-of-range lookups
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
