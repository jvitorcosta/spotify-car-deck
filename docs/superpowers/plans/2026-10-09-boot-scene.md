# Boot Scene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** At boot, while the greeting plays and WiFi connects, show a 2D night-drive scene: the owner's silver Honda City sedan drives in, cruises for at least 5 s and until WiFi is up, then drives off; its lights pulse with the sound.

**Architecture:** Pure maths in `util/bootanim` (motion, light levels, colour blending, clipping) and `util/loudness` (clip loudness), a generated car sprite in `ui/car_sprite` (from `tools/gen_car_sprite.py`), and the renderer `ui/bootscene`, which runs in its own FreeRTOS task: the sky is drawn once, the lower 144 rows are re-rendered every frame through one 320 × 24 `TFT_eSprite` strip. `setup()` connects WiFi without touching the display and waits for the scene; `loop()` and the network task's late-WiFi path wait while it is active.

**Tech Stack:** ESP32 Arduino (arduino-esp32 2.0.17 / ESP-IDF 4.4), TFT_eSPI 2.5.43, FreeRTOS, Unity host tests compiled with g++ (`tools/run_tests.ps1`), Python 3 standard library for generators.

**Spec:** `docs/superpowers/specs/2026-10-09-boot-scene-design.md`

## Global Constraints

- Board: ESP32-2432S028R, **no PSRAM**; byte heap is tight once TLS runs. The scene's strip sprite is 320 × 24 × 16-bit = 15 360 bytes, allocated at scene start and freed at scene end.
- TFT_eSPI is not thread-safe: while `bootscene::active()`, only the scene task draws.
- `TFT_eSprite` 16-bit buffers store **byte-swapped** RGB565; write pixels through `theme::be()` (`src/ui/theme.h:62`).
- Pure modules (`util/*`, `ui/car_sprite`) must compile on the host: no Arduino headers.
- Timing constants (spec): enter 900 ms, exit 900 ms, minimum show 5000 ms, sway ±6 px over 4000 ms, road/streetlights 90 px/s, skyline 14 px/s, spokes 9 rad/s, underglow hue 120°/s, 25 fps target.
- Car: 200 × 66 px sprite, ground line y = 214 on screen, cruise x = 60, start x = -202, end x = 330; wheel centres sprite x = 46 / 160.
- Commits: Conventional Commits `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Host tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` (all suites, currently 25 pass). One suite: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\<suite>\<suite>.cpp <module.cpp ...>`.
- Firmware build: `pio run -e esp32dev`; flash: `pio run -e esp32dev -t upload --upload-port COM11`; the deck is on COM11.

## Review Focus

1. **Hotspot off at boot, turned on later**: the car keeps cruising with "WiFi not found - retrying...", leaves once WiFi connects, the deck appears and Spotify connects (heap freed first). Pinned by Task 6, Step 6.
2. **Car partly off-screen** (entering at x < 0, leaving past 320): nothing may be written outside the 320 × 24 strip. Pinned by the `clip` tests in Task 3 and the edge-frame check in Task 6, Step 5.
3. **Caption change mid-scene**: the sky is redrawn with the new caption and no leftover characters of the old one. Pinned by Task 6, Step 6.
4. **WiFi connects very fast**: the car still stays ≥ 5 s and setup draws nothing until it leaves. Pinned by the `mayExit` tests in Task 3 and the timing log in Task 6, Step 5.
5. **Strip allocation fails**: black screen with the caption, same exit rule, the sound still plays. Pinned by Task 5, Step 4.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/util/loudness.h/.cpp` (new) | Peak loudness 0–255 of a 16-bit PCM window. Pure. |
| `src/audio/greeting.h/.cpp` (modify) | Add `durationMs()` and `level(ms)` for the scene. |
| `src/util/bootanim.h/.cpp` (new) | Scene maths: car x per phase, exit gating, bob, spokes, scrolling, star twinkle, RGB565 helpers, light levels, reflection, clipping. Pure. |
| `tools/gen_car_sprite.py` (new) | Builds the car from geometry, writes `src/ui/car_sprite.inc`. |
| `src/ui/car_sprite.h/.cpp/.inc` (new) | The generated car: colour + pixel kind per pixel. Pure. |
| `src/ui/bootscene.h/.cpp` (new) | Scene task: sky, strips, layers, caption, lifecycle. Firmware only. |
| `src/main.cpp` (modify) | Boot sequence, `loop()` waits while the scene is active. |
| `src/core/nettask.cpp` (modify) | Late-WiFi path waits for the scene before `spclient::begin()`. |
| `test/test_loudness`, `test/test_bootanim`, `test/test_car_sprite` (new) | Host suites. |
| `README.md` (modify) | Boot scene feature and architecture entries. |

---

### Task 1: Commit the greeting feature

The greeting (boot sound) is implemented and was verified on the deck, but it is uncommitted. Commit it on its own before the scene builds on it.

**Files:**
- Commit (already in the working tree): `src/audio/greeting.h`, `src/audio/greeting.cpp`, `tools/gen_greeting.py`, `tools/greeting_clip.py`, `data/greeting_default.pcm`, `platformio.ini`, `.gitignore`, `src/main.cpp`, `README.md`
- Must stay uncommitted: `data/greeting.pcm` (the owner's clip; git-ignored)

**Interfaces:**
- Produces: `greeting::play()` (`src/audio/greeting.h`); constants in `src/audio/greeting.cpp`: `CLIP_RATE = 16000`, `OUT_RATE = 32000`, `VOLUME = 10`; embedded clip symbols `_binary_data_greeting_pcm_start/_end` (16-bit signed LE mono).

- [ ] **Step 1: Check the working tree holds exactly the greeting change**

Run: `git status --short`
Expected:
```
 M .gitignore
 M README.md
 M platformio.ini
 M src/main.cpp
?? data/greeting_default.pcm
?? src/audio/
?? tools/gen_greeting.py
?? tools/greeting_clip.py
```
(`data/greeting.pcm` must not appear: it is git-ignored.)

- [ ] **Step 2: Build the firmware and the hardware check**

Run: `pio run -e esp32dev` then `pio run -e hwcheck`
Expected: both end with `SUCCESS`; no warnings from `src/audio/greeting.cpp`.

- [ ] **Step 3: Run the host tests**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: `suites pass=25 fail=0`

- [ ] **Step 4: Commit**

```bash
git add .gitignore README.md platformio.ini src/main.cpp data/greeting_default.pcm src/audio tools/gen_greeting.py tools/greeting_clip.py
git commit -m "feat(audio): boot greeting on the SPEAK connector

Plays an embedded clip at boot through the built-in DAC (GPIO 26): 16-bit
16 kHz clips, played at 32 kHz (IDF 4.4 DAC mode mis-clocks below ~20 kHz),
volume applied before noise-shaped reduction to 8 bits. Default chime is
committed; tools/gen_greeting.py converts the owner's own WAV to
data/greeting.pcm (git-ignored), which replaces it at build time.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Loudness and the greeting's level API

**Files:**
- Create: `src/util/loudness.h`, `src/util/loudness.cpp`, `test/test_loudness/test_loudness.cpp`
- Modify: `src/audio/greeting.h`, `src/audio/greeting.cpp`

**Interfaces:**
- Produces: `uint8_t loudness(const uint8_t* pcm16le, size_t samples, size_t at, size_t window)` in namespace `loudness`; `uint32_t greeting::durationMs()`; `uint8_t greeting::level(uint32_t ms)`.

- [ ] **Step 1: Write the failing test**

`test/test_loudness/test_loudness.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/loudness.h"

void setUp() {}
void tearDown() {}

// Little-endian 16-bit samples into a byte buffer starting at `out` (may be unaligned).
static void put(uint8_t* out, const int16_t* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = (uint8_t)(s[i] & 0xFF);
        out[2 * i + 1] = (uint8_t)((uint16_t)s[i] >> 8);
    }
}

void test_silence_is_zero() {
    const int16_t s[4] = {0, 0, 0, 0};
    uint8_t b[8];
    put(b, s, 4);
    TEST_ASSERT_EQUAL_UINT8(0, loudness::loudness(b, 4, 0, 4));
}
void test_full_scale_is_255_both_polarities() {
    const int16_t pos[2] = {0, 32767}, neg[2] = {-32768, 0};
    uint8_t b[4];
    put(b, pos, 2);
    TEST_ASSERT_EQUAL_UINT8(255, loudness::loudness(b, 2, 0, 2));
    put(b, neg, 2);
    TEST_ASSERT_EQUAL_UINT8(255, loudness::loudness(b, 2, 0, 2));
}
void test_half_scale_is_128() {
    const int16_t s[1] = {16384};
    uint8_t b[2];
    put(b, s, 1);
    TEST_ASSERT_EQUAL_UINT8(128, loudness::loudness(b, 1, 0, 1));
}
// Only [at, at + window) counts: the loud sample outside the window is ignored.
void test_window_selects_samples() {
    const int16_t s[6] = {32767, 0, 1000, 0, 0, 0};
    uint8_t b[12];
    put(b, s, 6);
    TEST_ASSERT_EQUAL_UINT8(8, loudness::loudness(b, 6, 1, 3));   // peak 1000 -> 8
}
// A window running past the end is clamped; a start past the end gives 0.
void test_window_clamped_at_the_clip_end() {
    const int16_t s[3] = {0, 0, -32767};
    uint8_t b[6];
    put(b, s, 3);
    TEST_ASSERT_EQUAL_UINT8(255, loudness::loudness(b, 3, 2, 320));
    TEST_ASSERT_EQUAL_UINT8(0, loudness::loudness(b, 3, 3, 320));
    TEST_ASSERT_EQUAL_UINT8(0, loudness::loudness(b, 3, 99, 320));
}
// Embedded data has no alignment guarantee: an odd start address must read the same.
void test_odd_alignment() {
    const int16_t s[2] = {0, 16384};
    uint8_t raw[5];
    put(raw + 1, s, 2);
    TEST_ASSERT_EQUAL_UINT8(128, loudness::loudness(raw + 1, 2, 0, 2));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_silence_is_zero);
    RUN_TEST(test_full_scale_is_255_both_polarities);
    RUN_TEST(test_half_scale_is_128);
    RUN_TEST(test_window_selects_samples);
    RUN_TEST(test_window_clamped_at_the_clip_end);
    RUN_TEST(test_odd_alignment);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_loudness\test_loudness.cpp`
Expected: `COMPILE FAILED` (`loudness.h` missing).

- [ ] **Step 3: Implement**

`src/util/loudness.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
// Loudness of a 16-bit signed little-endian PCM clip, for the boot scene's lights. PURE,
// host-tested. Reads bytes, so the clip may sit at any address (embedded files are unaligned).
namespace loudness {
// Peak absolute sample in [at, at + window), clamped to the clip, scaled to 0-255
// (32767 -> 255). 0 when `at` is past the end.
uint8_t loudness(const uint8_t* pcm16le, size_t samples, size_t at, size_t window);
}
```

`src/util/loudness.cpp`:
```cpp
#include "loudness.h"

namespace loudness {

uint8_t loudness(const uint8_t* pcm16le, size_t samples, size_t at, size_t window) {
    if (at >= samples) return 0;
    const size_t end = (window > samples - at) ? samples : at + window;
    int32_t peak = 0;
    for (size_t i = at; i < end; ++i) {
        const int16_t s = (int16_t)(pcm16le[2 * i] | (pcm16le[2 * i + 1] << 8));
        const int32_t a = s < 0 ? -(int32_t)s : s;
        if (a > peak) peak = a;
    }
    const int32_t v = (peak * 255 + 16383) / 32767;
    return (uint8_t)(v > 255 ? 255 : v);
}

}
```

- [ ] **Step 4: Run the suite to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_loudness\test_loudness.cpp src\util\loudness.cpp`
Expected: `6 Tests 0 Failures 0 Ignored`

- [ ] **Step 5: Add the level API to the greeting**

In `src/audio/greeting.h`, replace the namespace block:
```cpp
namespace greeting {
void play();
}
```
with:
```cpp
namespace greeting {
void play();
uint32_t durationMs();          // clip length (16 kHz)
uint8_t level(uint32_t ms);     // clip loudness 0-255 over 20 ms at `ms`; 0 past the end. Ignores VOLUME.
}
```
and add `#include <cstdint>` under `#pragma once`.

In `src/audio/greeting.cpp`:
1. Add `#include "../util/loudness.h"` after `#include <driver/i2s.h>`.
2. After the `constexpr int VOLUME = ...;` line, add:
```cpp

static const uint8_t* clipData() { return _binary_data_greeting_pcm_start; }
static size_t clipSamples() {
    return (size_t)(_binary_data_greeting_pcm_end - _binary_data_greeting_pcm_start) / 2;
}
```
3. In `task()`, replace the two lines
```cpp
    const uint8_t* clip = _binary_data_greeting_pcm_start;
    const size_t len = (size_t)(_binary_data_greeting_pcm_end - _binary_data_greeting_pcm_start) / 2;
```
with
```cpp
    const uint8_t* clip = clipData();
    const size_t len = clipSamples();
```
4. After `void play() { ... }`, add:
```cpp

uint32_t durationMs() { return (uint32_t)((uint64_t)clipSamples() * 1000 / CLIP_RATE); }

uint8_t level(uint32_t ms) {
    const size_t at = (size_t)((uint64_t)ms * CLIP_RATE / 1000);
    return loudness::loudness(clipData(), clipSamples(), at, CLIP_RATE / 50);
}
```

- [ ] **Step 6: Build and run all host tests**

Run: `pio run -e esp32dev` → `SUCCESS`.
Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` → `suites pass=26 fail=0`.

- [ ] **Step 7: Commit**

```bash
git add src/util/loudness.h src/util/loudness.cpp test/test_loudness src/audio/greeting.h src/audio/greeting.cpp
git commit -m "feat(audio): greeting loudness for the boot scene

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Boot scene maths (`util/bootanim`)

One deviation from the spec's table, on purpose: the cruise sway uses `sin(2π (t − 900) / 4000)` (phase starting at the end of the enter phase) instead of `sin(2π t / 4000)`; the spec's formula jumps 6 px when the enter phase hands over to the cruise.

**Files:**
- Create: `src/util/bootanim.h`, `src/util/bootanim.cpp`, `test/test_bootanim/test_bootanim.cpp`

**Interfaces:**
- Produces (namespace `bootanim`): constants `ENTER_MS, EXIT_MS, MIN_SHOW_MS, SWAY_MS` (`uint32_t`), `START_X, CRUISE_X, END_X, SWAY_PX` (`int`), `NO_EXIT` (`uint32_t` 0xFFFFFFFF); `constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)`; `constexpr uint16_t hex565(uint32_t rgb)`; `int cruiseX(uint32_t t)`; `int carX(uint32_t t, uint32_t tExit)`; `bool exitDone(uint32_t t, uint32_t tExit)`; `bool mayExit(uint32_t t, bool soundDone, bool online)`; `int bob(uint32_t t)`; `float spokeAngle(uint32_t t)`; `int scroll(uint32_t t, int pxPerS, int period)`; `bool starHidden(uint32_t t, int i)`; `uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha)`; `uint16_t scale565(uint16_t c, uint8_t level)`; `uint16_t hue565(uint32_t t)`; `uint16_t headColor(uint8_t e)`; `uint8_t tailLevel(uint8_t e)`; `uint8_t tailGlowAlpha(uint8_t e)`; `uint8_t beamAlpha(uint8_t e)`; `uint8_t glowAlpha(uint8_t e)`; `uint8_t reflectAlpha(int d, bool upper)`; `bool clip(int& a, int& b, int lo, int hi)`.

- [ ] **Step 1: Write the failing test**

`test/test_bootanim/test_bootanim.cpp`:
```cpp
#include <unity.h>
#include "../../src/util/bootanim.h"

using namespace bootanim;

void setUp() {}
void tearDown() {}

void test_enter_eases_from_off_screen_to_cruise() {
    TEST_ASSERT_EQUAL_INT(-202, carX(0, NO_EXIT));
    TEST_ASSERT_EQUAL_INT(-5, carX(450, NO_EXIT));     // -202 + round(262 * 0.75)
    TEST_ASSERT_EQUAL_INT(60, carX(900, NO_EXIT));
}
// The sway starts at phase 0 when the enter phase ends: no jump at the hand-over.
void test_cruise_sways_around_the_centre() {
    TEST_ASSERT_EQUAL_INT(60, carX(900, NO_EXIT));
    TEST_ASSERT_EQUAL_INT(66, carX(900 + 1000, NO_EXIT));
    TEST_ASSERT_EQUAL_INT(54, carX(900 + 3000, NO_EXIT));
    for (uint32_t t = 900; t < 60000; t += 37) {
        int x = carX(t, NO_EXIT);
        TEST_ASSERT_TRUE(x >= 54 && x <= 66);
    }
}
void test_exit_accelerates_off_screen_from_the_cruise_x() {
    const uint32_t tExit = 900 + 1000;                   // cruise x = 66 here
    TEST_ASSERT_EQUAL_INT(66, carX(tExit, tExit));
    TEST_ASSERT_EQUAL_INT(132, carX(tExit + 450, tExit));   // 66 + 264 * 0.25
    TEST_ASSERT_EQUAL_INT(330, carX(tExit + 900, tExit));
    TEST_ASSERT_EQUAL_INT(330, carX(tExit + 5000, tExit));
    TEST_ASSERT_FALSE(exitDone(tExit + 899, tExit));
    TEST_ASSERT_TRUE(exitDone(tExit + 900, tExit));
    TEST_ASSERT_FALSE(exitDone(tExit + 900, NO_EXIT));
}
// Exit only after 5 s, once the sound has ended, and while online (Review Focus 4).
void test_exit_gating() {
    TEST_ASSERT_FALSE(mayExit(4999, true, true));
    TEST_ASSERT_TRUE(mayExit(5000, true, true));
    TEST_ASSERT_FALSE(mayExit(6000, false, true));
    TEST_ASSERT_FALSE(mayExit(6000, true, false));
    TEST_ASSERT_TRUE(mayExit(600000, true, true));
}
void test_bob_every_fifth_sixth_of_a_second() {
    TEST_ASSERT_EQUAL_INT(1, bob(0));
    TEST_ASSERT_EQUAL_INT(0, bob(200));
    TEST_ASSERT_EQUAL_INT(1, bob(840));
}
void test_scroll_and_spokes() {
    TEST_ASSERT_EQUAL_INT(90, scroll(1000, 90, 160));
    TEST_ASSERT_EQUAL_INT(20, scroll(2000, 90, 160));
    TEST_ASSERT_EQUAL_INT(0, scroll(0, 14, 720));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 9.0f, spokeAngle(1000));
}
void test_star_twinkle() {
    TEST_ASSERT_TRUE(starHidden(0, 0));
    TEST_ASSERT_FALSE(starHidden(0, 1));
    TEST_ASSERT_TRUE(starHidden(2000, 1));   // 2000*3/1000 + 1 = 7
}
void test_rgb565_helpers() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(255, 255, 255));
    TEST_ASSERT_EQUAL_HEX16(0xBE19, hex565(0xBCC3CB));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, blend565(0xFFFF, 0x0000, 255));
    TEST_ASSERT_EQUAL_HEX16(0x0000, blend565(0xFFFF, 0x0000, 0));
    TEST_ASSERT_EQUAL_HEX16(0x8410, blend565(0xFFFF, 0x0000, 128));
    TEST_ASSERT_EQUAL_HEX16(0x8410, scale565(0xFFFF, 128));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, scale565(0xFFFF, 255));
}
// hsl(h, 100 %, 58 %): one full cycle every 3 s.
void test_underglow_hue_cycles() {
    TEST_ASSERT_EQUAL_HEX16(rgb565(255, 41, 41), hue565(0));
    TEST_ASSERT_EQUAL_HEX16(rgb565(41, 255, 41), hue565(1000));
    TEST_ASSERT_EQUAL_HEX16(rgb565(41, 41, 255), hue565(2000));
    TEST_ASSERT_EQUAL_HEX16(hue565(0), hue565(3000));
}
void test_light_levels_follow_loudness() {
    TEST_ASSERT_EQUAL_HEX16(rgb565(160, 160, 128), headColor(0));
    TEST_ASSERT_EQUAL_HEX16(rgb565(255, 255, 204), headColor(255));
    TEST_ASSERT_EQUAL_UINT8(140, tailLevel(0));
    TEST_ASSERT_EQUAL_UINT8(255, tailLevel(255));
    TEST_ASSERT_EQUAL_UINT8(0, tailGlowAlpha(12));
    TEST_ASSERT_EQUAL_UINT8(128, tailGlowAlpha(255));
    TEST_ASSERT_EQUAL_UINT8(40, beamAlpha(0));
    TEST_ASSERT_EQUAL_UINT8(96, beamAlpha(255));
    TEST_ASSERT_EQUAL_UINT8(115, glowAlpha(0));
    TEST_ASSERT_EQUAL_UINT8(217, glowAlpha(255));
}
void test_streetlight_reflection_falls_off() {
    TEST_ASSERT_EQUAL_UINT8(140, reflectAlpha(0, true));
    TEST_ASSERT_EQUAL_UINT8(70, reflectAlpha(13, true));
    TEST_ASSERT_EQUAL_UINT8(0, reflectAlpha(26, true));
    TEST_ASSERT_EQUAL_UINT8(77, reflectAlpha(0, false));
    TEST_ASSERT_EQUAL_UINT8(0, reflectAlpha(18, false));
    TEST_ASSERT_EQUAL_UINT8(0, reflectAlpha(500, false));
}
// Partly off-screen spans are trimmed, fully off-screen ones rejected (Review Focus 2).
void test_clip() {
    int a = -202, b = -2;
    TEST_ASSERT_FALSE(clip(a, b, 0, 320));
    a = -10; b = 190;
    TEST_ASSERT_TRUE(clip(a, b, 0, 320));
    TEST_ASSERT_EQUAL_INT(0, a);
    TEST_ASSERT_EQUAL_INT(190, b);
    a = 300; b = 500;
    TEST_ASSERT_TRUE(clip(a, b, 0, 320));
    TEST_ASSERT_EQUAL_INT(320, b);
    a = 330; b = 530;
    TEST_ASSERT_FALSE(clip(a, b, 0, 320));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_enter_eases_from_off_screen_to_cruise);
    RUN_TEST(test_cruise_sways_around_the_centre);
    RUN_TEST(test_exit_accelerates_off_screen_from_the_cruise_x);
    RUN_TEST(test_exit_gating);
    RUN_TEST(test_bob_every_fifth_sixth_of_a_second);
    RUN_TEST(test_scroll_and_spokes);
    RUN_TEST(test_star_twinkle);
    RUN_TEST(test_rgb565_helpers);
    RUN_TEST(test_underglow_hue_cycles);
    RUN_TEST(test_light_levels_follow_loudness);
    RUN_TEST(test_streetlight_reflection_falls_off);
    RUN_TEST(test_clip);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_bootanim\test_bootanim.cpp`
Expected: `COMPILE FAILED`.

- [ ] **Step 3: Implement**

`src/util/bootanim.h`:
```cpp
#pragma once
#include <cstdint>
// Boot scene motion, light and colour maths (docs/superpowers/specs/2026-10-09-boot-scene-design.md).
// PURE, host-tested. `t` is ms since the scene started; colours are native RGB565.
namespace bootanim {

constexpr uint32_t ENTER_MS = 900, EXIT_MS = 900, MIN_SHOW_MS = 5000, SWAY_MS = 4000;
constexpr int START_X = -202, CRUISE_X = 60, END_X = 330, SWAY_PX = 6;
constexpr uint32_t NO_EXIT = 0xFFFFFFFFu;   // tExit while the car hasn't started leaving

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t hex565(uint32_t rgb) {
    return rgb565((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
}

int cruiseX(uint32_t t);                      // sway around CRUISE_X, phase 0 at ENTER_MS
int carX(uint32_t t, uint32_t tExit);         // car's left edge on screen
bool exitDone(uint32_t t, uint32_t tExit);    // the car has left the screen
bool mayExit(uint32_t t, bool soundDone, bool online);
int bob(uint32_t t);                          // 1 = body 1 px lower this frame
float spokeAngle(uint32_t t);                 // radians
int scroll(uint32_t t, int pxPerS, int period);   // left scroll offset in [0, period)
bool starHidden(uint32_t t, int i);

uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha);   // alpha 255 = fg
uint16_t scale565(uint16_t c, uint8_t level);                  // brightness, 255 = unchanged
uint16_t hue565(uint32_t t);                  // underglow colour: hsl(t * 120 / 1000, 100 %, 58 %)

// Lights from the clip loudness e (0-255; 0 = resting look).
uint16_t headColor(uint8_t e);                // headlight LED/DRL pixels
uint8_t tailLevel(uint8_t e);                 // taillight brightness for scale565
uint8_t tailGlowAlpha(uint8_t e);             // red glow behind the taillight (0 when e <= 12)
uint8_t beamAlpha(uint8_t e);                 // headlight beam alpha at the lamp
uint8_t glowAlpha(uint8_t e);                 // underglow alpha
// Streetlight reflection on paint `d` px (horizontally) from the lamp; upper = above the shoulder line.
uint8_t reflectAlpha(int d, bool upper);

// Trims [a, b) to [lo, hi); false when nothing is left.
bool clip(int& a, int& b, int lo, int hi);
}
```

`src/util/bootanim.cpp`:
```cpp
#include "bootanim.h"
#include <cmath>

namespace bootanim {

static constexpr float TWO_PI = 6.28318530718f;

int cruiseX(uint32_t t) {
    const uint32_t p = (t - ENTER_MS) % SWAY_MS;
    return CRUISE_X + (int)lroundf(SWAY_PX * sinf(TWO_PI * (float)p / (float)SWAY_MS));
}

int carX(uint32_t t, uint32_t tExit) {
    if (tExit != NO_EXIT) {
        const uint32_t dt = t - tExit;
        if (dt >= EXIT_MS) return END_X;
        const float q = (float)dt / (float)EXIT_MS;
        const int xc = cruiseX(tExit);
        return xc + (int)lroundf((float)(END_X - xc) * q * q);
    }
    if (t < ENTER_MS) {
        const float q = (float)t / (float)ENTER_MS;
        return START_X + (int)lroundf((float)(CRUISE_X - START_X) * (1.0f - (1.0f - q) * (1.0f - q)));
    }
    return cruiseX(t);
}

bool exitDone(uint32_t t, uint32_t tExit) { return tExit != NO_EXIT && t - tExit >= EXIT_MS; }

bool mayExit(uint32_t t, bool soundDone, bool online) {
    return t >= MIN_SHOW_MS && soundDone && online;
}

int bob(uint32_t t) { return ((uint64_t)t * 6 / 1000) % 5 == 0 ? 1 : 0; }

float spokeAngle(uint32_t t) { return (float)t * 9.0f / 1000.0f; }

int scroll(uint32_t t, int pxPerS, int period) {
    return (int)(((uint64_t)t * (uint64_t)pxPerS / 1000) % (uint64_t)period);
}

bool starHidden(uint32_t t, int i) { return ((uint64_t)t * 3 / 1000 + (uint64_t)i) % 7 == 0; }

uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
    const uint32_t a = alpha, na = 255 - alpha;
    const uint32_t r = (((fg >> 11) & 31) * a + ((bg >> 11) & 31) * na + 127) / 255;
    const uint32_t g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * na + 127) / 255;
    const uint32_t b = ((fg & 31) * a + (bg & 31) * na + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint16_t scale565(uint16_t c, uint8_t level) { return blend565(c, 0x0000, level); }

uint16_t hue565(uint32_t t) {
    const float h = (float)(((uint64_t)t * 120 / 1000) % 360) / 360.0f;
    const float l = 0.58f, q = 1.0f, p = 2.0f * l - q;   // saturation 100 %
    auto f = [&](float x) {
        x -= floorf(x);
        if (x < 1.0f / 6) return p + (q - p) * 6 * x;
        if (x < 0.5f) return q;
        if (x < 2.0f / 3) return p + (q - p) * (2.0f / 3 - x) * 6;
        return p;
    };
    return rgb565((uint8_t)lroundf(f(h + 1.0f / 3) * 255), (uint8_t)lroundf(f(h) * 255),
                  (uint8_t)lroundf(f(h - 1.0f / 3) * 255));
}

uint16_t headColor(uint8_t e) {
    const uint8_t l = (uint8_t)(160 + 95 * e / 255);
    return rgb565(l, l, (uint8_t)(l * 4 / 5));
}
uint8_t tailLevel(uint8_t e) { return (uint8_t)(140 + 115 * e / 255); }
uint8_t tailGlowAlpha(uint8_t e) { return e > 12 ? (uint8_t)(128 * e / 255) : 0; }
uint8_t beamAlpha(uint8_t e) { return (uint8_t)(40 + 56 * e / 255); }
uint8_t glowAlpha(uint8_t e) { return (uint8_t)(115 + 102 * e / 255); }

uint8_t reflectAlpha(int d, bool upper) {
    if (d < 0) d = -d;
    const int reach = upper ? 26 : 18, peak = upper ? 140 : 77;
    return d >= reach ? 0 : (uint8_t)(peak * (reach - d) / reach);
}

bool clip(int& a, int& b, int lo, int hi) {
    if (a < lo) a = lo;
    if (b > hi) b = hi;
    return a < b;
}

}
```

- [ ] **Step 4: Run the suite to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_bootanim\test_bootanim.cpp src\util\bootanim.cpp`
Expected: `12 Tests 0 Failures 0 Ignored`. If a colour assertion is off by one RGB565 step, fix the rounding in the implementation (the expected values come from the spec's formulas), not the test.

- [ ] **Step 5: Commit**

```bash
git add src/util/bootanim.h src/util/bootanim.cpp test/test_bootanim
git commit -m "feat(util): boot scene motion and light maths

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The car sprite (generator + `ui/car_sprite`)

**Files:**
- Create: `tools/gen_car_sprite.py`, `src/ui/car_sprite.h`, `src/ui/car_sprite.cpp`, `src/ui/car_sprite.inc` (generated), `test/test_car_sprite/test_car_sprite.cpp`

**Interfaces:**
- Produces (namespace `car_sprite`): `enum Kind : uint8_t { NONE = 0, UPPER = 1, LOWER = 2, OTHER = 3, HEAD = 4, TAIL = 5 }`; constants `REAR_WHEEL_X = 46`, `FRONT_WHEEL_X = 160`, `WHEEL_Y = 52` (wheel centres in sprite coordinates); `int width()`; `int height()`; `bool pixel(int x, int y, uint16_t* rgb565, uint8_t* kind)` (false = transparent or outside; then `*kind` is `NONE`).

- [ ] **Step 1: Write the failing test**

`test/test_car_sprite/test_car_sprite.cpp`:
```cpp
#include <unity.h>
#include "../../src/ui/car_sprite.h"

void setUp() {}
void tearDown() {}

static uint8_t kindAt(int x, int y, uint16_t* c = nullptr) {
    uint8_t k = 99;
    car_sprite::pixel(x, y, c, &k);
    return k;
}

void test_size() {
    TEST_ASSERT_EQUAL_INT(200, car_sprite::width());
    TEST_ASSERT_EQUAL_INT(66, car_sprite::height());
}
void test_transparent_around_the_body_and_outside() {
    TEST_ASSERT_FALSE(car_sprite::pixel(0, 0, nullptr, nullptr));       // above the trunk
    TEST_ASSERT_FALSE(car_sprite::pixel(199, 5, nullptr, nullptr));     // in front of the windshield
    TEST_ASSERT_FALSE(car_sprite::pixel(30, 60, nullptr, nullptr));     // where the rear tyre goes
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(-1, 10));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(200, 10));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(10, 66));
}
void test_body_paint() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::LOWER, kindAt(110, 40, &c));
    TEST_ASSERT_EQUAL_HEX16(0xBE19, c);                                 // #BCC3CB silver
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(120, 24, &c));
    TEST_ASSERT_EQUAL_HEX16(0xD6DC, c);                                 // #D5DBE1 above the shoulder
}
void test_lamp_kinds() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(190, 32, &c));    // DRL strip
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::TAIL, kindAt(8, 26));          // taillight LED strip
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(184, 30));       // smoked headlight housing
}
// The owner's trunk lip spoiler stands above the trunk line.
void test_trunk_lip_spoiler() {
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(5, 19));
    TEST_ASSERT_TRUE(car_sprite::pixel(5, 18, nullptr, nullptr));
    TEST_ASSERT_FALSE(car_sprite::pixel(5, 17, nullptr, nullptr));
}
void test_wheel_arch_liner_is_dark() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(46, 45, &c));
    TEST_ASSERT_EQUAL_HEX16(0x0882, c);                                 // #0E1016
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_size);
    RUN_TEST(test_transparent_around_the_body_and_outside);
    RUN_TEST(test_body_paint);
    RUN_TEST(test_lamp_kinds);
    RUN_TEST(test_trunk_lip_spoiler);
    RUN_TEST(test_wheel_arch_liner_is_dark);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_car_sprite\test_car_sprite.cpp`
Expected: `COMPILE FAILED`.

- [ ] **Step 3: Write the generator**

`tools/gen_car_sprite.py` (this exact code was prototyped and reproduces the approved mockup `city-2024-v3.html`):
```python
"""Generate src/ui/car_sprite.inc: the boot scene's car, a side view of a silver Honda City sedan
(7th gen, Brazil 2024) facing right, 200 x 66 px, built from geometry measured off reference
photos (docs/superpowers/specs/2026-10-09-boot-scene-design.md, "Car sprite").
Run: python tools/gen_car_sprite.py [--png out.png]   (output is committed)
Wheels are not in the sprite (drawn in code so they spin). Standard library only.
"""
import math
import os
import struct
import sys
import zlib

W, H = 200, 66
RWX, FWX, WCY = 46, 160, 51.5            # wheel centres (x rear, x front, y)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "ui", "car_sprite.inc")

# Pixel kinds (match ui/car_sprite.h)
NONE, UPPER, LOWER, OTHER, HEAD, TAIL = 0, 1, 2, 3, 4, 5


def hexrgb(s):
    return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))


C = {k: hexrgb(v) for k, v in dict(
    mid="#a7afb8", body="#bcc3cb", hi="#eef2f5", hi2="#d5dbe1", lo="#959ea8", shade="#7c858f",
    deep="#5d656e", dark="#22262e", glass="#16233a", glassHi="#4f6d92", trim="#2c3038",
    chrome="#e8ecef", liner="#0e1016", housing="#28323f", housingTop="#7d8fa8", led="#e8f2ff",
    drl="#ffffff", tailTop="#ff5050", tailLow="#b81414", tailLed="#ff8080", reflector="#a01010",
    amber="#ffb030").items()}


def in_poly(px, py, poly):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def arc(cx, r):
    """Wheel-arch points from the front (cx + r) to the rear (cx - r), centre (cx, WCY + 2.5)."""
    return [(cx + math.cos(math.radians(a)) * r, WCY + 2.5 - math.sin(math.radians(a)) * r)
            for a in range(0, 181, 10)]


BODY = ([(4, 51), (1, 46), (0, 38), (0, 29), (1, 24), (3, 21), (22, 19.2), (26, 19), (40, 12), (56, 5),
         (70, 2.2), (86, 1.2), (100, 1), (112, 2), (120, 4), (134, 13), (144, 20), (150, 21.5),
         (172, 25), (188, 28), (196, 30.5), (199, 34), (200, 40), (199, 47), (196, 52), (186, 54),
         (178, 54)] + arc(FWX, 17.5) + [(64, 55)] + arc(RWX, 17.5) + [(12, 52)])
GLASS = [(48, 20.6), (60, 9.5), (72, 4.6), (86, 3.4), (100, 3.2), (111, 4), (119, 6.2), (133, 15.6),
         (139, 20.3)]
TAIL_POLY = [(0, 24.5), (2, 22), (18, 23.5), (21, 26), (18, 28.5), (1, 29)]
HEAD_POLY = [(164, 25.2), (186, 27.4), (197, 29.8), (199.5, 34.5), (191, 34.5), (176, 30.4)]
INTAKE = [(186, 44), (197, 41.5), (198, 50), (189, 50.5)]


def shoulder_y(x):      # the sharp shoulder crease, nearly level, very slightly rising to the rear
    return 27 - (x - 16) * 0.006


def crease_y(x):        # lower door crease rising toward the rear wheel
    return 48 - (140 - x) * 0.04


def build():
    px = [[None] * W for _ in range(H)]     # (rgb, kind) or None

    def P(x, y, col, kind=OTHER):
        x, y = int(x), int(y)
        if 0 <= x < W and 0 <= y < H:
            px[y][x] = (C[col] if isinstance(col, str) else col, kind)

    def fill(poly, fn):
        for y in range(H):
            for x in range(W):
                if in_poly(x + 0.5, y + 0.5, poly):
                    fn(x, y)

    # paint, banded top to bottom
    def paint(x, y):
        s = shoulder_y(x)
        if y < 21:
            P(x, y, "mid", UPPER)
        elif y < s - 2:
            P(x, y, "hi2", UPPER)
        elif y < s:
            P(x, y, "hi", UPPER)
        elif y < s + 2:
            P(x, y, "shade", LOWER)
        elif y < crease_y(x):
            P(x, y, "body", LOWER)
        elif y < 52:
            P(x, y, "lo", LOWER)
        else:
            P(x, y, "deep", LOWER)
    fill(BODY, paint)
    for x in range(74, 116):
        P(x, 1, "hi", UPPER)
    for x in range(88, 108):
        P(x, 2, "hi", UPPER)
    # shark-fin antenna
    for x in range(75, 83):
        top = 2 - (x - 75) * 0.4 if x < 80 else 0
        for y in range(round(top), 2):
            P(x, y, "trim")
    # side glass, black B-pillar, quarter-glass divider, reflections, chrome beltline
    fill(GLASS, lambda x, y: P(x, y, "glass"))
    for y in range(3, 21):
        for x in range(96, 100):
            P(x, y, "trim")
    for y in range(9, 21):
        P(round(63 - (y - 9) * 0.25), y, "trim")
    for k in range(10):
        P(117 + k, 8 + k, "glassHi")
        P(118 + k, 8 + k, "glassHi")
    for k in range(8):
        P(72 + k, 6 + k, "glassHi")
    for x in range(49, 139):
        P(x, round(21 - (x - 49) * 0.004), "chrome")
    # lower door crease, side skirt
    for x in range(66, 141):
        P(x, round(crease_y(x)), "shade", LOWER)
    for x in range(64, 144):
        P(x, 54, "dark")
    # door shut lines: front door, B-pillar, rear door curving round the rear arch
    for y in range(21, 54):
        P(141, y, "deep", LOWER)
        P(98, y, "deep", LOWER)
    for y in range(21, 38):
        P(64, y, "deep", LOWER)
    for y in range(38, 54):
        P(round(64 + (y - 38) * 0.2), y, "deep", LOWER)
    # hood / fender shut line
    for x in range(146, 172):
        P(x, round(23 + (x - 146) * 0.08), "lo", UPPER)
    # door handles on the shoulder line
    for hx in (82, 122):
        for x in range(hx, hx + 7):
            P(x, round(shoulder_y(x)), "chrome")
            P(x, round(shoulder_y(x)) + 1, "trim")
    # fuel door on the rear quarter
    for y in range(28, 34):
        for x in range(36, 43):
            if y in (28, 33) or x in (36, 42):
                P(x, y, "shade", LOWER)
    # door-mounted mirror with an amber LED indicator
    for y in range(16, 22):
        for x in range(135, 143):
            if not (y == 16 and (x < 137 or x > 140)):
                P(x, y, "hi" if y < 18 else "body", UPPER)
    for x in range(136, 142):
        P(x, 21, "amber")
    for y in range(19, 23):
        P(141, y, "trim")
    # owner's trunk lip spoiler (body-coloured ducktail)
    for x in range(2, 15):
        P(x, 19, "body", UPPER)
        P(x, 20, "shade", UPPER)
    for x in range(2, 9):
        P(x, 18, "hi", UPPER)
    P(1, 19, "deep", UPPER)
    # wrap-around taillight with the Z-shaped LED strip, bumper reflector
    fill(TAIL_POLY, lambda x, y: P(x, y, "tailTop" if y < 25 else "tailLow", TAIL))
    for x in range(2, 17):
        P(x, 26, "tailLed", TAIL)
    P(17, 25, "tailLed", TAIL)
    P(18, 26, "tailLed", TAIL)
    for y in range(43, 48):
        P(1, y, "reflector")
        P(2, y, "reflector")
    # slim smoked headlight: chrome top edge, LED projector row, DRL strip; chrome bar; intake
    fill(HEAD_POLY, lambda x, y: P(x, y, "housing"))
    for x in range(172, 197):
        P(x, round(26.6 + (x - 172) * 0.13), "housingTop")
    for x in range(180, 197, 3):
        y = round(28 + (x - 180) * 0.14)
        P(x, y, "led", HEAD)
        P(x + 1, y, "led", HEAD)
    for x in range(172, 199):
        P(x, round(29.4 + (x - 172) * 0.17), "drl", HEAD)
    for x in range(193, 200):
        P(x, 35, "chrome")
    fill(INTAKE, lambda x, y: P(x, y, "trim" if (x + y) % 3 else "dark"))
    for x in range(184, 196):
        P(x, 52, "trim")
    # wheel arches: very dark liner, lighter lip
    for wx in (RWX, FWX):
        for y in range(59):
            for x in range(W):
                d = math.hypot(x + 0.5 - wx, y + 0.5 - (WCY + 2.5))
                if d < 17.5 and (px[y][x] or d < 16.6):
                    P(x, y, "deep" if d > 16.6 else "liner", OTHER)
    return px


def rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def write_inc(px):
    vals, kinds = [], []
    for y in range(H):
        for x in range(W):
            p = px[y][x]
            vals.append(rgb565(p[0]) if p else 0)
            kinds.append(p[1] if p else NONE)
    packed = [(kinds[i] << 4) | (kinds[i + 1] if i + 1 < len(kinds) else 0) for i in range(0, len(kinds), 2)]
    lines = ["// Generated by tools/gen_car_sprite.py (Honda City sedan, side view). Do not edit.",
             "static const int CAR_W = %d, CAR_H = %d;" % (W, H),
             "static const uint16_t CAR_PX[%d] = {" % len(vals)]
    for i in range(0, len(vals), 12):
        lines.append("    " + ", ".join("0x%04X" % v for v in vals[i:i + 12]) + ",")
    lines += ["};", "// Two 4-bit pixel kinds per byte, high nibble first (see car_sprite.h).",
              "static const uint8_t CAR_KIND[%d] = {" % len(packed)]
    for i in range(0, len(packed), 16):
        lines.append("    " + ", ".join("0x%02X" % v for v in packed[i:i + 16]) + ",")
    lines.append("};")
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("gen_car_sprite: %s  %dx%d, %d opaque px" % (os.path.relpath(OUT), W, H, sum(1 for k in kinds if k)))


def write_png(px, path, scale=4, bg=(0x1A, 0x25, 0x64)):
    rows = []
    for y in range(H):
        row = b"".join(bytes(px[y][x][0] if px[y][x] else bg) * scale for x in range(W))
        rows += [b"\x00" + row] * scale

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W * scale, H * scale, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


if __name__ == "__main__":
    pixels = build()
    if "--png" in sys.argv:
        write_png(pixels, sys.argv[sys.argv.index("--png") + 1])
    else:
        write_inc(pixels)
```

- [ ] **Step 4: Generate the sprite and look at it**

Run: `python tools/gen_car_sprite.py`
Expected: `gen_car_sprite: src\ui\car_sprite.inc  200x66, 8372 opaque px`
Run: `python tools/gen_car_sprite.py --png car_preview.png`, open the PNG (Read tool shows images): it must match the approved mockup (silver sedan facing right, trunk lip spoiler, black B-pillar, red wrap-around taillight at the left, smoked headlight at the right, empty dark wheel arches). Delete `car_preview.png` afterwards (do not commit it).

- [ ] **Step 5: Write the module**

`src/ui/car_sprite.h`:
```cpp
#pragma once
#include <cstdint>
// The boot scene's car (tools/gen_car_sprite.py): silver Honda City sedan, side view facing
// right, 200 x 66 px with the ground line at the bottom. Wheels are not in the sprite (drawn in
// code so they spin). PURE, host-tested.
namespace car_sprite {
enum Kind : uint8_t { NONE = 0, UPPER = 1, LOWER = 2, OTHER = 3, HEAD = 4, TAIL = 5 };
// UPPER/LOWER: paint above/below the shoulder line (gets the streetlight reflection);
// HEAD/TAIL: lamp pixels (recoloured from the sound); OTHER: glass, trim, liner...
constexpr int REAR_WHEEL_X = 46, FRONT_WHEEL_X = 160, WHEEL_Y = 52;   // wheel centres
int width();
int height();
// False when (x, y) is transparent or outside; then *kind (if given) is NONE. Otherwise
// *rgb565 (if given) is its native RGB565 colour and *kind its Kind.
bool pixel(int x, int y, uint16_t* rgb565, uint8_t* kind);
}
```

`src/ui/car_sprite.cpp`:
```cpp
#include "car_sprite.h"

namespace car_sprite {

#include "car_sprite.inc"

int width() { return CAR_W; }
int height() { return CAR_H; }

bool pixel(int x, int y, uint16_t* rgb565, uint8_t* kind) {
    uint8_t k = NONE;
    if (x >= 0 && y >= 0 && x < CAR_W && y < CAR_H) {
        const int i = y * CAR_W + x;
        k = (i & 1) ? (CAR_KIND[i >> 1] & 0x0F) : (CAR_KIND[i >> 1] >> 4);
        if (k != NONE && rgb565) *rgb565 = CAR_PX[i];
    }
    if (kind) *kind = k;
    return k != NONE;
}

}
```

- [ ] **Step 6: Run the suite to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_car_sprite\test_car_sprite.cpp src\ui\car_sprite.cpp`
Expected: `6 Tests 0 Failures 0 Ignored`

- [ ] **Step 7: Commit**

```bash
git add tools/gen_car_sprite.py src/ui/car_sprite.h src/ui/car_sprite.cpp src/ui/car_sprite.inc test/test_car_sprite
git commit -m "feat(ui): Honda City car sprite for the boot scene

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: The scene renderer (`ui/bootscene`)

Firmware-only (TFT_eSPI, FreeRTOS); its maths is tested in Tasks 2–4. This task ends with a build and a forced-failure check on the deck; the full visual check is Task 6 once it is wired into boot.

**Files:**
- Create: `src/ui/bootscene.h`, `src/ui/bootscene.cpp`

**Interfaces:**
- Consumes: everything `bootanim::` from Task 3; `car_sprite::pixel/width/height`, `car_sprite::Kind`, `REAR_WHEEL_X`, `FRONT_WHEEL_X`, `WHEEL_Y` from Task 4; `greeting::durationMs()`, `greeting::level(uint32_t)` from Task 2; `theme::be(uint16_t)`; `mem::log(const char*)`.
- Produces (namespace `bootscene`): `void start(TFT_eSPI& tft, bool (*online)())`; `void setCaption(const char* text)`; `bool active()`; `void waitDone()`.

- [ ] **Step 1: Write the header**

`src/ui/bootscene.h`:
```cpp
#pragma once
#include <TFT_eSPI.h>
// Boot scene: a silver Honda City drives along a night road while the greeting plays and WiFi
// connects (docs/superpowers/specs/2026-10-09-boot-scene-design.md). Runs in its own task; while
// active() it is the only code drawing on the display.
namespace bootscene {
// Starts the scene task. `online` is polled each frame: the car leaves once it has been shown
// for 5 s, the greeting has ended and online() is true.
void start(TFT_eSPI& tft, bool (*online)());
void setCaption(const char* text);   // static string, shown top left in the sky
bool active();                       // true until the car has left and the task has ended
void waitDone();                     // blocks until !active()
}
```

- [ ] **Step 2: Write the renderer**

`src/ui/bootscene.cpp`:
```cpp
#include "bootscene.h"
#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include "car_sprite.h"
#include "theme.h"
#include "../audio/greeting.h"
#include "../core/mem.h"
#include "../util/bootanim.h"

namespace bootscene {
namespace {

using bootanim::hex565;

constexpr int W = 320, H = 240;
constexpr int SKY_H = 96;                        // static sky, drawn once (and on caption change)
constexpr int STRIP_H = 24;                      // animated area y 96-239 in 6 strips
constexpr int STRIPS = (H - SKY_H) / STRIP_H;
constexpr int GROUND_Y = 214;                    // the car's ground line
constexpr int CAR_TOP = GROUND_Y - 65;           // sprite row 0 (without the bob)
constexpr uint32_t FRAME_MS = 40;                // 25 fps target
constexpr int ROAD_SPEED = 90, SKYLINE_SPEED = 14;
constexpr int LAMP_PERIOD = 160, DASH_PERIOD = 48, SKYLINE_PERIOD = 720;
constexpr float TWO_PI = 6.28318530718f;

constexpr uint16_t SKY_BANDS[4] = {hex565(0x0B1030), hex565(0x0F1640), hex565(0x141D52),
                                   hex565(0x1A2564)};

TFT_eSPI* s_tft = nullptr;
bool (*s_online)() = nullptr;
volatile bool s_active = false;
const char* volatile s_caption = "";
volatile uint32_t s_captionGen = 0;

uint16_t* s_buf = nullptr;   // the strip being rendered (byte-swapped, as TFT_eSprite stores it)
int s_y0 = 0;                // screen y of the strip's first row

// ---- strip drawing (everything clipped to the current strip) ----
inline void put(int x, int y, uint16_t c) { s_buf[(y - s_y0) * W + x] = theme::be(c); }
inline uint16_t get(int x, int y) { return theme::be(s_buf[(y - s_y0) * W + x]); }

void rect(int x, int y, int w, int h, uint16_t c) {
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h;
    if (!bootanim::clip(x0, x1, 0, W) || !bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) return;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) put(xx, yy, c);
}

void blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a) {
    if (!a) return;
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h;
    if (!bootanim::clip(x0, x1, 0, W) || !bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) return;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) put(xx, yy, bootanim::blend565(c, get(xx, yy), a));
}

// ---- static sky (drawn straight on the display) ----
struct Star { int x, y; uint16_t c; };
Star star(int i) {
    return {(i * 73) % W, (i * 37) % SKY_H, i % 3 ? hex565(0xCFD8FF) : hex565(0xFFFFFF)};
}
bool starSkipped(const Star& s) {
    return (s.y < 24 && s.x < 220) || (s.x >= 264 && s.x <= 288 && s.y >= 12 && s.y <= 36);   // caption, moon
}
uint16_t bandAt(int y) { return SKY_BANDS[y / 30 < 3 ? y / 30 : 3]; }

void drawSky(const char* caption) {
    for (int i = 0; i < 4; ++i) s_tft->fillRect(0, i * 30, W, i < 3 ? 30 : SKY_H - 90, SKY_BANDS[i]);
    for (int i = 0; i < 40; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, s.c);
    }
    for (int dy = -10; dy <= 10; ++dy)          // moon: disc with a crescent shadow and two craters
        for (int dx = -10; dx <= 10; ++dx) {
            const int r2 = dx * dx + dy * dy;
            if (r2 > 100) continue;
            const bool shadow = (dx + 4) * (dx + 4) + (dy + 3) * (dy + 3) > 100;
            const uint16_t c = shadow ? hex565(0xC9C49E)
                                      : (r2 < 30 && dx > -2) ? hex565(0xFFFBE8) : hex565(0xF4F1D0);
            s_tft->drawPixel(276 + dx, 24 + dy, c);
        }
    s_tft->fillRect(279, 21, 2, 2, hex565(0xDCD7B0));
    s_tft->fillRect(273, 28, 3, 2, hex565(0xDCD7B0));
    s_tft->setTextColor(hex565(0xCFD8FF), SKY_BANDS[0]);
    s_tft->drawString(caption, 6, 6, 2);
}

void twinkle(uint32_t t) {
    for (int i = 0; i < 40; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, bootanim::starHidden(t, i) ? bandAt(s.y) : s.c);
    }
}

// ---- animated layers ----
void drawBackground(uint32_t t) {
    rect(0, 96, W, 24, SKY_BANDS[3]);
    rect(0, 120, W, 20, hex565(0x212E76));
    const int off = bootanim::scroll(t, SKYLINE_SPEED, SKYLINE_PERIOD);
    for (int i = 0; i < 30; ++i) {                       // skyline, tops at y >= 97
        const int h = 14 + (i * 53) % 30, w = 18 + (i * 7) % 10;
        const int sx = ((i * 24 - off) % SKYLINE_PERIOD + SKYLINE_PERIOD) % SKYLINE_PERIOD - 40;
        if (sx >= W || sx + w <= 0) continue;
        const int top = 140 - h;
        rect(sx, top, w, h, hex565(0x0A0D24));
        for (int wy = top + 4; wy < 136; wy += 6)
            for (int wx = sx + 3; wx < sx + w - 3; wx += 5)
                if (((wx * 7 + wy * 3) >> 2) % 4 == 0) rect(wx, wy, 2, 2, hex565(0xF0C860));
    }
    rect(0, 140, W, 8, hex565(0x10152E));
    const int lo = bootanim::scroll(t, ROAD_SPEED, LAMP_PERIOD);
    for (int k = -1; k < 4; ++k) {                       // streetlights with their light cones
        const int px = k * LAMP_PERIOD - lo + 40;
        rect(px, 98, 3, 52, hex565(0x3A4058));
        rect(px, 96, 16, 3, hex565(0x3A4058));
        rect(px + 12, 99, 6, 2, hex565(0xFFE9A8));
        for (int i = 0; i < 49; ++i) blendRect(px + 15 - i / 2, 101 + i, 1 + i, 1, hex565(0xFFE9A8), 26);
    }
    rect(0, 148, W, 4, hex565(0x6B7089));                // guardrail
    rect(0, 152, W, 2, hex565(0x2A2E44));
    rect(0, 154, W, 64, hex565(0x262A3A));               // road
    rect(0, 218, W, 22, hex565(0x1B1E2B));
    const int d = bootanim::scroll(t, ROAD_SPEED, DASH_PERIOD);
    for (int x = -DASH_PERIOD; x < W; x += DASH_PERIOD) rect(x - d, 226, 26, 3, hex565(0xD8D8C0));
}

void wheel(int cx, int cy, uint32_t t) {
    const float base = bootanim::spokeAngle(t);
    for (int dy = -14; dy <= 14; ++dy) {
        const int y = cy + dy;
        if (y < s_y0 || y >= s_y0 + STRIP_H) continue;
        for (int dx = -14; dx <= 14; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= W) continue;
            const float d = sqrtf((float)(dx * dx + dy * dy));
            if (d > 13.5f) continue;
            uint16_t c;
            if (d > 10.5f) c = hex565(0x141414);             // tyre
            else if (d > 9.5f) c = hex565(0xC9CED4);         // rim lip
            else if (d <= 1.0f) c = hex565(0x3A3E46);        // hub centre
            else if (d <= 2.3f) c = hex565(0xC8CCD2);        // hub
            else {
                c = hex565(0x2B2F36);                        // dark rim face
                if (d >= 2.5f) {                             // 5 twin spokes, machined faces
                    const float a = atan2f((float)dy, (float)dx) - base;
                    float best = 9.0f;
                    for (int k = 0; k < 5; ++k)
                        for (float off : {-0.17f, 0.17f})
                            best = fminf(best, fabsf(remainderf(a - (k * TWO_PI / 5 + off), TWO_PI)));
                    if (best < 0.10f) c = hex565(0xDFE3E8);
                    else if (best < 0.18f) c = hex565(0xAAB0B8);
                }
            }
            put(x, y, c);
        }
    }
}

struct Car { uint32_t t; int x; int top; uint8_t e; bool lamp; int lampX; uint16_t glow; };

// The streetlight head nearest the car's middle, in car coordinates.
bool nearestLamp(uint32_t t, int carX, int* lampX) {
    const int lo = bootanim::scroll(t, ROAD_SPEED, LAMP_PERIOD);
    bool found = false;
    for (int k = -1; k < 4; ++k) {
        const int hx = k * LAMP_PERIOD - lo + 40 + 14 - carX;
        if (hx <= -40 || hx >= 240) continue;
        if (!found || abs(hx - 100) < abs(*lampX - 100)) *lampX = hx;
        found = true;
    }
    return found;
}

void drawCar(const Car& c) {
    rect(c.x + 24, GROUND_Y, 154, 2, hex565(0x121420));              // shadow
    const uint8_t ga = bootanim::glowAlpha(c.e);                       // RGB underglow
    blendRect(c.x + 66, GROUND_Y - 10, 76, 2, c.glow, ga);
    for (int i = 0; i < 8; ++i)
        blendRect(c.x + 30 - 3 * i, GROUND_Y - 7 + i, 140 + 6 * i, 1, c.glow, (uint8_t)(ga * (60 - 7 * i) / 100));
    const uint8_t ba = bootanim::beamAlpha(c.e);                       // headlight beam
    for (int i = 0; i < 90; ++i) {
        const int h = 3 + i * 35 / 100;
        blendRect(c.x + 200 + i, c.top + 31 - h / 3, 1, h, hex565(0xFFF3B0), (uint8_t)(ba * (90 - i) / 90));
    }
    blendRect(c.x - 4, c.top + 21, 6, 9, hex565(0xFF3030), bootanim::tailGlowAlpha(c.e));

    int x0 = c.x, x1 = c.x + car_sprite::width(), y0 = c.top, y1 = c.top + car_sprite::height();
    if (bootanim::clip(x0, x1, 0, W) && bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) {
        const uint16_t head = bootanim::headColor(c.e), refl = hex565(0xFFF0C8);
        const uint8_t tail = bootanim::tailLevel(c.e);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                uint16_t col;
                uint8_t kind;
                if (!car_sprite::pixel(x - c.x, y - c.top, &col, &kind)) continue;
                if (kind == car_sprite::HEAD) col = head;
                else if (kind == car_sprite::TAIL) col = bootanim::scale565(col, tail);
                else if (c.lamp && (kind == car_sprite::UPPER || kind == car_sprite::LOWER)) {
                    const uint8_t a = bootanim::reflectAlpha((x - c.x) - c.lampX, kind == car_sprite::UPPER);
                    if (a) col = bootanim::blend565(refl, col, a);
                }
                put(x, y, col);
            }
    }
    const int wy = CAR_TOP + car_sprite::WHEEL_Y;                      // wheels stay on the road (no bob)
    wheel(c.x + car_sprite::REAR_WHEEL_X, wy, c.t);
    wheel(c.x + car_sprite::FRONT_WHEEL_X, wy, c.t);
}

// No strip buffer: caption on black, same exit rule, no animation.
void runWithoutScene(uint32_t t0) {
    uint32_t gen = s_captionGen - 1;
    for (;;) {
        if (gen != s_captionGen) {
            gen = s_captionGen;
            s_tft->fillScreen(TFT_BLACK);
            s_tft->setTextColor(TFT_WHITE, TFT_BLACK);
            s_tft->drawString(s_caption, 10, 10, 2);
        }
        const uint32_t t = millis() - t0;
        if (bootanim::mayExit(t, t >= greeting::durationMs(), s_online())) return;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void task(void*) {
    mem::log("bootscene start");
    const uint32_t t0 = millis();
    TFT_eSprite spr(s_tft);
    spr.setColorDepth(16);
    s_buf = (uint16_t*)spr.createSprite(W, STRIP_H);
    if (!s_buf) {
        Serial.println("[bootscene] no memory for the strip buffer: scene skipped");
        runWithoutScene(t0);
    } else {
        uint32_t gen = s_captionGen;
        drawSky(s_caption);
        uint32_t tExit = bootanim::NO_EXIT, frames = 0, busyMs = 0;
        TickType_t wake = xTaskGetTickCount();
        for (bool done = false; !done;) {
            const uint32_t t = millis() - t0;
            if (tExit == bootanim::NO_EXIT && bootanim::mayExit(t, t >= greeting::durationMs(), s_online()))
                tExit = t;
            done = bootanim::exitDone(t, tExit);       // still draws this last frame: car off screen
            if (gen != s_captionGen) {
                gen = s_captionGen;
                drawSky(s_caption);
            }
            const uint32_t f0 = millis();
            twinkle(t);
            Car car{};
            car.t = t;
            car.x = bootanim::carX(t, tExit);
            car.top = CAR_TOP + bootanim::bob(t);
            car.e = greeting::level(t);
            car.lamp = nearestLamp(t, car.x, &car.lampX);
            car.glow = bootanim::hue565(t);
            for (int s = 0; s < STRIPS; ++s) {
                s_y0 = SKY_H + s * STRIP_H;
                drawBackground(t);
                drawCar(car);
                spr.pushSprite(0, s_y0);
            }
            busyMs += millis() - f0;
            ++frames;
            vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS));
        }
        spr.deleteSprite();
        s_buf = nullptr;
        Serial.printf("[bootscene] %u frames in %u ms, render avg %u ms, stack free %u\n",
                      (unsigned)frames, (unsigned)(millis() - t0), (unsigned)(frames ? busyMs / frames : 0),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
    mem::log("bootscene end");
    s_active = false;
    vTaskDelete(nullptr);
}

}  // namespace

void start(TFT_eSPI& tft, bool (*online)()) {
    s_tft = &tft;
    s_online = online;
    s_active = true;
    xTaskCreatePinnedToCore(task, "bootscene", 4096, nullptr, 1, nullptr, 1);
}

void setCaption(const char* text) {
    s_caption = text;
    s_captionGen = s_captionGen + 1;
}

bool active() { return s_active; }

void waitDone() {
    while (s_active) delay(20);
}

}
```

- [ ] **Step 3: Build**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`, no warnings from `src/ui/bootscene.cpp`, `src/ui/car_sprite.cpp`, `src/util/*.cpp`. The scene isn't called yet (Task 6).

- [ ] **Step 4: Check the no-memory fallback on the deck (Review Focus 5)**

Temporarily wire the scene with a forced failure, check, then revert:
1. In `src/ui/bootscene.cpp`, change `s_buf = (uint16_t*)spr.createSprite(W, STRIP_H);` to `s_buf = nullptr;  // TEMP fallback test`.
2. In `src/main.cpp`, add `#include "ui/bootscene.h"` with the other `ui/` includes, and right after the line `greeting::play();          // boot sound plays while WiFi connects` add:
```cpp
    bootscene::start(s_tft, net::isOnline);  // TEMP fallback test
    bootscene::setCaption("Connecting WiFi...");
    bootscene::waitDone();
```
3. Flash: `pio run -e esp32dev -t upload --upload-port COM11`; read the serial log for ~10 s after a reset (115200 baud).
Expected: `[bootscene] no memory for the strip buffer: scene skipped`; the screen shows "Connecting WiFi..." on black; the greeting plays; after ≥ 5 s (with the hotspot on) `[mem] bootscene end` and the deck boots as before.
4. Revert both temporary edits (`git diff src/main.cpp src/ui/bootscene.cpp` shows only the wanted file, then `git checkout src/main.cpp`; restore the `createSprite` line).

- [ ] **Step 5: Commit**

```bash
git add src/ui/bootscene.h src/ui/bootscene.cpp
git commit -m "feat(ui): boot scene renderer (night road, City, lights follow the greeting)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Boot integration, docs and on-deck verification

**Files:**
- Modify: `src/main.cpp` (includes; `setup()` around the greeting/WiFi block; first lines of `loop()`)
- Modify: `src/core/nettask.cpp` (includes; late-WiFi path in `run()`)
- Modify: `README.md` (feature section and architecture entry)

**Interfaces:**
- Consumes: `bootscene::start/setCaption/active/waitDone` (Task 5), `net::isOnline()`, `net::connectAny()`.

- [ ] **Step 1: Wire the scene into `setup()`**

In `src/main.cpp`, add `#include "ui/bootscene.h"` after `#include "ui/backlight.h"`. Then replace:
```cpp
    greeting::play();          // boot sound plays while WiFi connects
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    s_tft.drawString("Connecting WiFi...", 10, 10, 2);

    bool spotifyReady = false;
    if (net::connectAny()) {
        mem::log("boot+wifi");
        netclock::begin();
        s_tft.fillScreen(TFT_BLACK);
```
with:
```cpp
    greeting::play();          // boot sound plays while WiFi connects
    // Night drive while WiFi connects: the scene draws until the car leaves (>= 5 s, WiFi up).
    bootscene::start(s_tft, net::isOnline);
    bootscene::setCaption("Connecting WiFi...");

    bool spotifyReady = false;
    if (net::connectAny()) {
        mem::log("boot+wifi");
        netclock::begin();
        bootscene::waitDone();               // the display is ours again
        s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        s_tft.fillScreen(TFT_BLACK);
```
and replace:
```cpp
    } else {
        s_tft.drawString("WiFi not found - retrying...", 10, 40, 2);
    }
```
with:
```cpp
    } else {
        // The scene keeps running; the network task retries and the car leaves once WiFi is up.
        bootscene::setCaption("WiFi not found - retrying...");
    }
```

- [ ] **Step 2: `loop()` waits while the scene owns the display**

In `src/main.cpp`, make the first statements of `loop()`:
```cpp
void loop() {
    if (bootscene::active()) {   // boot scene still on screen (WiFi wasn't up at boot)
        delay(20);
        return;
    }
```
(The existing body follows unchanged; its first draw after the scene is a full one because `lastMode` starts at -1.)

- [ ] **Step 3: The late-WiFi path frees the scene's heap before TLS**

In `src/core/nettask.cpp`, add `#include "../ui/bootscene.h"` with the other includes, then replace:
```cpp
            if (net::isOnline() || net::connectAny()) {
                spclient::begin();
```
with:
```cpp
            if (net::isOnline() || net::connectAny()) {
                // The boot scene leaves once WiFi is up; let it free its strip before TLS needs heap.
                while (bootscene::active()) vTaskDelay(pdMS_TO_TICKS(100));
                spclient::begin();
```

- [ ] **Step 4: Build and run all host tests**

Run: `pio run -e esp32dev` → `SUCCESS`; `pio run -e hwcheck` → `SUCCESS`.
Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` → `suites pass=28 fail=0`.

- [ ] **Step 5: On the deck, hotspot ON (Review Focus 2 and 4)**

Flash: `pio run -e esp32dev -t upload --upload-port COM11`, reset, capture serial for 20 s.
Expected serial, in this order: `[mem] bootscene start`, `[greeting] played ...`, the WiFi lines, `[mem] boot+wifi`, `[bootscene] N frames in T ms, render avg R ms, stack free S` with **T ≥ 5900**, **R ≤ 40**, **S ≥ 512**, then `[mem] bootscene end` and the usual Spotify start.
Expected on screen (ask the owner to watch): the car drives in from the left without tearing at the screen edge, cruises with spinning wheels, lights pulse with the beeps and voice, underglow cycles, streetlight reflections sweep over the paint; after ~5 s it drives off the right edge with **no part of the car left on screen**, then "Spotify auth..." / the deck appears.
If R > 40 or S < 512: stop and report (the strip count or stack size needs revisiting), don't tune blindly.

- [ ] **Step 6: On the deck, hotspot OFF then ON (Review Focus 1 and 3)**

With the hotspot off, reset the deck and capture serial for ~60 s; turn the hotspot on after ~30 s.
Expected: the caption changes from "Connecting WiFi..." to "WiFi not found - retrying..." with no leftover characters; the car keeps cruising; after the hotspot comes up the car leaves, the serial log shows `[mem] bootscene end` **before** `[mem] late wifi`, and the deck (or "Nothing playing") appears and Spotify polls succeed.

- [ ] **Step 7: README**

In `README.md`, after the `### Greeting (boot sound)` section, add:
```markdown
### Boot scene

While the greeting plays and WiFi connects, the screen shows a night drive: a silver Honda City
(trunk lip spoiler, RGB underglow) drives in, cruises for at least 5 s and until WiFi is up,
then drives off. Its lights pulse with the greeting. If no WiFi is found the car keeps cruising
("WiFi not found - retrying...") until the hotspot appears. The car is generated by
`python tools/gen_car_sprite.py` (`--png out.png` for a preview).
```
In the Architecture list, after the `- **Greeting**` entry, add:
```markdown
- **Boot scene** (`ui/bootscene`, `ui/car_sprite`, `util/bootanim`, `util/loudness`) — own task
  at boot; sky drawn once, y 96–239 re-rendered at ~25 fps through one 320×24 strip (15 KB,
  freed at the end). `setup()` waits for it; `loop()` and the late-WiFi path wait while active.
```
In the "Pure, host-tested modules" entry, add `car_sprite` to the `ui/{...}` list and `bootanim,loudness` to the `util/{...}` list.

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp src/core/nettask.cpp README.md
git commit -m "feat: night-drive boot scene while WiFi connects

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 9: Mark the spec implemented**

In `docs/superpowers/specs/2026-10-09-boot-scene-design.md`, change the status line to `Status: implemented (main, ` followed by the short hashes of the Task 2–6 commits from `git log --oneline -5`, comma-separated, then `).`, and commit:
```bash
git add docs/superpowers/specs/2026-10-09-boot-scene-design.md
git commit -m "docs: boot scene spec implemented

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
