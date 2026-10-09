#pragma once
// Pin maps per board (BOARD_CYD / BOARD_S3 come from platformio.ini). The displays are set up by
// TFT_eSPI build flags; their pins are listed there.

#if defined(BOARD_S3)
// Freenove FNK0104B: ESP32-S3R8, 2.8" ILI9341 SPI, ES8311 codec
// (github.com/Freenove/Freenove_ESP32_S3_Display).
static const int PIN_BL = 45;                       // active HIGH (BSS138 low-side switch)
// Audio: ES8311 at I2C 0x18 (the bus is shared with the FT6336U touch at 0x38) + amplifier
static const int PIN_I2C_SDA = 16;
static const int PIN_I2C_SCL = 15;
static const int PIN_I2S_MCLK = 4;
static const int PIN_I2S_BCLK = 5;
static const int PIN_I2S_WS = 7;
static const int PIN_I2S_DOUT = 8;
static const int PIN_I2S_DIN = 6;
static const int PIN_AMP_EN = 1;
static const int AMP_ON = 0;                        // LOW plays (verified with hwcheck_s3, 2026-10-09)
// SD card (SDMMC 4-bit), unused for now: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47.

#elif defined(BOARD_CYD)
// ESP32-2432S028R "Cheap Yellow Display".
// Touch (XPT2046) — SEPARATE SPI bus (HSPI); only hwcheck uses it.
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

#else
#error "No board: build with -D BOARD_CYD or -D BOARD_S3 (platformio.ini)"
#endif
