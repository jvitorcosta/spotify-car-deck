#include "../../audio/sink.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "es8311.h"
#include "pins.h"

namespace audiosink {

// Clean codec path (no 8-bit DAC distortion): conservative start, tuned by ear (spec section 3).
extern const int GREETING_VOLUME = 100;
extern const int FINALE_VOLUME = 100;

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr int DMA_BUFS = 4;
constexpr int DMA_LEN = 256;
constexpr int CHUNK = 128;                 // mono samples per i2s_write
// Codec DAC gain (0xBF = 0 dB, 0.5 dB steps). The clips peak near full scale, so each dB above
// 0 shaves the loudest peaks; +6 dB was the owner's pick by ear after 100 % volume.
constexpr uint8_t CODEC_GAIN = 0xCB;
constexpr int MCLK_MULT = 384;             // Freenove's tested setting: 6.144 MHz at 16 kHz
int s_rate = 16000;
bool s_wire = false;
int16_t s_buf[2 * CHUNK];
}

bool open(int clipRate) {
    s_rate = clipRate;
    if (!s_wire) s_wire = Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = clipRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count = DMA_BUFS;
    cfg.dma_buf_len = DMA_LEN;
    cfg.tx_desc_auto_clear = true;         // silence on underflow
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[audio] i2s install failed");
        return false;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = PIN_I2S_MCLK;
    pins.bck_io_num = PIN_I2S_BCLK;
    pins.ws_io_num = PIN_I2S_WS;
    pins.data_out_num = PIN_I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    i2s_set_pin(PORT, &pins);
    if (!s_wire || !es8311::begin(clipRate, clipRate * MCLK_MULT) ||   // MCLK is running: codec can clock
        !es8311::setVolume(CODEC_GAIN)) {
        Serial.println("[audio] ES8311 not answering");
        i2s_driver_uninstall(PORT);
        return false;
    }
    pinMode(PIN_AMP_EN, OUTPUT);
    digitalWrite(PIN_AMP_EN, AMP_ON);      // only now: codec up, I2S sending silence (no pop)
    return true;
}

void write(const int16_t* s, size_t n) {
    while (n) {
        const size_t k = n < (size_t)CHUNK ? n : (size_t)CHUNK;
        for (size_t i = 0; i < k; ++i) s_buf[2 * i] = s_buf[2 * i + 1] = s[i];   // mono on both channels
        size_t written;
        i2s_write(PORT, s_buf, k * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        s += k;
        n -= k;
    }
}

void close() {
    const uint32_t dmaMs = 1000u * DMA_BUFS * DMA_LEN / (uint32_t)s_rate;
    vTaskDelay(pdMS_TO_TICKS(dmaMs + 20));  // let the queued DMA buffers play out
    digitalWrite(PIN_AMP_EN, !AMP_ON);
    i2s_driver_uninstall(PORT);
}

}
