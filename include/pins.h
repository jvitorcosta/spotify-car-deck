#pragma once
// ESP32-2432S028R "Cheap Yellow Display" pin map.

// Display (ILI9341) — configured via TFT_eSPI build flags; mirrored here for clarity.
// Touch (XPT2046) — SEPARATE SPI bus (HSPI).
static const int TOUCH_SCLK = 25;
static const int TOUCH_MISO = 39;
static const int TOUCH_MOSI = 32;
static const int TOUCH_CS   = 33;
static const int TOUCH_IRQ  = 36;

// SD card (VSPI)
static const int SD_CS   = 5;
static const int SD_SCK  = 18;
static const int SD_MISO = 19;
static const int SD_MOSI = 23;

// Backlight
static const int PIN_BL = 21;

