// Standalone hardware-check sketch (NOT part of the firmware).
// Build/flash with:  .devtools\pio.ps1 run -e hwcheck -t upload
// Proves: display init, colors, landscape orientation, backlight, and touch.
// Excluded from the esp32dev firmware build via build_src_filter.
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "pins.h"

static TFT_eSPI s_tft = TFT_eSPI();
static SPIClass touchSPI(HSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[hwcheck] start");

    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);

    s_tft.init();
    s_tft.invertDisplay(true);          // CYD panel: colors are inverted without this
    s_tft.setRotation(1);               // landscape 320x240
    s_tft.fillScreen(TFT_BLACK);
    s_tft.fillRect(0, 0, 320, 80, TFT_RED);
    s_tft.fillRect(0, 80, 320, 80, TFT_GREEN);
    s_tft.fillRect(0, 160, 320, 80, TFT_BLUE);
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    s_tft.drawString("PokeDeck hello", 10, 110, 4);

    touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
    Serial.println("[hwcheck] display + touch initialized");
}

void loop() {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        Serial.printf("[touch] raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
        s_tft.fillCircle(map(p.x, 200, 3900, 0, 320),
                       map(p.y, 200, 3900, 0, 240), 4, TFT_WHITE);
        delay(50);
    }
}
