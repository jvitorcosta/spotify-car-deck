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

int parse(const char* lrc, Lyrics& out) {
    out.n = 0;
    out.text[0] = '\0';
    if (!lrc) return 0;
    int used = 1;                         // text[0] is the shared "" for out-of-range lookups
    for (const char* p = lrc; *p && out.n < MAX_LINES;) {
        const char* eol = strchr(p, '\n');
        const char* end = eol ? eol : p + strlen(p);
        uint32_t t;
        const char* txt;
        if (parseTag(p, end, &t, &txt)) {
            const char* tend = end;
            while (tend > txt && (tend[-1] == '\r' || tend[-1] == '\n')) --tend;
            int len = (int)(tend - txt);
            if (used + len + 1 > TEXT_CAP) break;   // arena full: drop the rest
            memcpy(out.text + used, txt, len);
            out.text[used + len] = '\0';
            out.lines[out.n++] = {t, (uint16_t)used};
            used += len + 1;
        }
        if (!eol) break;
        p = eol + 1;
    }
    for (int i = 1; i < out.n; ++i) {     // insertion sort by time (input is nearly sorted)
        Line v = out.lines[i];
        int j = i - 1;
        while (j >= 0 && out.lines[j].tMs > v.tMs) { out.lines[j + 1] = out.lines[j]; --j; }
        out.lines[j + 1] = v;
    }
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
