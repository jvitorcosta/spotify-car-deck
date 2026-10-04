#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "pins.h"

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[boot] hello-screen");

    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);

    tft.init();
    tft.setRotation(1);               // landscape 320x240
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 320, 80, TFT_RED);
    tft.fillRect(0, 80, 320, 80, TFT_GREEN);
    tft.fillRect(0, 160, 320, 80, TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("PokeDeck hello", 10, 110, 4);

    touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
    Serial.println("[boot] display + touch initialized");
}

void loop() {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        Serial.printf("[touch] raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
        tft.fillCircle(map(p.x, 200, 3900, 0, 320),
                       map(p.y, 200, 3900, 0, 240), 4, TFT_WHITE);
        delay(50);
    }
}
