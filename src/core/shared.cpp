#include "shared.h"
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

void lyricLine(uint32_t gen, uint32_t posMs, char* out, size_t len) {
    Guard g;
    out[0] = '\0';
    if (gen == 0 || g_lyricsGen != gen) return;
    const lyricbuf::Lyrics& l = lyricsvc::arena();
    strncpy(out, lyricbuf::lineText(l, lyricbuf::currentIndex(l, posMs)), len - 1);
    out[len - 1] = '\0';
}

bool takeWalker(uint32_t gen) {
    Guard g;
    if (gen == 0 || g_walkerGen != gen) return false;
    g_walkerGen = 0;
    return true;
}

}
