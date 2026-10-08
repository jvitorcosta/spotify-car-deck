#include "shared.h"
#include "../util/text.h"
#include <string.h>
#include "../lyrics/lrclib.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace shared {

static SemaphoreHandle_t s_mtx = nullptr;
static AppState s_state{};
static uint32_t s_artGen = 0;  // gen the bitmap is valid for (0 = invalid)
static bool s_artNew = false;
static uint32_t s_lyricsGen = 0;   // gen the arena is valid for (0 = invalid)
static uint32_t s_walkerGen = 0;
static uint32_t s_statusGen = 0;   // gen s_status belongs to (0 = none)
static lyricstatus::Status s_status = lyricstatus::Status::Searching;
static uint32_t s_genreGen = 0;    // gen s_genre was posted for (0 = none waiting)
static uint8_t s_genre = 0;

void begin() { if (!s_mtx) s_mtx = xSemaphoreCreateMutex(); }
void lock() { xSemaphoreTake(s_mtx, portMAX_DELAY); }
void unlock() { xSemaphoreGive(s_mtx); }

void publish(const AppState& st) { Guard g; s_state = st; }
void snapshot(AppState& out) { Guard g; out = s_state; }

void postWalker(uint32_t gen) { Guard g; if (gen == s_state.trackGen) s_walkerGen = gen; }

void artInvalidate() { Guard g; s_artGen = 0; s_artNew = false; }

void postArt(uint32_t gen) {
    Guard g;
    if (gen != s_state.trackGen) return;   // stale: track moved on (bitmap stays invalid)
    s_artGen = gen;
    s_artNew = true;
}

bool takeArt(uint32_t gen) {
    Guard g;
    if (!s_artNew || s_artGen != gen) return false;
    s_artNew = false;
    return true;
}

bool artValidLocked(uint32_t gen) { return gen != 0 && s_artGen == gen; }

void lyricsInvalidate() { Guard g; s_lyricsGen = 0; }

void postLyrics(uint32_t gen) { Guard g; if (gen == s_state.trackGen) s_lyricsGen = gen; }

void postLyricsStatus(uint32_t gen, lyricstatus::Status s) {
    Guard g;
    if (gen != s_state.trackGen) return;   // stale: track moved on
    s_statusGen = gen;
    s_status = s;
}

void lyricView(uint32_t gen, uint32_t posMs, LyricView& out) {
    Guard g;
    out.status = (gen != 0 && s_statusGen == gen) ? s_status : lyricstatus::Status::Searching;
    out.firstLineMs = 0;
    out.line[0] = '\0';
    if (gen == 0 || s_lyricsGen != gen) return;
    const lyricbuf::Lyrics& l = lyricsvc::arena();
    if (l.n > 0) out.firstLineMs = l.lines[0].tMs;
    int idx = lyricbuf::currentIndex(l, posMs);
    if (idx < 0) idx = 0;                   // intro: show the upcoming first line
    txt::copy(out.line, lyricbuf::lineText(l, idx), sizeof(out.line));
}

bool takeWalker(uint32_t gen) {
    Guard g;
    if (gen == 0 || s_walkerGen != gen) return false;
    s_walkerGen = 0;
    return true;
}

void postGenre(uint32_t gen, uint8_t badge) {
    Guard g;
    if (gen != s_state.trackGen) return;   // stale: track moved on
    s_genreGen = gen;
    s_genre = badge;
}

bool takeGenre(uint32_t gen, uint8_t* badge) {
    Guard g;
    if (gen == 0 || s_genreGen != gen) return false;
    s_genreGen = 0;
    *badge = s_genre;
    return true;
}

}
