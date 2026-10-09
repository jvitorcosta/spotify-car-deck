// Standalone hardware check for the Freenove FNK0104B (NOT the firmware).
//   pio run -e hwcheck_s3 -t upload --upload-port COM12
// Proves: display pins, colour order and inversion, landscape rotation, backlight, PSRAM and
// flash size, and the audio path (ES8311, I2S, amplifier enable) with a 1 kHz tone.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cmath>
#include "audio/sink.h"
#include "board/board.h"
#include "pins.h"

static TFT_eSPI s_tft;

static void tone1k(uint32_t ms, int volume) {
    if (!audiosink::open(16000)) return;   // the sink logs why
    int16_t buf[160];
    for (uint32_t n = 0; n < 16 * ms; n += 160) {
        for (int i = 0; i < 160; ++i)
            buf[i] = (int16_t)(sinf(6.2831853f * 1000.0f * (float)(n + i) / 16000.0f) * 32767.0f * volume / 100);
        audiosink::write(buf, 160);
    }
    audiosink::close();
}

void setup() {
    Serial.begin(115200);
    delay(1500);                            // native USB: time for the PC to open the port
    Serial.println("\n[hwcheck_s3] start");
    board::begin();

    s_tft.init();
    s_tft.invertDisplay(true);
    s_tft.setRotation(1);                   // landscape 320x240
    s_tft.fillRect(0, 0, 320, 80, TFT_RED);
    s_tft.fillRect(0, 80, 320, 80, TFT_GREEN);
    s_tft.fillRect(0, 160, 320, 80, TFT_BLUE);
    s_tft.setTextColor(TFT_WHITE);
    s_tft.drawString("RED  (top)", 8, 8, 4);
    s_tft.drawString("GREEN", 8, 88, 4);
    s_tft.drawString("BLUE (bottom)", 8, 168, 4);
    char psram[40];
    snprintf(psram, sizeof psram, "PSRAM %u KB", (unsigned)(ESP.getPsramSize() / 1024));
    s_tft.drawString(psram, 170, 210, 2);

    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_BL, 0);
    for (int lvl : {255, 96, 16, 255}) {
        ledcWrite(0, lvl);
        Serial.printf("[hwcheck_s3] backlight %d\n", lvl);
        delay(700);
    }

    Serial.printf("[hwcheck_s3] tone 1 kHz, amp on = %s\n", AMP_ON ? "HIGH" : "LOW");
    tone1k(2000, 30);
    Serial.println("[hwcheck_s3] done");
}

// Repeats the report: native USB drops the port while the board resets, so a capture opened
// afterwards misses setup()'s lines.
void loop() {
    Serial.printf("[hwcheck_s3] PSRAM %u KB (free %u KB), flash %u MB, amp on = %s\n",
                  (unsigned)(ESP.getPsramSize() / 1024), (unsigned)(ESP.getFreePsram() / 1024),
                  (unsigned)(ESP.getFlashChipSize() >> 20), AMP_ON ? "HIGH" : "LOW");
    delay(3000);
}
