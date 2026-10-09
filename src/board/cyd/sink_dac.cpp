#include "../../audio/sink.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "../../util/dacstream.h"

namespace audiosink {

extern const int GREETING_VOLUME = 30;
// The finale (a loud meme clip, RMS ~8 dB above the greeting) distorted the speaker at 30 %;
// this board distorts with sustained level, so it gets its own volume, matched by average loudness.
extern const int FINALE_VOLUME = 12;

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;   // only I2S0 can drive the built-in DAC
constexpr int DMA_BUFS = 4;
constexpr int DMA_LEN = 256;             // frames per DMA buffer
constexpr size_t BUF_WORDS = 2 * 128;    // 128 stereo frames per i2s_write
int s_ramp = 0, s_outRate = 0;
dacstream::Encoder s_enc;
uint16_t s_buf[BUF_WORDS];
size_t s_n = 0;

void flush() {
    if (!s_n) return;
    size_t written;
    i2s_write(PORT, s_buf, s_n * sizeof(uint16_t), &written, portMAX_DELAY);
    s_n = 0;
}

void put(const uint16_t* w, int n) {
    for (int k = 0; k < n; ++k) {        // same word on both channels: order quirks don't matter
        s_buf[s_n++] = w[k];
        s_buf[s_n++] = w[k];
        if (s_n == BUF_WORDS) flush();
    }
}

void emit(int32_t v) {
    uint16_t w[2];
    put(w, s_enc.push(v, w));
}
}

bool open(int clipRate) {
    // The DAC runs at twice the clip rate. In built-in DAC mode ESP-IDF 4.4 sets the 8-bit clock
    // divider to 160 MHz / (rate * 32), which wraps below ~19.6 kHz: 16 kHz played ~5.5x too fast
    // (measured 87.8 kHz) while i2s_get_clk still reported 16000. 32 kHz divides cleanly (156).
    s_outRate = clipRate * 2;
    s_ramp = clipRate / 50;              // 20 ms ramp from/to 0 so the amp doesn't pop
    s_enc = dacstream::Encoder();
    s_n = 0;
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = s_outRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;   // the DAC takes the high byte
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count = DMA_BUFS;
    cfg.dma_buf_len = DMA_LEN;
    cfg.tx_desc_auto_clear = true;                     // send silence on underflow, not the last buffers again
    if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[greeting] i2s install failed");
        return false;
    }
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);         // GPIO 26 only (GPIO 25 is touch SCLK)
    for (int i = 0; i < s_ramp; ++i) emit(dacstream::rampUp(i, s_ramp));
    return true;
}

void write(const int16_t* s, size_t n) {
    for (size_t i = 0; i < n; ++i) emit(dacstream::MID + s[i]);
}

void close() {
    for (int i = 0; i < s_ramp; ++i) emit(dacstream::rampDown(i, s_ramp));
    uint16_t w[2];
    put(w, s_enc.finish(w));
    flush();
    // i2s_write returns once the last chunk is queued; DMA still holds up to DMA_BUFS buffers.
    // Let it drain (on underflow it sends zeros, matching the ramp's end), then release everything.
    const uint32_t dmaMs = 1000u * DMA_BUFS * DMA_LEN / (uint32_t)s_outRate;
    vTaskDelay(pdMS_TO_TICKS(dmaMs + 20));
    i2s_driver_uninstall(PORT);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
}

}
