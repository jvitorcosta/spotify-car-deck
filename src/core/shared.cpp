#include "shared.h"
#include "../util/text.h"
#include <string.h>
#include "../lyrics/lrclib.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace shared {

static SemaphoreHandle_t g_mtx = nullptr;
static AppState g_state{};
static uint32_t g_artGen = 0;  // gen the bitmap is valid for (0 = invalid)
static bool g_artNew = false;
static uint32_t g_lyricsGen = 0;   // gen the arena is valid for (0 = invalid)
static uint32_t g_walkerGen = 0;
static uint32_t g_statusGen = 0;   // gen g_status belongs to (0 = none)
static lyricstatus::Status g_status = lyricstatus::Status::Searching;
static uint32_t g_genreGen = 0;    // gen g_genre was posted for (0 = none waiting)
static uint8_t g_genre = 0;

void begin() { if (!g_mtx) g_mtx = xSemaphoreCreateMutex(); }
void lock() { xSemaphoreTake(g_mtx, portMAX_DELAY); }
void unlock() { xSemaphoreGive(g_mtx); }

void publish(const AppState& st) { Guard g; g_state = st; }
void snapshot(AppState& out) { Guard g; out = g_state; }

void postWalker(uint32_t gen) { Guard g; if (gen == g_state.trackGen) g_walkerGen = gen; }

void artInvalidate() { Guard g; g_artGen = 0; g_artNew = false; }

void postArt(uint32_t gen) {
    Guard g;
    if (gen != g_state.trackGen) return;   // stale: track moved on (bitmap stays invalid)
    g_artGen = gen;
    g_artNew = true;
}

bool takeArt(uint32_t gen) {
    Guard g;
    if (!g_artNew || g_artGen != gen) return false;
    g_artNew = false;
    return true;
}

bool artValidLocked(uint32_t gen) { return gen != 0 && g_artGen == gen; }

void lyricsInvalidate() { Guard g; g_lyricsGen = 0; }

void postLyrics(uint32_t gen) { Guard g; if (gen == g_state.trackGen) g_lyricsGen = gen; }

void postLyricsStatus(uint32_t gen, lyricstatus::Status s) {
    Guard g;
    if (gen != g_state.trackGen) return;   // stale: track moved on
    g_statusGen = gen;
    g_status = s;
}

void lyricView(uint32_t gen, uint32_t posMs, LyricView& out) {
    Guard g;
    out.status = (gen != 0 && g_statusGen == gen) ? g_status : lyricstatus::Status::Searching;
    out.firstLineMs = 0;
    out.line[0] = '\0';
    if (gen == 0 || g_lyricsGen != gen) return;
    const lyricbuf::Lyrics& l = lyricsvc::arena();
    if (l.n > 0) out.firstLineMs = l.lines[0].tMs;
    int idx = lyricbuf::currentIndex(l, posMs);
    if (idx < 0) idx = 0;                   // intro: show the upcoming first line
    txt::copy(out.line, lyricbuf::lineText(l, idx), sizeof(out.line));
}

bool takeWalker(uint32_t gen) {
    Guard g;
    if (gen == 0 || g_walkerGen != gen) return false;
    g_walkerGen = 0;
    return true;
}

void postGenre(uint32_t gen, uint8_t badge) {
    Guard g;
    if (gen != g_state.trackGen) return;   // stale: track moved on
    g_genreGen = gen;
    g_genre = badge;
}

bool takeGenre(uint32_t gen, uint8_t* badge) {
    Guard g;
    if (gen == 0 || g_genreGen != gen) return false;
    g_genreGen = 0;
    *badge = g_genre;
    return true;
}

}
