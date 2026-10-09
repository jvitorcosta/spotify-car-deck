#include "greeting.h"
#include <Arduino.h>
#include <cstring>
#include <driver/i2s.h>
#include "../util/loudness.h"

extern const uint8_t _binary_data_greeting_pcm_start[] asm("_binary_data_greeting_pcm_start");
extern const uint8_t _binary_data_greeting_pcm_end[] asm("_binary_data_greeting_pcm_end");
extern const uint8_t _binary_data_finale_pcm_start[] asm("_binary_data_finale_pcm_start");
extern const uint8_t _binary_data_finale_pcm_end[] asm("_binary_data_finale_pcm_end");
extern const uint8_t _binary_data_opener_pcm_start[] asm("_binary_data_opener_pcm_start");
extern const uint8_t _binary_data_opener_pcm_end[] asm("_binary_data_opener_pcm_end");

namespace greeting {

constexpr i2s_port_t PORT = I2S_NUM_0;   // only I2S0 can drive the built-in DAC
constexpr int CLIP_RATE = 16000;         // tools/gen_greeting.py output rate (16-bit signed LE)
// The DAC runs at twice the clip rate. In built-in DAC mode ESP-IDF 4.4 sets the 8-bit clock
// divider to 160 MHz / (rate * 32), which wraps below ~19.6 kHz: 16 kHz played ~5.5x too fast
// (measured 87.8 kHz) while i2s_get_clk still reported 16000. 32 kHz divides cleanly (156).
constexpr int OUT_RATE = CLIP_RATE * 2;
constexpr int RAMP = CLIP_RATE / 50;     // 20 ms ramp from/to 0 so the amp doesn't pop
constexpr int DMA_BUFS = 4;
constexpr int DMA_LEN = 256;             // frames per DMA buffer
constexpr int CHUNK = 128;               // frames per i2s_write
constexpr int32_t MID = 32768;           // silence, on the 0..65535 scale the DAC's 8 bits come from
constexpr int VOLUME = 30;               // percent of the clip's level
// The finale (a loud meme clip, RMS ~8 dB above the greeting) distorted the speaker at 30 %;
// this board distorts with sustained level, so it gets its own volume, matched by average loudness.
constexpr int FINALE_VOLUME = 12;

struct Clip { const uint8_t* data; size_t samples; const char* name; int volume; };
static const Clip GREETING = {_binary_data_greeting_pcm_start,
                              (size_t)(_binary_data_greeting_pcm_end - _binary_data_greeting_pcm_start) / 2, "greeting", VOLUME};
static const Clip FINALE = {_binary_data_finale_pcm_start,
                            (size_t)(_binary_data_finale_pcm_end - _binary_data_finale_pcm_start) / 2, "finale", FINALE_VOLUME};
static const Clip OPENER = {_binary_data_opener_pcm_start,
                            (size_t)(_binary_data_opener_pcm_end - _binary_data_opener_pcm_start) / 2, "opener", VOLUME};

// Clip-rate sample i of the stream on the 0..65535 scale: ramp up to MID, the clip at VOLUME,
// ramp back down to 0 (the DAC's idle level). Kept at 16 bits so the volume costs no resolution.
static volatile int s_playing = 0;   // clips playing (task alive, I2S installed)

static int32_t sampleAt(size_t i, const uint8_t* clip, size_t len, int volume) {
    if (i < RAMP) return MID * (int32_t)i / RAMP;
    i -= RAMP;
    if (i < len) {
        int16_t s;
        memcpy(&s, clip + 2 * i, 2);     // embedded data has no alignment guarantee
        return MID + (int32_t)s * volume / 100;
    }
    i -= len;
    if (i < RAMP) return MID * (int32_t)(RAMP - i) / RAMP;
    return 0;
}

static void task(void* arg) {
    const Clip* c = (const Clip*)arg;
    const uint8_t* clip = c->data;
    const size_t len = c->samples;

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = OUT_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;   // the DAC takes the high byte
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;   // same sample on both: order quirks don't matter
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count = DMA_BUFS;
    cfg.dma_buf_len = DMA_LEN;
    cfg.tx_desc_auto_clear = true;                     // send silence on underflow, not the last buffers again
    if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[greeting] i2s install failed");
        s_playing = s_playing - 1;
        vTaskDelete(nullptr);
    }
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);         // GPIO 26 only (GPIO 25 is touch SCLK)

    uint16_t buf[CHUNK * 2];
    const size_t total = (RAMP + len + RAMP) * 2;      // output frames
    int32_t err = 0;                                   // noise-shaping error carried to the next frame
    const uint32_t t0 = millis();
    for (size_t j = 0; j < total;) {
        size_t n = 0;
        for (; n < CHUNK && j < total; ++n, ++j) {
            // 2x upsampling: even frames take the clip sample, odd ones the midpoint to the next
            const size_t i = j / 2;
            int32_t v = sampleAt(i, clip, len, c->volume);
            if (j & 1) v = (v + sampleAt(i + 1, clip, len, c->volume)) / 2;
            // First-order noise shaping: the DAC keeps only the high byte; feeding the dropped
            // low byte into the next frame moves the rounding hiss up towards 16 kHz.
            int32_t want = v + err;
            int32_t q = constrain(want, 0, 65535) & 0xFF00;
            err = want - q;
            buf[2 * n] = (uint16_t)q;
            buf[2 * n + 1] = (uint16_t)q;
        }
        size_t written;
        i2s_write(PORT, buf, n * 2 * sizeof(uint16_t), &written, portMAX_DELAY);
    }
    // i2s_write returns once the last chunk is queued; DMA still holds up to DMA_BUFS buffers.
    constexpr uint32_t DMA_MS = 1000 * DMA_BUFS * DMA_LEN / OUT_RATE;
    const uint32_t playedMs = millis() - t0 + DMA_MS;
    // Let the DMA drain (on underflow it now sends zeros, matching the ramp's end), then release everything.
    vTaskDelay(pdMS_TO_TICKS(DMA_MS + 20));
    i2s_driver_uninstall(PORT);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    Serial.printf("[greeting] %s: played %u samples in ~%u ms (expected %u ms)\n", c->name, (unsigned)len,
                  (unsigned)playedMs, (unsigned)(total * 1000 / OUT_RATE));
    s_playing = s_playing - 1;
    vTaskDelete(nullptr);
}

void play() {
    s_playing = s_playing + 1;
    xTaskCreate(task, "greeting", 3072, (void*)&GREETING, 1, nullptr);
}

void playFinale() {
    if (FINALE.samples <= 1) return;                 // 1 sample = the build's placeholder
    s_playing = s_playing + 1;
    xTaskCreate(task, "finale", 3072, (void*)&FINALE, 1, nullptr);
}

void playOpener() {
    if (OPENER.samples <= 1) return;                 // 1 sample = the build's placeholder
    s_playing = s_playing + 1;
    xTaskCreate(task, "opener", 3072, (void*)&OPENER, 1, nullptr);
}

uint32_t openerMs() { return OPENER.samples > 1 ? (uint32_t)((uint64_t)OPENER.samples * 1000 / CLIP_RATE) : 0; }

uint32_t durationMs() { return (uint32_t)((uint64_t)GREETING.samples * 1000 / CLIP_RATE); }

bool playing() { return s_playing > 0; }

uint32_t finaleMs() { return FINALE.samples > 1 ? (uint32_t)((uint64_t)FINALE.samples * 1000 / CLIP_RATE) : 0; }

uint8_t level(uint32_t ms) {
    const size_t at = (size_t)((uint64_t)ms * CLIP_RATE / 1000);
    return loudness::loudness(GREETING.data, GREETING.samples, at, CLIP_RATE / 50);
}

}
