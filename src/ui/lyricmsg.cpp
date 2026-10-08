#include "lyricmsg.h"
#include "../util/text.h"
#include <cstring>

namespace lyricmsg {

// Real move and item names; {P} = the Pokemon on screen.
const char* const SEARCH[SEARCH_N] = {
    "{P} used SING! Searching for the verses...",
    "{P} is fetching this piece of art's verses...",
    "{P} used FORESIGHT! Reading the song...",
    "{P} used ECHOED VOICE! Listening for words...",
    "{P} used MIMIC! Learning the lyrics...",
    "{P} used ODOR SLEUTH! Sniffing out verses...",
    "A wild LYRIC appeared? {P} is chasing it...",
    "Accessed BILL's PC... Opening the Lyrics Box.",
    "{P} checked the POK\xC3\xA9" "DEX for these verses...",
    "{P} is listening closely...",
};
const char* const RETRY[RETRY_N] = {
    "The verses fled! {P} is trying again...",
    "Spotify is fast asleep... {P} used WAKE-UP SLAP!",
    "It's not very effective... trying again!",
};
const char* const FAIL[FAIL_N] = {
    "But it failed!",
    "OAK: There's a time and place for everything! But not now.",
    "{P}'s search missed!",
    "The verses got away!",
};
const char* const INSTRUMENTAL[INSTRUMENTAL_N] = {
    "No words here! {P} is humming along...",
    "{P} used TEETER DANCE! It's instrumental!",
    "{P} used GRASSWHISTLE! Nothing to sing...",
};
const char* const FOUND[FOUND_N] = {
    "It's super effective! Lyrics found!",
    "Gotcha! The verses were caught!",
};
const char* const IDLE = "{P} is enjoying the music";

static const char* const DEFAULT_NAME = "POK\xC3\xA9" "MON";

static size_t utf8Len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;   // stray continuation byte: copy it alone
}

// Appends len bytes of s, whole UTF-8 characters only; false once out is full.
static bool append(char* out, size_t cap, size_t& o, const char* s, size_t len, bool upper) {
    size_t i = 0;
    while (i < len) {
        size_t k = utf8Len((unsigned char)s[i]);
        if (i + k > len) k = len - i;
        if (o + k > cap - 1) return false;
        for (size_t j = 0; j < k; ++j) {
            char c = s[i + j];
            if (upper && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            out[o++] = c;
        }
        i += k;
    }
    return true;
}

void fill(const char* tmpl, const char* name, char* out, size_t n) {
    if (n == 0) return;
    const char* nm = (name && name[0]) ? name : DEFAULT_NAME;
    size_t o = 0;
    for (const char* p = tmpl; p && *p;) {
        const char* hit = strstr(p, "{P}");
        size_t lit = hit ? (size_t)(hit - p) : strlen(p);
        if (!append(out, n, o, p, lit, false) || !hit) break;
        if (!append(out, n, o, nm, strlen(nm), true)) break;
        p = hit + 3;
    }
    out[o] = '\0';
}

// Stable pseudo-random index per song; salt keeps the pools from moving in lockstep.
static int pick(uint32_t seed, int n, uint32_t salt) {
    uint32_t h = (seed + salt * 0x9E3779B9u) * 2654435761u;
    return (int)((h >> 16) % (uint32_t)n);
}

void compose(const In& in, Out& out) {
    using lyricstatus::Status;
    out.notes = true;
    out.dance = false;
    const char* tmpl = IDLE;
    switch (in.status) {
        case Status::Searching:
            tmpl = SEARCH[pick(in.seed, SEARCH_N, 1)];
            break;
        case Status::Retrying:
            tmpl = RETRY[pick(in.seed, RETRY_N, 2)];
            break;
        case Status::Synced:
        case Status::Plain:
            out.notes = in.status == Status::Synced;
            if (in.posMs < in.firstLineMs && in.statusAgeMs < FOUND_SHOW_MS) {
                tmpl = FOUND[pick(in.seed, FOUND_N, 3)];
                break;
            }
            if (!in.line || !in.line[0]) break;   // empty timed line (break): IDLE, never blank
            txt::copy(out.text, in.line, TEXT_CAP);
            out.dance = out.notes;            // timed lyrics: the notes bob along
            return;
        case Status::Instrumental:
            tmpl = in.statusAgeMs < FAIL_SHOW_MS ? INSTRUMENTAL[pick(in.seed, INSTRUMENTAL_N, 4)] : IDLE;
            break;
        case Status::None:
            tmpl = in.statusAgeMs < FAIL_SHOW_MS ? FAIL[pick(in.seed, FAIL_N, 5)] : IDLE;
            break;
    }
    fill(tmpl, in.pokeName, out.text, TEXT_CAP);
}

}
