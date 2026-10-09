#include "bios.h"
#include <cstring>

namespace bios {

// Types into the sky terminal (TERM_COLS wide) while the car drives in; no blank lines.
static const Line LINES[] = {
    {0, "SUPER BAIANO BIOS v1.5", nullptr, HEADER, false},
    {0, "HONDA CITY TOURING (C) 2022", nullptr, HEADER, false},
    {300, "CPU: 1.5L i-VTEC @ 6600 RPM", nullptr, PLAIN, false},
    {600, "MEMORY TEST: 4 SEATS", "OK", OK, false},
    {1000, "Detecting occupants...", nullptr, PLAIN, false},
    {1300, "DRIVER", "FOUND", OK, false},
    {1600, "PASSENGER", "NOT FOUND", WARN, false},
    {2000, "UNDERGLOW", "PURPLE [OK]", PURPLE, false},
    {2300, "SPOILER DOWNFORCE", "+2 kg [OK]", OK, false},
    {2550, "CUP HOLDERS", "2/2 [OK]", OK, false},
    {2800, "VTEC", "JUST WAIT [OK]", OK, false},
    {3100, "COFFEE LEVEL", "LOW [WARN]", WARN, false},
    {3400, "WI-FI", nullptr, OK, true},
    {3700, "POKEDEX", "1025 ENTRIES [OK]", OK, false},
    {4000, "Press DEL to enter SETUP", nullptr, PLAIN, false},
    {4300, "IGNITION", "START", OK, false},
};
constexpr int N = sizeof(LINES) / sizeof(LINES[0]);

int count() { return N; }
const Line& line(int i) { return LINES[i]; }

int format(const char* label, const char* value, char* out, int cap, int cols) {
    int n = 0;
    for (; label[n] && n < cols && n < cap - 1; ++n) out[n] = label[n];
    out[n] = 0;
    if (!value) return -1;
    int vlen = (int)strlen(value);
    const int room = cols - n - 3;               // at least " . " between label and value
    if (vlen > room) vlen = room < 0 ? 0 : room;
    const int v = cols - vlen;
    for (int k = n; k < v; ++k) out[k] = (k == n || k == v - 1) ? ' ' : '.';
    memcpy(out + v, value, vlen);
    out[cols] = 0;
    return v;
}

int shown(int i, uint32_t t, int len) {
    const Line& l = LINES[i];
    if (t < l.at) return 0;
    return typed(t - l.at, (int)strlen(l.label), len);
}

int typed(uint32_t dt, int labelLen, int len) {
    if (labelLen >= len || dt >= DOTS_MS) return len;   // plain text: whole at once
    return labelLen + (int)((uint64_t)(len - labelLen) * dt / DOTS_MS);
}

bool termSet(Term& term, const char* label, const char* value, uint8_t tone, uint32_t t, bool pending) {
    char v[TERM_VAL];
    strncpy(v, value, TERM_VAL - 1);
    v[TERM_VAL - 1] = 0;
    for (int i = 0; i < term.n; ++i) {
        TermLine& r = term.rows[i];
        if (strcmp(r.label, label) != 0) continue;
        if (r.tone == tone && r.pending == pending && strcmp(r.value, v) == 0) return false;
        memcpy(r.value, v, TERM_VAL);
        r.tone = tone;
        r.changed = t;
        r.pending = pending;
        ++term.gen;
        return true;
    }
    if (term.n == TERM_ROWS) {                       // full: the oldest scrolls off
        memmove(&term.rows[0], &term.rows[1], sizeof(TermLine) * (TERM_ROWS - 1));
        --term.n;
    }
    TermLine& r = term.rows[term.n++];
    r.label = label;
    memcpy(r.value, v, TERM_VAL);
    r.tone = tone;
    r.at = r.changed = t;
    r.pending = pending;
    ++term.gen;
    return true;
}

void termScript(Term& term, uint32_t t) {
    for (; term.fed < N && LINES[term.fed].at <= t; ++term.fed) {
        const Line& l = LINES[term.fed];
        if (!l.live) termSet(term, l.label, l.value ? l.value : "", l.tone, l.at);
    }
}

uint32_t liveAt() {
    for (const Line& l : LINES)
        if (l.live) return l.at;
    return 0;
}

void termExpire(Term& term, uint32_t t, uint32_t ttl) {
    int k = 0;
    for (int i = 0; i < term.n; ++i) {
        const TermLine& r = term.rows[i];
        if (!r.pending && t - r.changed >= ttl) continue;     // dropped
        if (k != i) term.rows[k] = r;
        ++k;
    }
    if (k != term.n) {
        term.n = k;
        ++term.gen;
    }
}
}
