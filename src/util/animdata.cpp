#include "animdata.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace animdata {

// Locates the <Anim>...</Anim> block whose <Name> is exactly `name`.
static bool findAnim(const char* xml, const char* name, const char** b, const char** e) {
    size_t nl = strlen(name);
    for (const char* p = strstr(xml, "<Anim>"); p; p = strstr(p, "<Anim>")) {
        const char* end = strstr(p, "</Anim>");
        if (!end) return false;
        const char* n = strstr(p, "<Name>");
        if (n && n < end) {
            n += 6;
            if (strncmp(n, name, nl) == 0 && n[nl] == '<') { *b = p; *e = end; return true; }
        }
        p = end + 7;
    }
    return false;
}

// Start of the text inside <tag> within [b,e), or nullptr.
static const char* tagBody(const char* b, const char* e, const char* tag) {
    char open[32];
    snprintf(open, sizeof(open), "<%s>", tag);
    const char* p = strstr(b, open);
    if (!p || p >= e) return nullptr;
    return p + strlen(open);
}

static int tagInt(const char* b, const char* e, const char* tag) {
    const char* p = tagBody(b, e, tag);
    return p ? atoi(p) : -1;
}

static bool tagText(const char* b, const char* e, const char* tag, char* out, size_t n) {
    const char* p = tagBody(b, e, tag);
    if (!p) return false;
    size_t i = 0;
    while (p[i] && p[i] != '<' && i + 1 < n) { out[i] = p[i]; ++i; }
    out[i] = '\0';
    return i > 0;
}

WalkAnim parseWalk(const char* xml) {
    WalkAnim w{};
    if (!xml) return w;
    const char *b, *e;
    if (!findAnim(xml, "Walk", &b, &e)) return w;
    char target[32];
    int hops = 0;
    while (tagText(b, e, "CopyOf", target, sizeof(target))) {
        if (++hops > 4 || !findAnim(xml, target, &b, &e)) return w;
    }
    w.frameW = tagInt(b, e, "FrameWidth");
    w.frameH = tagInt(b, e, "FrameHeight");
    for (const char* p = strstr(b, "<Duration>"); p && p < e && w.frames < MAX_FRAMES;
         p = strstr(p, "<Duration>")) {
        p += 10;
        w.ticks[w.frames++] = (uint16_t)atoi(p);
    }
    w.ok = w.frameW > 0 && w.frameH > 0 && w.frames > 0;
    return w;
}

}
