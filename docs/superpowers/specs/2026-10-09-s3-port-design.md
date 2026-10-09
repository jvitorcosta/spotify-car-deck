# Second board: Freenove ESP32-S3 2.8" (FNK0104B) — design

Date: 2026-10-09. Status: approved in chat, awaiting spec review.

## Goal

Run the deck on the Freenove FNK0104B (ESP32-S3R8, 8 MB octal PSRAM, 16 MB flash, ILI9341
2.8" SPI, ES8311 codec) **with feature parity**, while the ESP32-2432S028R "CYD" keeps building
and working from the same code. New S3-only features (touch, RGB LED, PSRAM-backed extras,
SDMMC cache) are out of scope; each becomes its own small project later.

What the S3 gets for free: clean codec audio (no 8-bit DAC workarounds), a large app partition
and PSRAM headroom for TLS.

## Decisions (from the brainstorm)

- Both boards supported (two build targets), not an S3-only fork.
- Parity first; extras later.
- Same toolchain for both: PlatformIO `espressif32@6.9.0` (arduino-esp32 2.0.17). Freenove's
  examples use core 3.x (`ESP_I2S.h`); we don't — moving cores is a separate, larger upgrade.
- Board-specific code lives in per-board files picked at build time (`build_src_filter`), behind
  small shared interfaces; shared code has no `#ifdef BOARD_...`. The only exception is
  `include/pins.h`, which selects a pin block.
- `feat/boot-scene` was squash-merged into `main` (19b19a1); this work is on `feat/s3-port`.

## Hardware facts (Freenove repo github.com/Freenove/Freenove_ESP32_S3_Display, FNK0104AB setup)

| Part | Pins / facts |
|---|---|
| Display ILI9341V, 4-wire SPI | MOSI 11, SCLK 12, CS 10, DC 46, MISO 13, RST -1 (tied to EN), `ILI9341_2_DRIVER`, BGR, inversion on, 40 MHz in their setup, `USE_HSPI_PORT` |
| Backlight | GPIO 45, active HIGH (BSS138 low-side switch) |
| Audio | ES8311 at I2C 0x18 on SDA 16 / SCL 15 (400 kHz); I2S MCLK 4, BCLK 5, WS 7, DOUT 8, DIN 6; amp (SC8002B/FM8002E) enable GPIO 1, driven LOW in their example (polarity inferred — verify) |
| Touch FT6336U | I2C 0x38, same bus (16/15), RST 18, INT 17 — unused here |
| SD | SDMMC 4-bit: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 — unused here |
| Other | WS2812B GPIO 42, BOOT key GPIO 0, battery ADC GPIO 9 (÷2) — unused |
| USB | native USB-Serial/JTAG (303A:1001, COM12 here); no UART bridge |

## Design

### 1. Build targets (`platformio.ini`)

- A shared `[common]` section: platform, framework, `lib_deps`, `embed_files`, `extra_scripts`,
  shared `build_flags` (C++17, debug level, fonts, `USER_SETUP_LOADED`, `ILI9341_2_DRIVER`,
  `TFT_WIDTH/HEIGHT`, `USE_HSPI_PORT`), `build_src_flags`, `build_unflags`, monitor settings.
- `[env:cyd]` — today's `esp32dev` renamed: `board = esp32dev`, `huge_app.csv`, CYD TFT pins and
  55 MHz SPI as today, `-D BOARD_CYD`, `build_src_filter` excluding `board/s3/` and the hwcheck
  sketches. `default_envs = cyd`.
- `[env:s3]`: `board = esp32-s3-devkitc-1`, `board_build.arduino.memory_type = qio_opi`,
  `board_build.flash_mode = qio`, `board_upload.flash_size = 16MB`, a committed
  `partitions_s3.csv` (nvs, otadata-free single `factory` app of 8 MB, the rest spiffs — unused),
  `-D BOARD_S3 -D BOARD_HAS_PSRAM -D ARDUINO_USB_MODE=1 -D ARDUINO_USB_CDC_ON_BOOT=1`, Freenove
  TFT pins at `SPI_FREQUENCY=40000000` (raise later only if measured stable), `TFT_BL=45`,
  `build_src_filter` excluding `board/cyd/` and the hwcheck sketches.
- `[env:hwcheck]` (CYD, unchanged behaviour) and new `[env:hwcheck_s3]` (see §6).
- `[env:native]` unchanged.
- Every reference to `esp32dev` in README, `.devtools/pio.ps1` comments and tools becomes `cyd`.
  (Old `.superpowers/sdd/` ledgers are history and are left alone.)

### 2. Pins (`include/pins.h`)

One file, two blocks under `#if defined(BOARD_S3)` / `#else`: backlight, SD (CYD SPI pins; S3
SDMMC pins listed for later), audio (S3 I2S + I2C + amp enable; CYD DAC channel), touch (CYD
XPT2046 for hwcheck). A missing board define is a compile error.

### 3. Audio

`src/audio/greeting.cpp` keeps everything board-independent: the embedded clips (greeting,
opener, finale), per-clip volume, the 20 ms ramps, the playback task, `playing()`, `level()`,
`durationMs()` and friends. It no longer touches I2S/DAC registers. It feeds a sink:

```cpp
// src/audio/sink.h — one implementation per board, compiled by build_src_filter
namespace audiosink {
bool open(int clipRate);                       // false: no audio this time (logged)
void write(const int16_t* s, size_t n);        // clip-rate samples, volume already applied
void close();                                  // drain, silence, release
}
```

- `src/board/cyd/sink_dac.cpp`: today's code moved as-is — I2S0 built-in DAC on GPIO 26 at 2x
  the clip rate (IDF 4.4 divider wrap), midpoint upsampling, first-order noise shaping to 8 bits,
  `tx_desc_auto_clear`, DMA drain before uninstall. Output must be bit-identical to today.
- `src/board/s3/sink_es8311.cpp`: I2S standard (legacy `driver/i2s.h`, I2S_NUM_0) master TX at the
  clip rate (16 kHz), 16-bit, MCLK = 256·fs on GPIO 4, BCLK 5, WS 7, DOUT 8;
  `tx_desc_auto_clear`. On open: `Wire.begin(16, 15, 400000)`, ES8311 init for DAC playback with
  MCLK from the pin, set the codec volume register, enable the amp (GPIO 1). On close: drain,
  amp off, I2S uninstall.
- `src/board/s3/es8311.{h,cpp}`: minimal register init (reset, clock manager for the MCLK/fs
  ratio, serial port 16-bit I2S, DAC path on, ADC off, volume), adapted from Espressif's
  `esp-adf`/`esp_codec_dev` ES8311 driver (Apache-2.0) with attribution in the file header.
- Volumes: `VOLUME` / `FINALE_VOLUME` move to per-board constants exposed by the sink
  (`audiosink::GREETING_VOLUME`, `audiosink::FINALE_VOLUME`); CYD keeps 30 / 12. S3 starts at a
  conservative value and is tuned by ear with the owner.
- Unknown verified on hardware: the amp-enable polarity (LOW per Freenove's example).

### 4. SD cache

`src/images/cache.cpp`'s SD mount moves behind the board split: CYD keeps the SPI mount; on S3
`cache::begin()` reports no card and every cache call is a miss (parity with today's no-card
setup). SDMMC support is a later extra.

### 5. Memory

PSRAM is enabled (`BOARD_HAS_PSRAM`); with core 2.0.17 large `malloc`s may land in PSRAM
automatically, which is fine for TLS buffers. `mem::` keeps measuring internal 8-bit heap (the
scarce kind). The CYD-driven heap discipline stays — harmless on S3, still needed on the CYD.
The boot log adds PSRAM size/free on S3.

### 6. Hardware check for the S3 (`src/hwcheck_s3.cpp`, `env:hwcheck_s3`)

First thing flashed to the new board, before the app: colour bars + orientation text (proves
pins, BGR, inversion, rotation), backlight steps, serial report of chip, flash size, PSRAM
size/free, then a 1 kHz tone for 2 s through the S3 sink (proves codec, I2S, amp polarity).
If the tone is silent with the amp LOW, try HIGH and record the finding in `pins.h`.

### 7. Docs

README: a "Boards" section (both boards, build targets, flashing the S3 over its native USB, the
hwcheck targets), updated build commands.

## Testing and acceptance

- Host tests: `tools/run_tests.ps1` stays green (pure code is unchanged; no board code in it).
- Both targets build: `pio run -e cyd`, `pio run -e s3`, `pio run -e hwcheck`,
  `pio run -e hwcheck_s3`.
- CYD regression (end of the port): boot scene + greeting/opener/finale timings as in today's
  logs (greeting ~6.4 s, opener at montage start, finale ends with the montage), Spotify polls,
  idle heap ~108 KB.
- S3 acceptance checklist:
  1. hwcheck_s3: correct colours/orientation, backlight, PSRAM 8 MB reported, tone audible.
  2. Boot scene renders and runs at least as fast as the CYD (log `render avg`), BIOS terminal,
     montage + end card.
  3. Greeting, opener and finale audible and clean, timings as on the CYD.
  4. Spotify, lyrics, genre badge, walker sprites, album art.
  5. 10 minutes of playback: zero `allocfail`, no restarts.

## Out of scope

Touch input, RGB LED, battery reading, SDMMC cache, PSRAM-backed features, arduino-esp32 3.x,
display SPI above 40 MHz unless measured stable during the port.

## Risks

- Amp-enable polarity and ES8311 init details: covered by hwcheck_s3 before the app.
- `esp32-s3-devkitc-1` + `qio_opi` on platform 6.9.0: verified by hwcheck_s3 (PSRAM size).
- Native-USB serial resets/enumeration differ from the CYD's UART bridge: `tools/capture_serial.py`
  may need to open the port without toggling DTR/RTS; checked in the hwcheck step.
