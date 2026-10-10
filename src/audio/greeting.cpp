#include "greeting.h"
#include <Arduino.h>
#include <atomic>
#include <cstring>
#include "sink.h"
#include "../util/loudness.h"

extern const uint8_t _binary_data_greeting_pcm_start[] asm("_binary_data_greeting_pcm_start");
extern const uint8_t _binary_data_greeting_pcm_end[] asm("_binary_data_greeting_pcm_end");
extern const uint8_t _binary_data_finale_pcm_start[] asm("_binary_data_finale_pcm_start");
extern const uint8_t _binary_data_finale_pcm_end[] asm("_binary_data_finale_pcm_end");
extern const uint8_t _binary_data_opener_pcm_start[] asm("_binary_data_opener_pcm_start");
extern const uint8_t _binary_data_opener_pcm_end[] asm("_binary_data_opener_pcm_end");

namespace greeting {

constexpr int CLIP_RATE = 16000;         // tools/gen_greeting.py output rate (16-bit signed LE)
constexpr int CHUNK = 128;               // samples per audiosink::write

// Volumes are per board (audiosink): the CYD's DAC distorts with level, the S3's codec doesn't.
struct Clip { const uint8_t* data; size_t samples; const char* name; const int* volume; };
static const Clip GREETING = {_binary_data_greeting_pcm_start,
                              (size_t)(_binary_data_greeting_pcm_end - _binary_data_greeting_pcm_start) / 2,
                              "greeting", &audiosink::GREETING_VOLUME};
static const Clip FINALE = {_binary_data_finale_pcm_start,
                            (size_t)(_binary_data_finale_pcm_end - _binary_data_finale_pcm_start) / 2,
                            "finale", &audiosink::FINALE_VOLUME};
static const Clip OPENER = {_binary_data_opener_pcm_start,
                            (size_t)(_binary_data_opener_pcm_end - _binary_data_opener_pcm_start) / 2,
                            "opener", &audiosink::GREETING_VOLUME};

// Clips playing (task alive, audio output open). Atomic: callers on core 1, clip tasks on either core.
static std::atomic<int> s_playing{0};
// One clip at a time on the audio output (audiosink's contract): a clip that starts while another
// still plays waits here instead of colliding with it. Created by the first play*() call (setup()).
static SemaphoreHandle_t s_output = nullptr;


static void task(void* arg) {
    const Clip* c = (const Clip*)arg;
    const int volume = *c->volume;
    xSemaphoreTake(s_output, portMAX_DELAY);
    const uint32_t t0 = millis();
    if (!audiosink::open(CLIP_RATE)) {
        xSemaphoreGive(s_output);
        s_playing.fetch_sub(1);
        vTaskDelete(nullptr);
    }
    int16_t buf[CHUNK];
    for (size_t i = 0; i < c->samples;) {
        size_t n = 0;
        for (; n < CHUNK && i < c->samples; ++n, ++i) {
            int16_t s;
            memcpy(&s, c->data + 2 * i, 2);              // embedded data has no alignment guarantee
            buf[n] = (int16_t)((int32_t)s * volume / 100);
        }
        audiosink::write(buf, n);
    }
    audiosink::close();
    xSemaphoreGive(s_output);
    Serial.printf("[greeting] %s: played %u samples in ~%u ms (clip %u ms)\n", c->name,
                  (unsigned)c->samples, (unsigned)(millis() - t0),
                  (unsigned)(c->samples * 1000 / CLIP_RATE));
    s_playing.fetch_sub(1);
    vTaskDelete(nullptr);
}

static void start(const Clip* c, const char* name) {
    if (!s_output) s_output = xSemaphoreCreateMutex();
    s_playing.fetch_add(1);
    if (xTaskCreate(task, name, 3072, const_cast<Clip*>(c), 1, nullptr) != pdPASS) {
        s_playing.fetch_sub(1);              // never started: don't keep the boot scene waiting
        Serial.printf("[greeting] %s: no memory for its task\n", name);
    }
}

void play() { start(&GREETING, "greeting"); }

void playFinale() {
    if (FINALE.samples <= 1) return;                 // 1 sample = the build's placeholder
    start(&FINALE, "finale");
}

void playOpener() {
    if (OPENER.samples <= 1) return;                 // 1 sample = the build's placeholder
    start(&OPENER, "opener");
}

uint32_t openerMs() { return OPENER.samples > 1 ? (uint32_t)((uint64_t)OPENER.samples * 1000 / CLIP_RATE) : 0; }

uint32_t durationMs() { return (uint32_t)((uint64_t)GREETING.samples * 1000 / CLIP_RATE); }

bool playing() { return s_playing.load() > 0; }

uint32_t finaleMs() { return FINALE.samples > 1 ? (uint32_t)((uint64_t)FINALE.samples * 1000 / CLIP_RATE) : 0; }

uint8_t level(uint32_t ms) {
    const size_t at = (size_t)((uint64_t)ms * CLIP_RATE / 1000);
    return loudness::loudness(GREETING.data, GREETING.samples, at, CLIP_RATE / 50);
}

}
