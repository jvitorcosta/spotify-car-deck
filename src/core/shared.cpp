#include "shared.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace shared {

static SemaphoreHandle_t g_mtx = nullptr;
static AppState g_state{};
static uint8_t* g_art = nullptr;
static int g_artLen = 0;
static uint32_t g_artGen = 0;
static std::vector<lrc::LrcLine>* g_lyrics = nullptr;
static uint32_t g_lyricsGen = 0;
static uint32_t g_walkerGen = 0;

void begin() { if (!g_mtx) g_mtx = xSemaphoreCreateMutex(); }
void lock() { xSemaphoreTake(g_mtx, portMAX_DELAY); }
void unlock() { xSemaphoreGive(g_mtx); }

void publish(const AppState& st) { Guard g; g_state = st; }
void snapshot(AppState& out) { Guard g; out = g_state; }

void postArt(uint32_t gen, uint8_t* jpeg, int len) {
    Guard g;
    if (gen != g_state.trackGen) { free(jpeg); return; }   // stale: track moved on
    free(g_art);
    g_art = jpeg; g_artLen = len; g_artGen = gen;
}

void postLyrics(uint32_t gen, std::vector<lrc::LrcLine>* lines) {
    Guard g;
    if (gen != g_state.trackGen) { delete lines; return; }
    delete g_lyrics;
    g_lyrics = lines; g_lyricsGen = gen;
}

void postWalker(uint32_t gen) { Guard g; if (gen == g_state.trackGen) g_walkerGen = gen; }

bool takeArt(uint32_t gen, uint8_t** jpeg, int* len) {
    Guard g;
    if (!g_art || g_artGen != gen) return false;
    *jpeg = g_art; *len = g_artLen;
    g_art = nullptr; g_artLen = 0;
    return true;
}

bool takeLyrics(uint32_t gen, std::vector<lrc::LrcLine>** lines) {
    Guard g;
    if (!g_lyrics || g_lyricsGen != gen) return false;
    *lines = g_lyrics;
    g_lyrics = nullptr;
    return true;
}

bool takeWalker(uint32_t gen) {
    Guard g;
    if (gen == 0 || g_walkerGen != gen) return false;
    g_walkerGen = 0;
    return true;
}

}
