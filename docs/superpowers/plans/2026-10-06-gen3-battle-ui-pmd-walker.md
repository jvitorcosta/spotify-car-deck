# Gen-3 Battle UI + PMD Walk-Cycle Walker — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restyle the deck as a Gen-3 Pokémon battle screen (battle boxes, HP tag, dialogue box, pixel icons) and replace the bob/flip walker with a real PMD walk cycle, falling back to the current walker when no sheet exists.

**Architecture:** Pure, host-tested modules hold every decision that can be expressed without hardware: palette + RGB565 conversion (`ui/theme`), device-icon mapping + icon art (`ui/icon_map`), AnimData XML parsing (`util/animdata`), walk-frame arithmetic (`util/walkanim`), dirty rects (`util/walkrect`, exists). Device modules do I/O only: a generic HTTPS/SD fetch (`images/fetch`, `images/cache`), a single walk-frame store with PMD + fallback loaders (`images/walksprite`), Gen-3 draw primitives (`ui/battle`, `ui/icons`), and the screen (`ui/screen_now`) driven by `main.cpp`.

**Tech Stack:** C++17, PlatformIO `espressif32` / Arduino-ESP32, TFT_eSPI 2.5.43, PNGdec 1.1.6, TJpg_Decoder, SpotifyArduino, SD. Host tests: Unity via `.devtools/ntest.ps1` (g++).

**Spec:** `docs/superpowers/specs/2026-10-06-merged-status-panel-pmd-walker-design.md`

## Global Constraints

- Target board `esp32dev`, **no PSRAM**: never allocate a full-frame bitmap; transient decode buffers must be bounded and freed.
- Display ILI9341 landscape 320×240, `tft.invertDisplay(true)`; image data is pushed as **big-endian RGB565** with swap off (same as album art / existing sprites).
- External hosts only: `api.spotify.com`, `accounts.spotify.com`, `i.scdn.co`, `pokeapi.co`, `raw.githubusercontent.com`, `lrclib.net`.
- PMD base URL: `https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/<DDDD>/` (`%04d` dex). Files: `AnimData.xml` (≤ 32 KB), `Walk-Anim.png` (≤ 40 KB). Row 2 = Right.
- Walk band height **32 px**; walk-frame store capped at **16 KB** (RGB565 + 1-byte mask = 3 B/px); max frame width 64 px; max 16 frames.
- Pure modules (`ui/theme`, `ui/icon_map`, `util/*`) must not include `Arduino.h`/TFT headers so they compile on the host.
- Native tests: `pio test -e native` is broken on this machine. Run each suite with
  `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 <test.cpp> <module.cpp...>` (exit 0 = pass).
- Device build/flash: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev [-t upload --upload-port COM11]`; serial: `python .devtools\serial_read.py COM11 <seconds>`.
- Secrets live only in `src/config.h` (git-ignored). Serial baud 115200.
- Commits: Conventional Commits (`type(scope): subject`, e.g. `perf(net): ...`), one deliverable per commit, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Pokémon with no PMD walk sheet / no `Walk` anim / `CopyOf` chains** — must fall back to the PokeAPI walker, never show nothing. Covered: `test_animdata` (missing Walk, CopyOf, prefix names) + Task 5 device step forcing a fallback dex.
2. **Very large or wide sheets** (big Pokémon: 10+ frames, 64+ px frames) — must downscale/skip frames to fit 16 KB, never overflow. Covered: `test_walkanim` (`keepEvery` cap, `fitBand` downscale, merged durations).
3. **Repeat flag before the first player poll** — must show "off", not repeat-one. Covered: Task 2 convention change + `test_icon_map` repeat mapping test.
4. **Corrupt SD cache entries** (truncated PNG/XML from a power cut in the car) — must be deleted and re-downloaded, not fail forever. Covered: Task 5 `removePath` on parse/decode failure; device step deletes a file manually.
5. **Long lyric lines / no lyric** in the 2-line dialogue box — must truncate with `...`, and an empty box when there is no line. Covered: Task 7 `wrapTwo` pure helper with tests in `test_textfit`.

---

## File Structure

```
src/ui/theme.h / theme.cpp      MODIFY  rgb565 helper, Gen-3 battle palette, HP shine colours (PURE)
src/ui/icon_map.h / .cpp        CREATE  Icon enum, device-type -> icon, 12x12 icon art (PURE)
src/util/animdata.h / .cpp      CREATE  PMD AnimData.xml -> Walk frame size + durations (PURE)
src/util/walkanim.h / .cpp      CREATE  bbox, fit-to-band, nearest index, frame skip, frame timing (PURE)
src/util/textfit.h / .cpp       CREATE  two-line greedy wrap with ellipsis by a width callback (PURE)
src/images/fetch.h / .cpp       CREATE  HTTPS GET -> heap buffer (size-guarded, NUL-terminated)
src/images/cache.h / .cpp       MODIFY  path-based API (+ /pmd dir), sprite API kept on top
src/images/png.h / .cpp         MODIFY  shared PNG decoder + sprite bytes only (drawSprite/walk loader removed)
src/images/walksprite.h / .cpp  CREATE  walk-frame store; loadPmd(); loadFallback()
src/ui/battle.h / .cpp          CREATE  Gen-3 primitives: background, battle box, shadow text, HP bar, EXP bar, dialogue box
src/ui/icons.h / .cpp           CREATE  drawIcon (from icon_map art), procedural spinning CD
src/ui/screen_now.h / .cpp      REWRITE battle layout, top strip, status box, walker, dialogue lyric, status screens
src/ui/widgets.h / .cpp         MODIFY  remove panel()/hpBar() (superseded); keep Button/hit for future touch
include/app_state.h             MODIFY  deviceType[16]; repeat convention comment
src/spotify/client.cpp          MODIFY  store device.type; map repeat to 0 off / 1 context / 2 track
src/main.cpp                    MODIFY  load PMD->fallback, heap log, CD/walker/top-strip timing
test/test_theme, test_icon_map, test_animdata, test_walkanim, test_textfit   (host suites)
CREDITS.md                      CREATE
```

---

### Task 1: Battle palette + RGB565 helper (PURE)

**Files:**
- Modify: `src/ui/theme.h`
- Test: `test/test_theme/test_theme.cpp`

**Interfaces:**
- Produces: `constexpr uint16_t theme::rgb(uint8_t r, uint8_t g, uint8_t b)`; constants `SKY, GRASS, HORIZON, BOX_FILL, BOX_BORDER, BOX_SHADOW, TEXT, TEXT_SHADOW, HP_TAG, HP_TAG_TEXT, HP_EMPTY, HP_GREEN, HP_GREEN_SHINE, HP_YELLOW, HP_YELLOW_SHINE, HP_RED, HP_RED_SHINE, EXP_BLUE, DLG_FRAME, DLG_LINE, DLG_FILL, DLG_SHADOW, TOP_DARK, ICON_OFF, CD_SILVER`; `hpColor(float)`, `hpShine(float)`. Existing `GBA_*`, `POKE_RED`, `typeColor` unchanged.

- [ ] **Step 1: Add failing tests to `test/test_theme/test_theme.cpp`**

Add these functions above `main` and register them with `RUN_TEST` in `main`:

```cpp
void test_rgb565_primaries() {
    TEST_ASSERT_EQUAL_HEX16(0xF800, theme::rgb(0xFF, 0x00, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, theme::rgb(0x00, 0xFF, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x001F, theme::rgb(0x00, 0x00, 0xFF));
    TEST_ASSERT_EQUAL_HEX16(0xFFDB, theme::rgb(0xF8, 0xF8, 0xD8));   // battle box cream
}
void test_hp_color_thresholds() {
    TEST_ASSERT_EQUAL_HEX16(theme::HP_GREEN,  theme::hpColor(0.51f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_YELLOW, theme::hpColor(0.50f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_YELLOW, theme::hpColor(0.21f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_RED,    theme::hpColor(0.20f));
}
void test_hp_shine_follows_color() {
    TEST_ASSERT_EQUAL_HEX16(theme::HP_GREEN_SHINE,  theme::hpShine(0.9f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_YELLOW_SHINE, theme::hpShine(0.4f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_RED_SHINE,    theme::hpShine(0.1f));
}
```

```cpp
    RUN_TEST(test_rgb565_primaries);
    RUN_TEST(test_hp_color_thresholds);
    RUN_TEST(test_hp_shine_follows_color);
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_theme\test_theme.cpp src\ui\theme.cpp`
Expected: `COMPILE FAILED` (`rgb` / `hpShine` / `HP_GREEN_SHINE` not declared).

- [ ] **Step 3: Replace the HP section of `src/ui/theme.h`**

Replace everything from `// Pokémon HP-bar colors` through the end of `hpColor` with:

```cpp
// 8-bit-per-channel colour -> RGB565 (what TFT_eSPI fill/draw calls take).
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Gen-3 (FRLG/Emerald) battle palette. Approximations of the GBA games; tune on
// the real panel, which renders colours differently from a PC screen.
constexpr uint16_t SKY         = rgb(0xA8, 0xD8, 0xF8);
constexpr uint16_t GRASS       = rgb(0x88, 0xC8, 0x78);
constexpr uint16_t HORIZON     = rgb(0x60, 0xA8, 0x58);
constexpr uint16_t BOX_FILL    = rgb(0xF8, 0xF8, 0xD8);
constexpr uint16_t BOX_BORDER  = rgb(0x40, 0x48, 0x48);
constexpr uint16_t BOX_SHADOW  = rgb(0x58, 0x70, 0x60);
constexpr uint16_t TEXT        = rgb(0x40, 0x40, 0x40);
constexpr uint16_t TEXT_SHADOW = rgb(0xD8, 0xD0, 0xB0);
constexpr uint16_t HP_TAG      = rgb(0x48, 0x48, 0x48);
constexpr uint16_t HP_TAG_TEXT = rgb(0xF8, 0xB8, 0x00);
constexpr uint16_t HP_EMPTY    = rgb(0x50, 0x60, 0x58);
constexpr uint16_t EXP_BLUE    = rgb(0x40, 0xC8, 0xF8);
constexpr uint16_t DLG_FRAME   = rgb(0x28, 0x48, 0x60);
constexpr uint16_t DLG_LINE    = rgb(0x68, 0xA0, 0xB8);
constexpr uint16_t DLG_FILL    = rgb(0xF8, 0xF8, 0xF8);
constexpr uint16_t DLG_SHADOW  = rgb(0xD0, 0xD0, 0xD0);
constexpr uint16_t TOP_DARK    = rgb(0x28, 0x30, 0x38);
constexpr uint16_t ICON_OFF    = rgb(0x60, 0x68, 0x70);
constexpr uint16_t CD_SILVER   = rgb(0xC0, 0xC0, 0xC8);

// Pokémon HP-bar colours (fill + lighter 2 px shine line on top).
constexpr uint16_t HP_GREEN        = rgb(0x58, 0xD0, 0x80);
constexpr uint16_t HP_GREEN_SHINE  = rgb(0x90, 0xF8, 0xB8);
constexpr uint16_t HP_YELLOW       = rgb(0xF8, 0xC8, 0x00);
constexpr uint16_t HP_YELLOW_SHINE = rgb(0xF8, 0xE8, 0x70);
constexpr uint16_t HP_RED          = rgb(0xF8, 0x58, 0x38);
constexpr uint16_t HP_RED_SHINE    = rgb(0xF8, 0xA0, 0x88);

// HP-bar fill colour for a remaining fraction (Pokémon thresholds:
// >50% green, >20% yellow, else red).
constexpr uint16_t hpColor(float frac) {
    return frac > 0.5f ? HP_GREEN : (frac > 0.2f ? HP_YELLOW : HP_RED);
}
constexpr uint16_t hpShine(float frac) {
    return frac > 0.5f ? HP_GREEN_SHINE : (frac > 0.2f ? HP_YELLOW_SHINE : HP_RED_SHINE);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_theme\test_theme.cpp src\ui\theme.cpp`
Expected: `7 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Device build still compiles**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]` (widgets.cpp still uses `hpColor`).

- [ ] **Step 6: Commit**

```bash
git add src/ui/theme.h test/test_theme/test_theme.cpp
git commit -m "Add Gen-3 battle palette and tested RGB565 helper"
```

---

### Task 2: Device-icon map + icon art + device type / repeat convention

**Files:**
- Create: `src/ui/icon_map.h`, `src/ui/icon_map.cpp`
- Create: `test/test_icon_map/test_icon_map.cpp`
- Modify: `include/app_state.h`, `src/spotify/client.cpp:145-151`

**Interfaces:**
- Produces:
  - `enum class icons::Icon : uint8_t { Phone, Laptop, Speaker, Tv, Car, Shuffle, Repeat, Note };`
  - `constexpr int icons::SIZE = 12;`
  - `icons::Icon icons::forDevice(const char* spotifyType);`
  - `const char* icons::row(icons::Icon i, int y);` (12-char string of `#`/`.`; `nullptr` if y out of range)
  - `bool icons::pixel(icons::Icon i, int x, int y);`
  - `AppState::deviceType[16]`; `AppState::repeat` convention **0 = off, 1 = context, 2 = track**.

- [ ] **Step 1: Write the failing test `test/test_icon_map/test_icon_map.cpp`**

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/ui/icon_map.h"

using icons::Icon;
void setUp() {}
void tearDown() {}

void test_device_types_map_to_icons() {
    TEST_ASSERT_EQUAL_INT((int)Icon::Phone,   (int)icons::forDevice("Smartphone"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Phone,   (int)icons::forDevice("Tablet"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Laptop,  (int)icons::forDevice("Computer"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice("Speaker"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice("CastAudio"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Tv,      (int)icons::forDevice("TV"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Tv,      (int)icons::forDevice("GameConsole"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Car,     (int)icons::forDevice("Automobile"));
}
void test_device_type_case_insensitive() {
    TEST_ASSERT_EQUAL_INT((int)Icon::Phone, (int)icons::forDevice("smartphone"));
}
void test_unknown_empty_null_device_is_speaker() {
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice("Unknown"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice(""));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice(nullptr));
}
void test_every_icon_is_12x12() {
    for (int i = 0; i <= (int)Icon::Note; ++i) {
        for (int y = 0; y < icons::SIZE; ++y) {
            const char* r = icons::row((Icon)i, y);
            TEST_ASSERT_NOT_NULL(r);
            TEST_ASSERT_EQUAL_INT(icons::SIZE, (int)strlen(r));
        }
        TEST_ASSERT_NULL(icons::row((Icon)i, icons::SIZE));
    }
}
void test_pixel_reads_art_and_bounds() {
    TEST_ASSERT_TRUE(icons::pixel(Icon::Phone, 3, 0));    // "...######..." top edge
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, 0, 0));
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, -1, 0));
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, 12, 0));
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, 0, 12));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_device_types_map_to_icons);
    RUN_TEST(test_device_type_case_insensitive);
    RUN_TEST(test_unknown_empty_null_device_is_speaker);
    RUN_TEST(test_every_icon_is_12x12);
    RUN_TEST(test_pixel_reads_art_and_bounds);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_icon_map\test_icon_map.cpp`
Expected: `COMPILE FAILED` (`icon_map.h` missing).

- [ ] **Step 3: Create `src/ui/icon_map.h`**

```cpp
#pragma once
#include <cstdint>
// Pixel-art icons for the top strip and dialogue box (the fonts are ASCII-only,
// so these stand in for emoji). PURE: no Arduino/TFT, host-tested.
namespace icons {
enum class Icon : uint8_t { Phone, Laptop, Speaker, Tv, Car, Shuffle, Repeat, Note };
constexpr int SIZE = 12;
// Spotify device "type" (Smartphone, Computer, Speaker, TV, Automobile, ...) -> icon.
// Unknown/empty/null -> Speaker.
Icon forDevice(const char* spotifyType);
// Row y of the 12x12 art as '#'(on)/'.'(off); nullptr if y is out of range.
const char* row(Icon i, int y);
bool pixel(Icon i, int x, int y);
}
```

- [ ] **Step 4: Create `src/ui/icon_map.cpp`**

```cpp
#include "icon_map.h"
#include <cctype>
#include <cstring>

namespace icons {

static const char* const ART[][SIZE] = {
    {   // Phone
        "...######...", "..#......#..", "..#.####.#..", "..#.#..#.#..",
        "..#.#..#.#..", "..#.#..#.#..", "..#.#..#.#..", "..#.####.#..",
        "..#......#..", "..#..##..#..", "..#......#..", "...######...",
    },
    {   // Laptop
        "............", ".##########.", ".#........#.", ".#........#.",
        ".#........#.", ".#........#.", ".#........#.", ".##########.",
        "############", "#..........#", "############", "............",
    },
    {   // Speaker
        "..########..", "..#......#..", "..#..##..#..", "..#..##..#..",
        "..#......#..", "..#.####.#..", "..##....##..", "..#......#..",
        "..##....##..", "..#.####.#..", "..#......#..", "..########..",
    },
    {   // Tv
        "...#....#...", "....#..#....", ".....##.....", "############",
        "#..........#", "#..........#", "#..........#", "#..........#",
        "#..........#", "############", "..#......#..", "............",
    },
    {   // Car
        "............", "............", "...######...", "..#..#...#..",
        ".#...#....#.", "############", "#..........#", "#.##....##.#",
        "############", "..##....##..", "............", "............",
    },
    {   // Shuffle (crossing arrows)
        "............", "............", "..........#.", "###....#####",
        "...#..#...#.", "....##......", "....##......", "...#..#...#.",
        "###....#####", "..........#.", "............", "............",
    },
    {   // Repeat (loop with arrow heads)
        "............", "........#...", ".#########..", "#.......#...",
        "#...........", "#..........#", "#..........#", "...........#",
        "...#.......#", "..#########.", "...#........", "............",
    },
    {   // Note (eighth note)
        "......###...", "......####..", "......#..##.", "......#...#.",
        "......#.....", "......#.....", "......#.....", "..#####.....",
        ".######.....", ".######.....", "..####......", "............",
    },
};

static bool eq(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    return *a == *b;
}

Icon forDevice(const char* t) {
    if (!t || !t[0]) return Icon::Speaker;
    if (eq(t, "Smartphone") || eq(t, "Tablet")) return Icon::Phone;
    if (eq(t, "Computer")) return Icon::Laptop;
    if (eq(t, "TV") || eq(t, "CastVideo") || eq(t, "STB") || eq(t, "GameConsole")) return Icon::Tv;
    if (eq(t, "Automobile")) return Icon::Car;
    return Icon::Speaker;   // Speaker, AVR, CastAudio, AudioDongle, Unknown, ...
}

const char* row(Icon i, int y) {
    if (y < 0 || y >= SIZE) return nullptr;
    return ART[(int)i][y];
}

bool pixel(Icon i, int x, int y) {
    const char* r = row(i, y);
    return r && x >= 0 && x < SIZE && r[x] == '#';
}

}
```

- [ ] **Step 5: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_icon_map\test_icon_map.cpp src\ui\icon_map.cpp`
Expected: `5 Tests 0 Failures 0 Ignored` / `OK`. If `test_every_icon_is_12x12` fails, fix the art row named in the failure to exactly 12 characters.

- [ ] **Step 6: Store device type and fix the repeat convention**

In `include/app_state.h`, replace the line `bool isPlaying; bool shuffle; int repeat; int popularity; int volume;` and the `deviceName` line with:

```cpp
    bool isPlaying; bool shuffle;
    int repeat;            // 0 = off, 1 = context, 2 = track (zero-init = off)
    int popularity; int volume;
    char deviceName[48];
    char deviceType[16];   // Spotify device type, e.g. "Smartphone"
```

In `src/spotify/client.cpp`, replace the body of `onPlayer` with:

```cpp
static void onPlayer(PlayerDetails pd) {
    AppState& st = *target;
    copyStr(st.deviceName, pd.device.name, sizeof(st.deviceName));
    copyStr(st.deviceType, pd.device.type, sizeof(st.deviceType));
    st.volume  = pd.device.volumePercent;
    st.shuffle = pd.shuffleState;
    // Library enum is repeat_track=0, repeat_context=1, repeat_off=2; ours makes the
    // zero-initialised state mean "off" so nothing lights up before the first poll.
    st.repeat  = pd.repeateState == repeat_track   ? 2
               : pd.repeateState == repeat_context ? 1 : 0;
}
```

- [ ] **Step 7: Device build compiles**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`.

- [ ] **Step 8: Commit**

```bash
git add src/ui/icon_map.h src/ui/icon_map.cpp test/test_icon_map/test_icon_map.cpp include/app_state.h src/spotify/client.cpp
git commit -m "Add tested device-icon map and pixel icon art; store device type"
```

---

### Task 3: PMD AnimData parser (PURE)

**Files:**
- Create: `src/util/animdata.h`, `src/util/animdata.cpp`
- Create: `test/test_animdata/test_animdata.cpp`

**Interfaces:**
- Produces:
  - `constexpr int animdata::MAX_FRAMES = 16;`
  - `struct animdata::WalkAnim { bool ok; int frameW, frameH, frames; uint16_t ticks[MAX_FRAMES]; };`
  - `animdata::WalkAnim animdata::parseWalk(const char* xml);` — ticks are PMD game ticks (1/60 s).

- [ ] **Step 1: Write the failing test `test/test_animdata/test_animdata.cpp`**

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/animdata.h"

void setUp() {}
void tearDown() {}

static const char* XML_BASIC =
    "<AnimData><ShadowSize>1</ShadowSize><Anims>"
    "<Anim><Name>Idle</Name><Index>7</Index><FrameWidth>24</FrameWidth><FrameHeight>32</FrameHeight>"
    "<Durations><Duration>40</Duration><Duration>20</Duration></Durations></Anim>"
    "<Anim><Name>WalkAlt</Name><Index>9</Index><FrameWidth>99</FrameWidth><FrameHeight>99</FrameHeight>"
    "<Durations><Duration>1</Duration></Durations></Anim>"
    "<Anim>\n\t<Name>Walk</Name>\n\t<Index>0</Index>\n\t<FrameWidth>32</FrameWidth>\n"
    "\t<FrameHeight>40</FrameHeight>\n\t<Durations>\n\t\t<Duration>8</Duration>\n"
    "\t\t<Duration>10</Duration>\n\t\t<Duration>8</Duration>\n\t\t<Duration>10</Duration>\n"
    "\t</Durations>\n</Anim>"
    "</Anims></AnimData>";

void test_parses_walk_block() {
    animdata::WalkAnim w = animdata::parseWalk(XML_BASIC);
    TEST_ASSERT_TRUE(w.ok);
    TEST_ASSERT_EQUAL_INT(32, w.frameW);
    TEST_ASSERT_EQUAL_INT(40, w.frameH);
    TEST_ASSERT_EQUAL_INT(4, w.frames);
    TEST_ASSERT_EQUAL_UINT16(8, w.ticks[0]);
    TEST_ASSERT_EQUAL_UINT16(10, w.ticks[3]);
}
void test_follows_copy_of() {
    const char* xml =
        "<Anims><Anim><Name>Idle</Name><FrameWidth>24</FrameWidth><FrameHeight>32</FrameHeight>"
        "<Durations><Duration>40</Duration><Duration>20</Duration></Durations></Anim>"
        "<Anim><Name>Walk</Name><Index>0</Index><CopyOf>Idle</CopyOf></Anim></Anims>";
    animdata::WalkAnim w = animdata::parseWalk(xml);
    TEST_ASSERT_TRUE(w.ok);
    TEST_ASSERT_EQUAL_INT(24, w.frameW);
    TEST_ASSERT_EQUAL_INT(2, w.frames);
    TEST_ASSERT_EQUAL_UINT16(40, w.ticks[0]);
}
void test_missing_walk_is_not_ok() {
    const char* xml = "<Anims><Anim><Name>Idle</Name><FrameWidth>24</FrameWidth>"
                      "<FrameHeight>32</FrameHeight><Durations><Duration>4</Duration>"
                      "</Durations></Anim></Anims>";
    TEST_ASSERT_FALSE(animdata::parseWalk(xml).ok);
}
void test_prefix_name_does_not_match() {
    const char* xml = "<Anims><Anim><Name>WalkAlt</Name><FrameWidth>9</FrameWidth>"
                      "<FrameHeight>9</FrameHeight><Durations><Duration>4</Duration>"
                      "</Durations></Anim></Anims>";
    TEST_ASSERT_FALSE(animdata::parseWalk(xml).ok);
}
void test_copy_of_cycle_is_not_ok() {
    const char* xml = "<Anims><Anim><Name>Walk</Name><CopyOf>Hop</CopyOf></Anim>"
                      "<Anim><Name>Hop</Name><CopyOf>Walk</CopyOf></Anim></Anims>";
    TEST_ASSERT_FALSE(animdata::parseWalk(xml).ok);
}
void test_null_and_garbage_are_not_ok() {
    TEST_ASSERT_FALSE(animdata::parseWalk(nullptr).ok);
    TEST_ASSERT_FALSE(animdata::parseWalk("404: Not Found").ok);
}
void test_more_durations_than_max_are_capped() {
    char xml[1200] = "<Anims><Anim><Name>Walk</Name><FrameWidth>32</FrameWidth>"
                     "<FrameHeight>32</FrameHeight><Durations>";
    for (int i = 0; i < 20; ++i) strcat(xml, "<Duration>5</Duration>");
    strcat(xml, "</Durations></Anim></Anims>");
    animdata::WalkAnim w = animdata::parseWalk(xml);
    TEST_ASSERT_TRUE(w.ok);
    TEST_ASSERT_EQUAL_INT(animdata::MAX_FRAMES, w.frames);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_walk_block);
    RUN_TEST(test_follows_copy_of);
    RUN_TEST(test_missing_walk_is_not_ok);
    RUN_TEST(test_prefix_name_does_not_match);
    RUN_TEST(test_copy_of_cycle_is_not_ok);
    RUN_TEST(test_null_and_garbage_are_not_ok);
    RUN_TEST(test_more_durations_than_max_are_capped);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_animdata\test_animdata.cpp`
Expected: `COMPILE FAILED` (`animdata.h` missing).

- [ ] **Step 3: Create `src/util/animdata.h`**

```cpp
#pragma once
#include <cstdint>
// PMDCollab SpriteCollab AnimData.xml -> the "Walk" animation. PURE, host-tested.
namespace animdata {
constexpr int MAX_FRAMES = 16;
struct WalkAnim {
    bool ok;
    int frameW, frameH, frames;
    uint16_t ticks[MAX_FRAMES];   // per-frame duration in game ticks (1/60 s)
};
// Finds <Anim><Name>Walk</Name>..., following <CopyOf> aliases (max 4 hops).
// ok=false when Walk is absent, a CopyOf target is missing/cyclic, or size/durations
// are missing.
WalkAnim parseWalk(const char* xml);
}
```

- [ ] **Step 4: Create `src/util/animdata.cpp`**

```cpp
#include "animdata.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace animdata {

// Locates the <Anim>...</Anim> block whose <Name> is exactly `name`.
static bool findAnim(const char* xml, const char* name, const char** b, const char** e) {
    size_t nl = strlen(name);
    for (const char* p = strstr(xml, "<Anim>"); p; p = strstr(p, "<Anim>")) {
        const char* end = strstr(p, "</Anim>");
        if (!end) return false;
        const char* n = strstr(p, "<Name>");
        if (n && n < end) {
            n += 6;
            if (strncmp(n, name, nl) == 0 && n[nl] == '<') { *b = p; *e = end; return true; }
        }
        p = end + 7;
    }
    return false;
}

// Start of the text inside <tag> within [b,e), or nullptr.
static const char* tagBody(const char* b, const char* e, const char* tag) {
    char open[32];
    snprintf(open, sizeof(open), "<%s>", tag);
    const char* p = strstr(b, open);
    if (!p || p >= e) return nullptr;
    return p + strlen(open);
}

static int tagInt(const char* b, const char* e, const char* tag) {
    const char* p = tagBody(b, e, tag);
    return p ? atoi(p) : -1;
}

static bool tagText(const char* b, const char* e, const char* tag, char* out, size_t n) {
    const char* p = tagBody(b, e, tag);
    if (!p) return false;
    size_t i = 0;
    while (p[i] && p[i] != '<' && i + 1 < n) { out[i] = p[i]; ++i; }
    out[i] = '\0';
    return i > 0;
}

WalkAnim parseWalk(const char* xml) {
    WalkAnim w{};
    if (!xml) return w;
    const char *b, *e;
    if (!findAnim(xml, "Walk", &b, &e)) return w;
    char target[32];
    int hops = 0;
    while (tagText(b, e, "CopyOf", target, sizeof(target))) {
        if (++hops > 4 || !findAnim(xml, target, &b, &e)) return w;
    }
    w.frameW = tagInt(b, e, "FrameWidth");
    w.frameH = tagInt(b, e, "FrameHeight");
    for (const char* p = strstr(b, "<Duration>"); p && p < e && w.frames < MAX_FRAMES;
         p = strstr(p, "<Duration>")) {
        p += 10;
        w.ticks[w.frames++] = (uint16_t)atoi(p);
    }
    w.ok = w.frameW > 0 && w.frameH > 0 && w.frames > 0;
    return w;
}

}
```

- [ ] **Step 5: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_animdata\test_animdata.cpp src\util\animdata.cpp`
Expected: `7 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 6: Commit**

```bash
git add src/util/animdata.h src/util/animdata.cpp test/test_animdata/test_animdata.cpp
git commit -m "Add tested PMD AnimData Walk parser (CopyOf, caps, bad input)"
```

---

### Task 4: Walk-frame arithmetic (PURE)

**Files:**
- Create: `src/util/walkanim.h`, `src/util/walkanim.cpp`
- Create: `test/test_walkanim/test_walkanim.cpp`

**Interfaces:**
- Consumes: nothing (ticks arrays shaped like `animdata::WalkAnim::ticks`).
- Produces (namespace `walkanim`):
  - `struct Box { int x0, y0, x1, y1; };` (inclusive; empty when `x1 < x0`), `Box emptyBox();`, `void include(Box& b, int x, int y);`, `bool isEmpty(const Box& b);`
  - `struct Fit { int w, h; };` `Fit fitBand(int bw, int bh, int maxH, int maxW);` — nearest-neighbour target size, aspect kept, never upscales, each side ≥ 1.
  - `int srcIndex(int o, int outLen, int srcStart, int srcLen);` → `srcStart + o*srcLen/outLen`.
  - `int keepEvery(int frames, int w, int h, int capBytes);` — smallest k ≥ 1 with `ceil(frames/k)*w*h*3 <= capBytes`; 0 if one frame doesn't fit.
  - `int keptCount(int frames, int k);` → `ceil(frames/k)`.
  - `void mergedDurationsMs(const uint16_t* ticks, int n, int k, uint16_t* outMs);` — kept frame i spans source frames `[i*k, min(i*k+k, n))`; ms = ticks*1000/60.
  - `int frameAt(uint32_t elapsedMs, const uint16_t* durMs, int n);` — cyclic; 0 if n ≤ 0 or total is 0.

- [ ] **Step 1: Write the failing test `test/test_walkanim/test_walkanim.cpp`**

```cpp
#include <unity.h>
#include "../../src/util/walkanim.h"

using namespace walkanim;
void setUp() {}
void tearDown() {}

void test_box_union() {
    Box b = emptyBox();
    TEST_ASSERT_TRUE(isEmpty(b));
    include(b, 5, 7); include(b, 2, 9); include(b, 4, 3);
    TEST_ASSERT_FALSE(isEmpty(b));
    TEST_ASSERT_EQUAL_INT(2, b.x0); TEST_ASSERT_EQUAL_INT(3, b.y0);
    TEST_ASSERT_EQUAL_INT(5, b.x1); TEST_ASSERT_EQUAL_INT(9, b.y1);
}
void test_fit_keeps_native_when_it_fits() {
    Fit f = fitBand(24, 30, 32, 64);
    TEST_ASSERT_EQUAL_INT(24, f.w); TEST_ASSERT_EQUAL_INT(30, f.h);
}
void test_fit_downscales_tall_keeping_aspect() {
    Fit f = fitBand(40, 64, 32, 64);
    TEST_ASSERT_EQUAL_INT(20, f.w); TEST_ASSERT_EQUAL_INT(32, f.h);
}
void test_fit_downscales_wide() {
    Fit f = fitBand(128, 32, 32, 64);
    TEST_ASSERT_EQUAL_INT(64, f.w); TEST_ASSERT_EQUAL_INT(16, f.h);
}
void test_fit_never_zero() {
    Fit f = fitBand(1, 200, 32, 64);
    TEST_ASSERT_EQUAL_INT(1, f.w); TEST_ASSERT_EQUAL_INT(32, f.h);
}
void test_src_index_maps_nearest() {
    TEST_ASSERT_EQUAL_INT(10, srcIndex(0, 16, 10, 32));
    TEST_ASSERT_EQUAL_INT(40, srcIndex(15, 16, 10, 32));
    TEST_ASSERT_EQUAL_INT(13, srcIndex(3, 8, 10, 8));   // 1:1
}
void test_keep_every_fits_cap() {
    TEST_ASSERT_EQUAL_INT(1, keepEvery(4, 28, 32, 16384));    // 4*28*32*3 = 10752
    TEST_ASSERT_EQUAL_INT(2, keepEvery(10, 32, 32, 16384));   // 10 frames = 30720 -> 5 = 15360
    TEST_ASSERT_EQUAL_INT(0, keepEvery(4, 80, 80, 16384));    // one frame = 19200
}
void test_kept_count_rounds_up() {
    TEST_ASSERT_EQUAL_INT(5, keptCount(10, 2));
    TEST_ASSERT_EQUAL_INT(4, keptCount(7, 2));
    TEST_ASSERT_EQUAL_INT(4, keptCount(4, 1));
}
void test_merged_durations() {
    const uint16_t ticks[5] = {6, 6, 12, 12, 30};
    uint16_t ms[3];
    mergedDurationsMs(ticks, 5, 2, ms);
    TEST_ASSERT_EQUAL_UINT16(200, ms[0]);   // (6+6) ticks
    TEST_ASSERT_EQUAL_UINT16(400, ms[1]);   // (12+12)
    TEST_ASSERT_EQUAL_UINT16(500, ms[2]);   // 30
}
void test_frame_at_cycles() {
    const uint16_t d[3] = {100, 200, 100};
    TEST_ASSERT_EQUAL_INT(0, frameAt(0, d, 3));
    TEST_ASSERT_EQUAL_INT(0, frameAt(99, d, 3));
    TEST_ASSERT_EQUAL_INT(1, frameAt(100, d, 3));
    TEST_ASSERT_EQUAL_INT(2, frameAt(399, d, 3));
    TEST_ASSERT_EQUAL_INT(0, frameAt(400, d, 3));   // wraps
    TEST_ASSERT_EQUAL_INT(1, frameAt(400 * 1000 + 150, d, 3));
}
void test_frame_at_degenerate() {
    const uint16_t z[2] = {0, 0};
    TEST_ASSERT_EQUAL_INT(0, frameAt(1234, z, 2));
    TEST_ASSERT_EQUAL_INT(0, frameAt(1234, z, 0));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_box_union);
    RUN_TEST(test_fit_keeps_native_when_it_fits);
    RUN_TEST(test_fit_downscales_tall_keeping_aspect);
    RUN_TEST(test_fit_downscales_wide);
    RUN_TEST(test_fit_never_zero);
    RUN_TEST(test_src_index_maps_nearest);
    RUN_TEST(test_keep_every_fits_cap);
    RUN_TEST(test_kept_count_rounds_up);
    RUN_TEST(test_merged_durations);
    RUN_TEST(test_frame_at_cycles);
    RUN_TEST(test_frame_at_degenerate);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walkanim\test_walkanim.cpp`
Expected: `COMPILE FAILED` (`walkanim.h` missing).

- [ ] **Step 3: Create `src/util/walkanim.h`**

```cpp
#pragma once
#include <cstdint>
// Arithmetic for turning a sprite sheet row into walker frames. PURE, host-tested.
namespace walkanim {
struct Box { int x0, y0, x1, y1; };                 // inclusive; empty when x1 < x0
Box emptyBox();
void include(Box& b, int x, int y);
bool isEmpty(const Box& b);

struct Fit { int w, h; };
// Target size for a bw x bh crop in a maxW x maxH slot: native if it fits, else
// nearest-neighbour downscale keeping aspect. Never upscales; sides >= 1.
Fit fitBand(int bw, int bh, int maxH, int maxW);
// Source coordinate for output coordinate o when mapping srcLen -> outLen.
int srcIndex(int o, int outLen, int srcStart, int srcLen);

// Smallest k >= 1 so that keeping every k-th of `frames` (w x h, 3 B/px) fits
// capBytes; 0 if even one frame does not fit.
int keepEvery(int frames, int w, int h, int capBytes);
int keptCount(int frames, int k);
// Kept frame i lasts the sum of source ticks [i*k, min(i*k+k, n)), in ms (60 ticks/s).
void mergedDurationsMs(const uint16_t* ticks, int n, int k, uint16_t* outMs);
// Frame index at elapsedMs in a looping animation with per-frame ms durations.
int frameAt(uint32_t elapsedMs, const uint16_t* durMs, int n);
}
```

- [ ] **Step 4: Create `src/util/walkanim.cpp`**

```cpp
#include "walkanim.h"

namespace walkanim {

Box emptyBox() { return {1 << 30, 1 << 30, -1, -1}; }

void include(Box& b, int x, int y) {
    if (x < b.x0) b.x0 = x;
    if (x > b.x1) b.x1 = x;
    if (y < b.y0) b.y0 = y;
    if (y > b.y1) b.y1 = y;
}

bool isEmpty(const Box& b) { return b.x1 < b.x0 || b.y1 < b.y0; }

Fit fitBand(int bw, int bh, int maxH, int maxW) {
    if (bw <= maxW && bh <= maxH) return {bw, bh};
    int w = bw * maxH / bh, h = maxH;
    if (w > maxW) { w = maxW; h = bh * maxW / bw; }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return {w, h};
}

int srcIndex(int o, int outLen, int srcStart, int srcLen) {
    return srcStart + o * srcLen / outLen;
}

int keptCount(int frames, int k) { return (frames + k - 1) / k; }

int keepEvery(int frames, int w, int h, int capBytes) {
    int per = w * h * 3;
    if (frames < 1 || per > capBytes) return 0;
    for (int k = 1; k <= frames; ++k)
        if (keptCount(frames, k) * per <= capBytes) return k;
    return frames;
}

void mergedDurationsMs(const uint16_t* ticks, int n, int k, uint16_t* outMs) {
    for (int i = 0; i * k < n; ++i) {
        uint32_t t = 0;
        for (int j = i * k; j < i * k + k && j < n; ++j) t += ticks[j];
        outMs[i] = (uint16_t)(t * 1000 / 60);
    }
}

int frameAt(uint32_t elapsedMs, const uint16_t* durMs, int n) {
    if (n <= 0) return 0;
    uint32_t total = 0;
    for (int i = 0; i < n; ++i) total += durMs[i];
    if (total == 0) return 0;
    uint32_t t = elapsedMs % total;
    for (int i = 0; i < n; ++i) {
        if (t < durMs[i]) return i;
        t -= durMs[i];
    }
    return n - 1;
}

}
```

- [ ] **Step 5: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walkanim\test_walkanim.cpp src\util\walkanim.cpp`
Expected: `11 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 6: Commit**

```bash
git add src/util/walkanim.h src/util/walkanim.cpp test/test_walkanim/test_walkanim.cpp
git commit -m "Add tested walk-frame arithmetic (bbox, fit, frame skip, timing)"
```

---

### Task 5: Fetch + cache paths + walk-frame store (PMD loader with fallback)

The current walker (`img::loadWalkSprite` in `png.cpp`, drawn by `ui::drawWalker`) is replaced by `walk::` in this task. To keep the build working, `ui::drawWalker` is switched to the new store here (still on the old layout); Task 7 moves it into the battle layout.

**Files:**
- Create: `src/images/fetch.h`, `src/images/fetch.cpp`
- Modify: `src/images/cache.h`, `src/images/cache.cpp`
- Modify: `src/images/png.h`, `src/images/png.cpp`
- Create: `src/images/walksprite.h`, `src/images/walksprite.cpp`
- Modify: `src/ui/screen_now.cpp` (`drawWalker` reads `walk::`), `src/main.cpp` (load call + heap log)

**Interfaces:**
- Consumes: `animdata::parseWalk`, `walkanim::*` (Tasks 3–4), `walkrect::plan` (exists), `interp::walkX` (exists).
- Produces:
  - `bool fetch::httpsGet(const char* url, size_t maxLen, uint8_t** out, size_t* outLen, int* httpCode = nullptr);` — on success `*out` is `malloc`ed with **`*outLen + 1` bytes, `(*out)[*outLen] == 0`**; caller frees.
  - `cache::begin()`, `bool cache::hasPath(const String&)`, `bool cache::savePath(const String&, const uint8_t*, size_t)`, `bool cache::readAll(const String& path, size_t maxLen, uint8_t** out, size_t* outLen)` (same NUL-terminated contract), `void cache::removePath(const String&)`; existing `has/save/open(dex)` keep working.
  - `PNG& img::decoder();` `bool img::loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen);` (`drawSprite`, `loadWalkSprite`, `walk*()` removed from `img`).
  - namespace `walk`: `BAND_H = 32`, `MAX_W = 64`, `CAP_BYTES = 16384`, `MAX_FRAMES = 16`; `struct Info { bool ready; bool pmd; int w, h, frames; };`; `bool loadPmd(int dex);` `bool loadFallback(const char* spriteUrl, int dex);` `const Info& info();` `const uint16_t* pixels(int frame);` `const uint8_t* mask(int frame);` `uint16_t durationMs(int frame);`

- [ ] **Step 1: Create `src/images/fetch.h` / `fetch.cpp`**

```cpp
// fetch.h
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace fetch {
// HTTPS GET of `url` into a heap buffer. Fails on non-200, unknown/oversized length
// (> maxLen) or a stalled/short read. On success *out holds *outLen bytes plus a
// trailing NUL (so text can be parsed in place); caller free()s it.
bool httpsGet(const char* url, size_t maxLen, uint8_t** out, size_t* outLen,
              int* httpCode = nullptr);
}
```

```cpp
// fetch.cpp
#include "fetch.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace fetch {

bool httpsGet(const char* url, size_t maxLen, uint8_t** out, size_t* outLen, int* httpCode) {
    *out = nullptr;
    *outLen = 0;
    if (httpCode) *httpCode = 0;
    if (!url || !url[0]) return false;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    https.useHTTP10(true);   // plain (non-chunked) body so getSize() is the length
    if (!https.begin(client, url)) return false;
    int code = https.GET();
    if (httpCode) *httpCode = code;
    if (code != 200) { https.end(); return false; }
    int len = https.getSize();
    if (len <= 0 || (size_t)len > maxLen) { https.end(); return false; }

    uint8_t* data = (uint8_t*)malloc((size_t)len + 1);
    if (!data) { https.end(); return false; }
    WiFiClient* s = https.getStreamPtr();
    int got = 0;
    uint32_t last = millis();
    while (got < len && (https.connected() || s->available())) {
        size_t a = s->available();
        if (a) {
            size_t want = (size_t)(len - got);
            if (a < want) want = a;
            got += s->readBytes(data + got, want);
            last = millis();
        } else {
            if (millis() - last > 8000) break;   // stalled
            delay(1);
        }
    }
    https.end();
    if (got != len) { free(data); return false; }
    data[len] = 0;
    *out = data;
    *outLen = (size_t)len;
    return true;
}

}
```

- [ ] **Step 2: Generalise `src/images/cache.*` to paths**

Replace `cache.h` with:

```cpp
#pragma once
#include <Arduino.h>
#include <FS.h>

// SD-card persistent cache (VSPI; pins in include/pins.h). Holds PokeAPI sprite PNGs
// under /sprites and PMD walk sheets under /pmd. If the card never mounted every
// call is a no-op (false/empty), so a missing card degrades to "always download".
namespace cache {

bool begin();

// Path API.
bool hasPath(const String& path);
bool savePath(const String& path, const uint8_t* data, size_t n);
// Reads the whole file (<= maxLen) into a malloc'ed buffer of n+1 bytes, NUL-terminated.
bool readAll(const String& path, size_t maxLen, uint8_t** out, size_t* outLen);
void removePath(const String& path);

// PokeAPI sprite API (dex-keyed, /sprites/<dex>.png).
String spritePath(int dex);
bool has(int dex);
bool save(int dex, const uint8_t* data, size_t n);
// Spelled fs::File because TFT_eSPI.h defines FS_NO_GLOBALS before FS.h.
fs::File open(int dex);

}
```

Replace `cache.cpp` with:

```cpp
#include "cache.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"

namespace cache {

static SPIClass sdSPI(VSPI);
static bool ready = false;

bool begin() {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    ready = SD.begin(SD_CS, sdSPI);
    if (ready && !SD.exists("/sprites")) SD.mkdir("/sprites");
    if (ready && !SD.exists("/pmd")) SD.mkdir("/pmd");
    Serial.printf("[cache] SD %s\n", ready ? "ready" : "unavailable");
    return ready;
}

bool hasPath(const String& path) { return ready && SD.exists(path); }

bool savePath(const String& path, const uint8_t* data, size_t n) {
    if (!ready) return false;
    fs::File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t w = f.write(data, n);
    f.close();
    if (w != n) { SD.remove(path); return false; }   // don't leave a truncated file
    return true;
}

bool readAll(const String& path, size_t maxLen, uint8_t** out, size_t* outLen) {
    *out = nullptr;
    *outLen = 0;
    if (!ready) return false;
    fs::File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t n = f.size();
    if (n == 0 || n > maxLen) { f.close(); return false; }
    uint8_t* data = (uint8_t*)malloc(n + 1);
    if (!data) { f.close(); return false; }
    size_t got = f.read(data, n);
    f.close();
    if (got != n) { free(data); return false; }
    data[n] = 0;
    *out = data;
    *outLen = n;
    return true;
}

void removePath(const String& path) { if (ready) SD.remove(path); }

String spritePath(int dex) { return "/sprites/" + String(dex) + ".png"; }
bool has(int dex) { return hasPath(spritePath(dex)); }
bool save(int dex, const uint8_t* data, size_t n) { return savePath(spritePath(dex), data, n); }
fs::File open(int dex) { return ready ? SD.open(spritePath(dex), FILE_READ) : fs::File(); }

}
```

- [ ] **Step 3: Slim `src/images/png.*` to the shared decoder + sprite bytes**

Replace `png.h` with:

```cpp
#pragma once
#include <PNGdec.h>
#include <stddef.h>
#include <stdint.h>

// PokeAPI sprite bytes + the single shared PNGdec instance. PNG objects are large
// (~45 KB of inflate state), so every PNG decode in the firmware uses this one.
namespace img {

PNG& decoder();

// Raw PNG bytes for dex `dex`: SD cache if present, else HTTPS download of `url`
// (<= 40000 bytes) saved to the cache. *outData is malloc'ed (free() it).
bool loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen);

}
```

Replace `png.cpp` with:

```cpp
#include "png.h"
#include "cache.h"
#include "fetch.h"

namespace img {

static PNG g_png;
PNG& decoder() { return g_png; }

bool loadSpriteBytes(int dex, const char* url, uint8_t** outData, size_t* outLen) {
    if (cache::readAll(cache::spritePath(dex), 40000, outData, outLen)) return true;
    if (!fetch::httpsGet(url, 40000, outData, outLen)) return false;
    cache::save(dex, *outData, *outLen);
    return true;
}

}
```

- [ ] **Step 4: Create `src/images/walksprite.h`**

```cpp
#pragma once
#include <stdint.h>

// The walker's frames for the current song. Loaded once per track change; the
// per-frame blit needs no network or decode. Two sources:
//  - PMD SpriteCollab walk cycle (Right-facing row), several frames;
//  - fallback: the PokeAPI sprite cropped to one frame (UI adds bob + mirror).
namespace walk {
constexpr int BAND_H     = 32;      // walk band height in the status box
constexpr int MAX_W      = 64;      // widest frame kept
constexpr int CAP_BYTES  = 16384;   // RGB565 + 1-byte mask per pixel, all frames
constexpr int MAX_FRAMES = 16;

struct Info { bool ready; bool pmd; int w, h, frames; };

bool loadPmd(int dex);
bool loadFallback(const char* spriteUrl, int dex);

const Info& info();
const uint16_t* pixels(int frame);   // big-endian RGB565, w*h
const uint8_t*  mask(int frame);     // 1 = opaque, w*h
uint16_t durationMs(int frame);      // 0 for the fallback frame
}
```

- [ ] **Step 5: Create `src/images/walksprite.cpp`**

```cpp
#include "walksprite.h"
#include <Arduino.h>
#include "png.h"
#include "cache.h"
#include "fetch.h"
#include "../util/animdata.h"
#include "../util/walkanim.h"

namespace walk {

// Store layout: all frames' pixels first (keeps uint16 access aligned), then masks.
alignas(4) static uint8_t g_pool[CAP_BYTES];
static Info g_info{false, false, 0, 0, 0};
static uint16_t g_durMs[MAX_FRAMES];

const Info& info() { return g_info; }
static uint16_t* pixelsW(int f) { return (uint16_t*)g_pool + f * g_info.w * g_info.h; }
static uint8_t* maskW(int f) {
    return g_pool + g_info.frames * g_info.w * g_info.h * 2 + f * g_info.w * g_info.h;
}
const uint16_t* pixels(int f) { return pixelsW(f); }
const uint8_t* mask(int f) { return maskW(f); }
uint16_t durationMs(int f) { return (f >= 0 && f < g_info.frames) ? g_durMs[f] : 0; }

static inline bool bitAt(const uint8_t* bits, int x) { return (bits[x >> 3] >> (7 - (x & 7))) & 1; }

// ---------------------------------------------------------------- PMD sheet
static const int DIR_RIGHT = 2;          // PMD row order: Down, DownRight, Right, ...
static const int SHEET_MAX_W = 512;
static int s_fw, s_fh, s_rowY0, s_frames, s_pass, s_k, s_kept;
static walkanim::Box s_box;
static walkanim::Fit s_fit;
static uint16_t s_line[SHEET_MAX_W];     // static: keep the PNGdec callback stack small
static uint8_t s_bits[SHEET_MAX_W / 8];

static int pmdDraw(PNGDRAW* d) {
    int ry = d->y - s_rowY0;
    if (ry < 0 || ry >= s_fh) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    if (s_pass == 1) {   // measure the bounding box shared by every frame of the row
        for (int x = 0; x < s_frames * s_fw; ++x)
            if (bitAt(s_bits, x)) walkanim::include(s_box, x % s_fw, ry);
        return 1;
    }
    int bw = s_box.x1 - s_box.x0 + 1, bh = s_box.y1 - s_box.y0 + 1;
    for (int oy = 0; oy < s_fit.h; ++oy) {
        if (walkanim::srcIndex(oy, s_fit.h, s_box.y0, bh) != ry) continue;
        for (int fi = 0; fi < s_kept; ++fi) {
            int f = fi * s_k;
            uint16_t* px = pixelsW(fi);
            uint8_t* m = maskW(fi);
            for (int ox = 0; ox < s_fit.w; ++ox) {
                int sx = f * s_fw + walkanim::srcIndex(ox, s_fit.w, s_box.x0, bw);
                px[oy * s_fit.w + ox] = s_line[sx];
                m[oy * s_fit.w + ox] = bitAt(s_bits, sx);
            }
        }
    }
    return 1;
}

static bool decodeRow(uint8_t* data, size_t n, const animdata::WalkAnim& a) {
    PNG& png = img::decoder();
    s_fw = a.frameW; s_fh = a.frameH; s_rowY0 = DIR_RIGHT * a.frameH;
    for (s_pass = 1; s_pass <= 2; ++s_pass) {
        if (png.openRAM(data, (int)n, pmdDraw) != PNG_SUCCESS) return false;
        if (s_pass == 1) {
            int sw = png.getWidth();
            if (sw > SHEET_MAX_W || png.getHeight() < s_rowY0 + s_fh) { png.close(); return false; }
            s_frames = sw / s_fw;
            if (a.frames < s_frames) s_frames = a.frames;
            if (s_frames < 1) { png.close(); return false; }
            s_box = walkanim::emptyBox();
        }
        int rc = png.decode(nullptr, 0);
        png.close();
        if (rc != PNG_SUCCESS) return false;
        if (s_pass == 1) {
            if (walkanim::isEmpty(s_box)) return false;
            s_fit = walkanim::fitBand(s_box.x1 - s_box.x0 + 1, s_box.y1 - s_box.y0 + 1, BAND_H, MAX_W);
            s_k = walkanim::keepEvery(s_frames, s_fit.w, s_fit.h, CAP_BYTES);
            if (s_k == 0) return false;
            s_kept = walkanim::keptCount(s_frames, s_k);
            g_info = {false, true, s_fit.w, s_fit.h, s_kept};   // layout for pass 2 writes
        }
    }
    walkanim::mergedDurationsMs(a.ticks, s_frames, s_k, g_durMs);
    g_info.ready = true;
    return true;
}

// Cache-or-download into a NUL-terminated heap buffer.
static bool getCached(const String& path, const String& url, size_t maxLen,
                      uint8_t** out, size_t* n, int* code) {
    *code = 0;
    if (cache::readAll(path, maxLen, out, n)) return true;
    if (!fetch::httpsGet(url.c_str(), maxLen, out, n, code)) return false;
    cache::savePath(path, *out, *n);
    return true;
}

bool loadPmd(int dex) {
    g_info.ready = false;
    if (dex < 1) return false;
    char base[96];
    snprintf(base, sizeof(base),
             "https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/", dex);
    String xmlPath = "/pmd/" + String(dex) + ".xml";
    String pngPath = "/pmd/" + String(dex) + ".png";

    uint8_t* xml = nullptr; size_t xn = 0; int code = 0;
    if (!getCached(xmlPath, String(base) + "AnimData.xml", 32768, &xml, &xn, &code)) {
        Serial.printf("[walk] fallback (xml http %d)\n", code);
        return false;
    }
    animdata::WalkAnim a = animdata::parseWalk((const char*)xml);
    free(xml);
    if (!a.ok) {
        cache::removePath(xmlPath);
        Serial.println("[walk] fallback (no Walk anim)");
        return false;
    }
    uint8_t* png = nullptr; size_t pn = 0;
    if (!getCached(pngPath, String(base) + "Walk-Anim.png", 40000, &png, &pn, &code)) {
        Serial.printf("[walk] fallback (png http %d)\n", code);
        return false;
    }
    bool ok = decodeRow(png, pn, a);
    free(png);
    if (!ok) {   // corrupt or unusable: drop the cache so the next play re-downloads
        cache::removePath(xmlPath);
        cache::removePath(pngPath);
        g_info.ready = false;
        Serial.println("[walk] fallback (sheet decode)");
        return false;
    }
    Serial.printf("[walk] pmd %d frames %dx%d (k=%d)\n", g_info.frames, g_info.w, g_info.h, s_k);
    return true;
}

// ---------------------------------------------------------------- fallback
static const int FULL_MAX = 128;
static uint16_t* f_px = nullptr;
static uint8_t* f_mask = nullptr;
static int f_w = 0, f_h = 0;

static int fullDraw(PNGDRAW* d) {
    if (d->y >= f_h) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    for (int x = 0; x < f_w; ++x) {
        f_px[d->y * f_w + x] = s_line[x];
        f_mask[d->y * f_w + x] = bitAt(s_bits, x);
    }
    return 1;
}

bool loadFallback(const char* spriteUrl, int dex) {
    g_info.ready = false;
    uint8_t* data = nullptr; size_t n = 0;
    if (!img::loadSpriteBytes(dex, spriteUrl, &data, &n)) {
        Serial.println("[walk] no sprite");
        return false;
    }
    PNG& png = img::decoder();
    bool ok = false;
    if (png.openRAM(data, (int)n, fullDraw) == PNG_SUCCESS) {
        f_w = png.getWidth(); f_h = png.getHeight();
        bool alpha = png.hasAlpha();
        if (f_w <= FULL_MAX && f_h <= FULL_MAX) {
            f_px = (uint16_t*)malloc(f_w * f_h * 2);
            f_mask = (uint8_t*)calloc(f_w * f_h, 1);       // undecoded rows = clear
            if (f_px && f_mask && png.decode(nullptr, 0) == PNG_SUCCESS) {
                if (!alpha) {   // no alpha channel: key out the corner colour
                    uint16_t key = f_px[0];
                    for (int i = 0; i < f_w * f_h; ++i) f_mask[i] = f_px[i] != key;
                }
                walkanim::Box b = walkanim::emptyBox();
                for (int y = 0; y < f_h; ++y)
                    for (int x = 0; x < f_w; ++x)
                        if (f_mask[y * f_w + x]) walkanim::include(b, x, y);
                if (!walkanim::isEmpty(b)) {
                    int bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
                    walkanim::Fit fit = walkanim::fitBand(bw, bh, BAND_H, MAX_W);
                    g_info = {false, false, fit.w, fit.h, 1};
                    uint16_t* px = pixelsW(0);
                    uint8_t* m = maskW(0);
                    for (int y = 0; y < fit.h; ++y) {
                        int sy = walkanim::srcIndex(y, fit.h, b.y0, bh);
                        for (int x = 0; x < fit.w; ++x) {
                            int sx = walkanim::srcIndex(x, fit.w, b.x0, bw);
                            px[y * fit.w + x] = f_px[sy * f_w + sx];
                            m[y * fit.w + x] = f_mask[sy * f_w + sx];
                        }
                    }
                    g_durMs[0] = 0;
                    g_info.ready = ok = true;
                }
            } else if (!f_px || !f_mask) {
                Serial.printf("[walk] no heap for %dx%d sprite\n", f_w, f_h);
            }
            free(f_px); f_px = nullptr;
            free(f_mask); f_mask = nullptr;
        }
        png.close();
    }
    free(data);
    if (ok) Serial.printf("[walk] fallback sprite %dx%d\n", g_info.w, g_info.h);
    return ok;
}

}
```

Note: `fitBand(…, BAND_H=32, MAX_W=64)` → one fallback frame is at most 64×32×3 = 6 KB, always within `CAP_BYTES`.

- [ ] **Step 6: Point `ui::drawWalker` at the new store (old layout, temporary)**

In `src/ui/screen_now.cpp`:
- replace `#include "../images/png.h"` with `#include "../images/walksprite.h"`;
- in `drawWalker`, change the buffer declaration to
  `static uint16_t buf[(walk::MAX_W + 2 * SLACK) * (walk::BAND_H + 1 + 2 * SLACK)];`
- replace the four lines that read the sprite (`if (!img::walkReady()) return;` … `int w = img::walkW(), h = img::walkH();`) with:

```cpp
    const walk::Info& wi = walk::info();
    if (!wi.ready) return;
    int fr = wi.pmd ? (step % wi.frames) : 0;   // Task 7 replaces this with real timing
    const uint16_t* spr = walk::pixels(fr);
    const uint8_t* msk = walk::mask(fr);
    int w = wi.w, h = wi.h;
```

- and make bob/mirror apply only to the fallback:

```cpp
    int bob = (!wi.pmd && (step % 2)) ? 1 : 0;
    bool mirror = !wi.pmd && ((step / 4) % 2);
```

In `src/ui/screen_now.h` remove the `constexpr int WALK_SIZE = 34;` line.

- [ ] **Step 7: Load PMD → fallback in `src/main.cpp`, with a heap log**

Add `#include "images/walksprite.h"` near the other image includes. In the full-redraw branch, replace

```cpp
        img::drawSprite(tft, g_state.pokeSpriteUrl, g_state.pokedexNum, 60, 185);
        img::loadWalkSprite(g_state.pokeSpriteUrl, g_state.pokedexNum, ui::WALK_SIZE);
```

with

```cpp
        if (!walk::loadPmd(g_state.pokedexNum))
            walk::loadFallback(g_state.pokeSpriteUrl, g_state.pokedexNum);
        Serial.printf("[heap] free=%u max=%u\n",
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
```

(The big static sprite in the left box disappears now; Task 7 removes that box.)

- [ ] **Step 8: Host suites still pass, device build compiles**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walkrect\test_walkrect.cpp src\util\walkrect.cpp`
Expected: `OK`.
Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`, no references to `img::drawSprite`/`img::walk*` remain (`grep -rn "drawSprite\|walkReady\|loadWalkSprite" src` prints nothing).

- [ ] **Step 9: Flash and observe**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`, then `python .devtools\serial_read.py COM11 60`, skipping tracks on the phone 3–4 times.
Expected per track: `[poke] #N …`, then either `[walk] pmd <n> frames <w>x<h> (k=<k>)` (most tracks) or `[walk] fallback (…)` followed by `[walk] fallback sprite <w>x<h>`, then `[heap] free=… max=…`. On screen the Pokémon walks on the old HP bar using real frames. No `Guru Meditation`/reboot.

- [ ] **Step 10: Exercise the fallback and the corrupt-cache path**

With the SD card in a PC (or via a temporary serial-free method), truncate one `/pmd/<dex>.png` to 100 bytes, reinsert, and play until that dex appears (or temporarily hard-code `pokedexNum` in `pokeapi::pickRandom` to that dex for one boot, then revert). Expected: `[walk] fallback (sheet decode)` then `[walk] fallback sprite …`, and on the next play of that dex `[walk] pmd …` (file was removed and re-downloaded). Revert any temporary code before committing.

- [ ] **Step 11: Commit**

```bash
git add src/images/fetch.h src/images/fetch.cpp src/images/cache.h src/images/cache.cpp src/images/png.h src/images/png.cpp src/images/walksprite.h src/images/walksprite.cpp src/ui/screen_now.h src/ui/screen_now.cpp src/main.cpp
git commit -m "Walk-frame store: PMD walk-cycle loader with SD cache and PokeAPI fallback"
```

---

### Task 6: Gen-3 draw primitives, icons and text fitting

**Files:**
- Create: `src/util/textfit.h`, `src/util/textfit.cpp`, `test/test_textfit/test_textfit.cpp`
- Create: `src/ui/battle.h`, `src/ui/battle.cpp`
- Create: `src/ui/icons.h`, `src/ui/icons.cpp`

**Interfaces:**
- Consumes: `theme::*` (Task 1), `icons::row/pixel/Icon` (Task 2).
- Produces:
  - `struct textfit::TwoLines { std::string a, b; };` `textfit::TwoLines textfit::wrapTwo(const std::string& s, int maxW, int (*width)(const std::string&, void*), void* ctx);` — fits on one line → `{s, ""}`; else greedy break at the last space that fits, line b truncated with `...` to fit.
  - `enum class ui::Tab { None, Left, Right };`
  - `void ui::background(TFT_eSPI&)`, `void ui::topStrip(TFT_eSPI&)`, `void ui::battleBox(TFT_eSPI&, int x, int y, int w, int h, ui::Tab tab)`, `void ui::shadowText(TFT_eSPI&, const char* s, int x, int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum)`, `void ui::hpBarBattle(TFT_eSPI&, int x, int y, int w, int h, float frac)`, `constexpr int ui::HP_TAG_W = 22;`, `void ui::expBar(TFT_eSPI&, int x, int y, int w, float frac)`, `void ui::dialogueBox(TFT_eSPI&, int x, int y, int w, int h)`.
  - `void ui::drawIcon(TFT_eSPI&, icons::Icon, int x, int y, uint16_t fg, uint16_t bg)`, `void ui::drawCd(TFT_eSPI&, int cx, int cy, int frame, uint16_t bg)`.

- [ ] **Step 1: Write the failing test `test/test_textfit/test_textfit.cpp`**

```cpp
#include <unity.h>
#include <string>
#include "../../src/util/textfit.h"

void setUp() {}
void tearDown() {}

// Fixed-width fake font: 6 px per character.
static int w6(const std::string& s, void*) { return (int)s.size() * 6; }

void test_short_line_stays_single() {
    textfit::TwoLines t = textfit::wrapTwo("hello world", 120, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("hello world", t.a.c_str());
    TEST_ASSERT_EQUAL_STRING("", t.b.c_str());
}
void test_breaks_at_last_fitting_space() {
    // 20 chars fit per line (120 px)
    textfit::TwoLines t = textfit::wrapTwo("and I said hey what is going on", 120, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("and I said hey what", t.a.c_str());
    TEST_ASSERT_EQUAL_STRING("is going on", t.b.c_str());
}
void test_overlong_second_line_gets_ellipsis() {
    textfit::TwoLines t = textfit::wrapTwo(
        "one two three four five six seven eight nine ten eleven", 60, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("one two", t.a.c_str());
    TEST_ASSERT_TRUE(t.b.size() * 6 <= 60);
    TEST_ASSERT_EQUAL_STRING("...", t.b.substr(t.b.size() - 3).c_str());
}
void test_no_space_hard_splits() {
    textfit::TwoLines t = textfit::wrapTwo("aaaaaaaaaaaaaaaaaaaaaaaaa", 60, w6, nullptr);   // 25 chars, 10 fit
    TEST_ASSERT_EQUAL_INT(10, (int)t.a.size());
    TEST_ASSERT_TRUE(t.b.size() * 6 <= 60);
}
void test_empty_is_empty() {
    textfit::TwoLines t = textfit::wrapTwo("", 60, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("", t.a.c_str());
    TEST_ASSERT_EQUAL_STRING("", t.b.c_str());
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_short_line_stays_single);
    RUN_TEST(test_breaks_at_last_fitting_space);
    RUN_TEST(test_overlong_second_line_gets_ellipsis);
    RUN_TEST(test_no_space_hard_splits);
    RUN_TEST(test_empty_is_empty);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_textfit\test_textfit.cpp`
Expected: `COMPILE FAILED` (`textfit.h` missing).

- [ ] **Step 3: Create `src/util/textfit.h` / `textfit.cpp`**

```cpp
// textfit.h
#pragma once
#include <string>
// Two-line greedy wrap measured by a caller-supplied width function (so the same
// logic serves the TFT fonts on device and a fake font in tests). PURE.
namespace textfit {
struct TwoLines { std::string a, b; };
using WidthFn = int (*)(const std::string&, void* ctx);
TwoLines wrapTwo(const std::string& s, int maxW, WidthFn width, void* ctx);
}
```

```cpp
// textfit.cpp
#include "textfit.h"

namespace textfit {

static std::string ellipsize(std::string s, int maxW, WidthFn width, void* ctx) {
    if (width(s, ctx) <= maxW) return s;
    while (!s.empty() && width(s + "...", ctx) > maxW) s.pop_back();
    return s + "...";
}

TwoLines wrapTwo(const std::string& s, int maxW, WidthFn width, void* ctx) {
    if (s.empty() || width(s, ctx) <= maxW) return {s, ""};
    // longest prefix that fits
    size_t fit = 0;
    while (fit < s.size() && width(s.substr(0, fit + 1), ctx) <= maxW) ++fit;
    size_t cut = s.rfind(' ', fit);
    size_t next;
    if (cut == std::string::npos || cut == 0) { cut = fit; next = fit; }   // hard split
    else next = cut + 1;
    return {s.substr(0, cut), ellipsize(s.substr(next), maxW, width, ctx)};
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_textfit\test_textfit.cpp src\util\textfit.cpp`
Expected: `5 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Create `src/ui/battle.h` / `battle.cpp`**

```cpp
// battle.h
#pragma once
#include <TFT_eSPI.h>
// Gen-3 battle-screen drawing primitives.
namespace ui {
enum class Tab { None, Left, Right };
constexpr int HP_TAG_W = 22;
void background(TFT_eSPI& t);                                    // sky / horizon / grass (y >= 20)
void topStrip(TFT_eSPI& t);                                      // dark strip y 0..19
void battleBox(TFT_eSPI& t, int x, int y, int w, int h, Tab tab);  // cream box, border, shadow, slanted tab
void shadowText(TFT_eSPI& t, const char* s, int x, int y, uint8_t font,
                uint16_t fg, uint16_t shadow, uint8_t datum);   // transparent text + 1 px shadow
void hpBarBattle(TFT_eSPI& t, int x, int y, int w, int h, float frac);  // x = start of "HP" tag
void expBar(TFT_eSPI& t, int x, int y, int w, float frac);      // 3 px blue bar
void dialogueBox(TFT_eSPI& t, int x, int y, int w, int h);      // teal frame, white interior
}
```

```cpp
// battle.cpp
#include "battle.h"
#include "theme.h"

namespace ui {

void background(TFT_eSPI& t) {
    t.fillRect(0, 20, 320, 98, theme::SKY);
    t.fillRect(0, 118, 320, 4, theme::HORIZON);
    t.fillRect(0, 122, 320, 118, theme::GRASS);
}

void topStrip(TFT_eSPI& t) { t.fillRect(0, 0, 320, 20, theme::TOP_DARK); }

// Box silhouette: rectangle with one side slanted by c px over its height.
static void shape(TFT_eSPI& t, int x, int y, int w, int h, Tab tab, int c, uint16_t col) {
    if (tab == Tab::Left) {
        t.fillRect(x + c, y, w - c, h, col);
        t.fillTriangle(x, y, x + c, y, x + c, y + h - 1, col);
    } else if (tab == Tab::Right) {
        t.fillRect(x, y, w - c, h, col);
        t.fillTriangle(x + w - c, y, x + w - 1, y, x + w - c, y + h - 1, col);
    } else {
        t.fillRect(x, y, w, h, col);
    }
}

void battleBox(TFT_eSPI& t, int x, int y, int w, int h, Tab tab) {
    const int c = (tab == Tab::None) ? 0 : 10;
    shape(t, x + 3, y + 3, w, h, tab, c, theme::BOX_SHADOW);
    shape(t, x, y, w, h, tab, c, theme::BOX_BORDER);
    shape(t, x + 2, y + 2, w - 4, h - 4, tab, c > 2 ? c - 2 : 0, theme::BOX_FILL);
}

void shadowText(TFT_eSPI& t, const char* s, int x, int y, uint8_t font,
                uint16_t fg, uint16_t shadow, uint8_t datum) {
    t.setTextDatum(datum);
    t.setTextColor(shadow);          // single-arg colour = transparent background
    t.drawString(s, x + 1, y + 1, font);
    t.setTextColor(fg);
    t.drawString(s, x, y, font);
    t.setTextDatum(TL_DATUM);
}

void hpBarBattle(TFT_eSPI& t, int x, int y, int w, int h, float frac) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    t.fillRect(x, y, HP_TAG_W, h, theme::HP_TAG);
    t.setTextColor(theme::HP_TAG_TEXT);
    t.setTextDatum(ML_DATUM);
    t.drawString("HP", x + 4, y + h / 2 + 1, 1);
    t.setTextDatum(TL_DATUM);
    int fx = x + HP_TAG_W, fw = w - HP_TAG_W;
    t.fillRect(fx, y, fw, h, theme::HP_EMPTY);
    int inner = fw - 2;
    int filled = (int)(inner * frac);
    int x0 = fx + 1 + (inner - filled);               // drains from the left
    t.fillRect(x0, y + 1, filled, h - 2, theme::hpColor(frac));
    t.fillRect(x0, y + 1, filled, 2, theme::hpShine(frac));
}

void expBar(TFT_eSPI& t, int x, int y, int w, float frac) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    t.fillRect(x, y, w, 3, theme::HP_EMPTY);
    t.fillRect(x, y, (int)(w * frac), 3, theme::EXP_BLUE);
}

void dialogueBox(TFT_eSPI& t, int x, int y, int w, int h) {
    t.fillRoundRect(x, y, w, h, 4, theme::DLG_FRAME);
    t.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 3, theme::DLG_LINE);
    t.fillRoundRect(x + 4, y + 4, w - 8, h - 8, 3, theme::DLG_FILL);
}

}
```

- [ ] **Step 6: Create `src/ui/icons.h` / `icons.cpp`**

```cpp
// icons.h
#pragma once
#include <TFT_eSPI.h>
#include "icon_map.h"
namespace ui {
// 12x12 pixel icon at (x,y), composed in RAM and pushed in one go.
void drawIcon(TFT_eSPI& t, icons::Icon i, int x, int y, uint16_t fg, uint16_t bg);
// 13x13 spinning CD centred at (cx,cy); frame 0..3 rotates the sheen 45 deg each.
void drawCd(TFT_eSPI& t, int cx, int cy, int frame, uint16_t bg);
}
```

```cpp
// icons.cpp
#include "icons.h"
#include "theme.h"
#include <math.h>

namespace ui {

static inline uint16_t be(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

void drawIcon(TFT_eSPI& t, icons::Icon i, int x, int y, uint16_t fg, uint16_t bg) {
    uint16_t buf[icons::SIZE * icons::SIZE];
    for (int py = 0; py < icons::SIZE; ++py)
        for (int px = 0; px < icons::SIZE; ++px)
            buf[py * icons::SIZE + px] = be(icons::pixel(i, px, py) ? fg : bg);
    t.pushImage(x, y, icons::SIZE, icons::SIZE, buf);
}

void drawCd(TFT_eSPI& t, int cx, int cy, int frame, uint16_t bg) {
    t.fillRect(cx - 7, cy - 7, 15, 15, bg);
    t.fillCircle(cx, cy, 6, theme::CD_SILVER);
    t.drawCircle(cx, cy, 6, theme::ICON_OFF);
    float a = (frame & 3) * (float)M_PI / 4.0f;
    int dx = (int)lroundf(5 * cosf(a)), dy = (int)lroundf(5 * sinf(a));
    t.drawLine(cx - dx, cy - dy, cx + dx, cy + dy, TFT_WHITE);   // sheen
    t.fillCircle(cx, cy, 2, theme::TOP_DARK);                    // hub
}

}
```

- [ ] **Step 7: Device build compiles (primitives unused yet)**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`.

- [ ] **Step 8: Commit**

```bash
git add src/util/textfit.h src/util/textfit.cpp test/test_textfit/test_textfit.cpp src/ui/battle.h src/ui/battle.cpp src/ui/icons.h src/ui/icons.cpp
git commit -m "Add Gen-3 battle draw primitives, pixel icons, tested two-line wrap"
```

---

### Task 7: Battle layout + top strip + dialogue lyric + walker timing

**Files:**
- Rewrite: `src/ui/screen_now.h`, `src/ui/screen_now.cpp`
- Modify: `src/ui/widgets.h`, `src/ui/widgets.cpp` (remove `panel`/`hpBar`)
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: Tasks 1, 2, 5, 6; `interp::walkX`, `walkrect::plan`, `txt::asciiFold`, `img::drawAlbumArt`.
- Produces (namespace `ui`): `NowButtons nowButtons();` (unchanged, kept for future touch), `void drawNow(TFT_eSPI&, const AppState&, uint16_t accent);`, `void drawTopStrip(TFT_eSPI&, const AppState&);`, `void drawCdFrame(TFT_eSPI&, int frame);`, `void drawProgressRegion(TFT_eSPI&, const AppState&);`, `void drawWalker(TFT_eSPI&, const AppState&, uint32_t animMs, int step);`, `void drawLyricArea(TFT_eSPI&, const char* line);`, `void resetLyricArea();`, `void drawOffline(TFT_eSPI&, const char* msg);`, constants `ART_X=11, ART_Y=27, ART_W=92, ART_H=92`.

- [ ] **Step 1: Replace `src/ui/screen_now.h`**

```cpp
#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
#include "widgets.h"
namespace ui {
// Control button layout, kept for when touch controls return (Task 13).
struct NowButtons { Button prev, play, next, vol, lyrics; };
NowButtons nowButtons();

// Album art target rect inside the art battle box (main pushes the cover here).
constexpr int ART_X = 11, ART_Y = 27, ART_W = 92, ART_H = 92;

// Full Gen-3 battle deck: background, top strip, art box, info box, status box,
// dialogue box. Called on track change / return from a status screen.
void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent);
// Top strip only (title, CD, shuffle/repeat, device icon + name).
void drawTopStrip(TFT_eSPI& t, const AppState& st);
// Spinning CD after "NOW PLAYING" (frame 0..3).
void drawCdFrame(TFT_eSPI& t, int frame);
// HP time text + HP bar + EXP (volume) bar, repainted in place.
void drawProgressRegion(TFT_eSPI& t, const AppState& st);
// Walker on the HP bar. animMs = play-time animation clock (PMD frame timing);
// step = frame counter (fallback bob/mirror).
void drawWalker(TFT_eSPI& t, const AppState& st, uint32_t animMs, int step);
// Current lyric line in the dialogue box (redraws only when the text changes).
void drawLyricArea(TFT_eSPI& t, const char* currentLine);
// Forget the last drawn lyric so the next drawLyricArea() repaints (after drawNow).
void resetLyricArea();
// Full-screen status message in a dialogue box ("No signal...", "Nothing playing").
void drawOffline(TFT_eSPI& t, const char* msg);
}
```

- [ ] **Step 2: Replace `src/ui/screen_now.cpp`**

```cpp
#include "screen_now.h"
#include <string>
#include "battle.h"
#include "icons.h"
#include "theme.h"
#include "../util/text.h"
#include "../util/textfit.h"
#include "../util/interp.h"
#include "../util/walkrect.h"
#include "../util/walkanim.h"
#include "../images/walksprite.h"

namespace ui {

// ---- layout (see spec §2) ----
static const int INFO_X = 114, INFO_Y = 30, INFO_W = 198, INFO_H = 86;
static const int STAT_X = 8, STAT_Y = 126, STAT_W = 304, STAT_H = 68;
static const int BAR_X = 24, BAR_Y = 176, BAR_W = 280, BAR_H = 10;   // incl. "HP" tag
static const int FILL_X = BAR_X + HP_TAG_W + 1, FILL_W = BAR_W - HP_TAG_W - 2;
static const int EXP_Y = 188;
static const int DLG_X = 4, DLG_Y = 198, DLG_W = 312, DLG_H = 41;
static const int CD_CY = 10;

NowButtons nowButtons() {
    NowButtons b;
    int y = 210, h = 26;
    b.prev   = {8,   y, 46, h, "<<"};
    b.play   = {58,  y, 56, h, ">II"};
    b.next   = {118, y, 46, h, ">>"};
    b.vol    = {168, y, 44, h, "VOL"};
    b.lyrics = {216, y, 96, h, "LYRICS"};
    return b;
}

static int g_cdX = 100;   // set by drawTopStrip from the title width

// Fold UTF-8 accents to ASCII (fonts are ASCII-only), then truncate with an
// ellipsis so the text fits within maxW pixels.
static String fitText(TFT_eSPI& t, const char* s, int maxW, uint8_t font, bool upper = false) {
    char folded[128];
    txt::asciiFold(s, folded, sizeof(folded));
    if (upper) for (char* p = folded; *p; ++p) *p = (char)toupper((unsigned char)*p);
    String str = folded;
    if (t.textWidth(str, font) <= maxW) return str;
    while (str.length() > 1) {
        str.remove(str.length() - 1);
        if (t.textWidth(str + "...", font) <= maxW) break;
    }
    return str + "...";
}

void drawCdFrame(TFT_eSPI& t, int frame) { drawCd(t, g_cdX, CD_CY, frame, theme::TOP_DARK); }

void drawTopStrip(TFT_eSPI& t, const AppState& st) {
    topStrip(t);
    shadowText(t, "NOW PLAYING", 6, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TL_DATUM);
    g_cdX = 6 + t.textWidth("NOW PLAYING", 2) + 10;
    drawCdFrame(t, 0);

    String name = fitText(t, st.deviceName[0] ? st.deviceName : "device", 110, 2);
    int nameW = t.textWidth(name, 2);
    shadowText(t, name.c_str(), 314, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TR_DATUM);
    int devX = 314 - nameW - 4 - icons::SIZE;
    drawIcon(t, icons::forDevice(st.deviceType), devX, 4, theme::BOX_FILL, theme::TOP_DARK);
    int repX = devX - 18, shufX = repX - 16;
    drawIcon(t, icons::Icon::Repeat, repX, 4,
             st.repeat ? theme::BOX_FILL : theme::ICON_OFF, theme::TOP_DARK);
    if (st.repeat == 2) {   // repeat-one: tiny "1"
        t.setTextColor(theme::HP_TAG_TEXT);
        t.drawString("1", repX + icons::SIZE, 9, 1);
    }
    drawIcon(t, icons::Icon::Shuffle, shufX, 4,
             st.shuffle ? theme::BOX_FILL : theme::ICON_OFF, theme::TOP_DARK);
}

void drawProgressRegion(TFT_eSPI& t, const AppState& st) {
    uint32_t rem = (st.durationMs > st.progressMs) ? (st.durationMs - st.progressMs) : 0;
    uint32_t tot = st.durationMs;
    float hpFrac = st.durationMs ? (float)rem / (float)st.durationMs : 1.0f;
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "HP %u:%02u/%u:%02u",
             rem / 60000, (rem / 1000) % 60, tot / 60000, (tot / 1000) % 60);
    t.fillRect(206, 129, 100, 16, theme::BOX_FILL);   // clear the old time
    shadowText(t, tbuf, 304, 129, 2, theme::TEXT, theme::TEXT_SHADOW, TR_DATUM);
    hpBarBattle(t, BAR_X, BAR_Y, BAR_W, BAR_H, hpFrac);
    expBar(t, FILL_X, EXP_Y, FILL_W, st.volume / 100.0f);
}

void drawWalker(TFT_eSPI& t, const AppState& st, uint32_t animMs, int step) {
    static const int SLACK = 8;
    static walkrect::Rect prev{0, 0, 0, 0};
    static uint16_t buf[(walk::MAX_W + 2 * SLACK) * (walk::BAND_H + 1 + 2 * SLACK)];
    const walk::Info& wi = walk::info();
    if (!wi.ready) return;

    int fr = 0;
    if (wi.pmd) {
        uint16_t d[walk::MAX_FRAMES];
        for (int i = 0; i < wi.frames; ++i) d[i] = walk::durationMs(i);
        fr = walkanim::frameAt(animMs, d, wi.frames);
    }
    const uint16_t* spr = walk::pixels(fr);
    const uint8_t* msk = walk::mask(fr);
    int w = wi.w, h = wi.h;

    float frac = st.durationMs ? (float)st.progressMs / (float)st.durationMs : 0.0f;
    int cx = interp::walkX(frac, FILL_X, FILL_W, w);   // drained/remaining boundary
    int bob = (!wi.pmd && (step % 2)) ? 1 : 0;           // fallback fake-walk only
    bool mirror = !wi.pmd && ((step / 4) % 2);
    int x0 = cx - w / 2, y0 = BAR_Y - h - bob;          // feet rest on the bar top

    walkrect::Plan p = walkrect::plan(prev, {x0, BAR_Y - h - 1, w, h + 1}, SLACK);
    if (p.clearPrev) t.fillRect(prev.x, prev.y, prev.w, prev.h, theme::BOX_FILL);
    const walkrect::Rect& r = p.push;
    const uint16_t fillBE = (uint16_t)((theme::BOX_FILL >> 8) | (theme::BOX_FILL << 8));
    for (int i = 0; i < r.w * r.h; ++i) buf[i] = fillBE;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sx = mirror ? (w - 1 - x) : x;
            if (!msk[y * w + sx]) continue;
            int bx = x0 + x - r.x, by = y0 + y - r.y;
            if (bx >= 0 && bx < r.w && by >= 0 && by < r.h) buf[by * r.w + bx] = spr[y * w + sx];
        }
    t.pushImage(r.x, r.y, r.w, r.h, buf);
    prev = p.next;
}

static int tftWidth2(const std::string& s, void* ctx) {
    return ((TFT_eSPI*)ctx)->textWidth(s.c_str(), 2);
}

static std::string g_lastLyric = "\x01";   // never a real line -> forces a draw
void resetLyricArea() { g_lastLyric = "\x01"; }

void drawLyricArea(TFT_eSPI& t, const char* currentLine) {
    char folded[160];
    txt::asciiFold(currentLine ? currentLine : "", folded, sizeof(folded));
    if (g_lastLyric == folded) return;
    g_lastLyric = folded;

    const int ix = DLG_X + 6, iy = DLG_Y + 5, iw = DLG_W - 12, ih = DLG_H - 10;
    t.fillRect(ix, iy, iw, ih, theme::DLG_FILL);
    if (!folded[0]) return;
    drawIcon(t, icons::Icon::Note, ix + 2, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    drawIcon(t, icons::Icon::Note, ix + iw - 14, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    const int textW = iw - 2 * 18;
    textfit::TwoLines l = textfit::wrapTwo(folded, textW, tftWidth2, &t);
    int cx = DLG_X + DLG_W / 2;
    if (l.b.empty()) {
        shadowText(t, l.a.c_str(), cx, DLG_Y + 20, 2, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
    } else {
        shadowText(t, l.a.c_str(), cx, DLG_Y + 12, 2, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
        shadowText(t, l.b.c_str(), cx, DLG_Y + 29, 2, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
    }
}

void drawOffline(TFT_eSPI& t, const char* msg) {
    background(t);
    topStrip(t);
    dialogueBox(t, 20, 96, 280, 48);
    shadowText(t, msg, 160, 120, 4, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
}

void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent) {
    background(t);
    drawTopStrip(t, st);

    // album art battle box (main pushes the cover into ART_X/Y/W/H)
    battleBox(t, 8, 24, 98, 98, Tab::None);
    t.fillRect(ART_X, ART_Y, ART_W, ART_H, theme::SKY);

    // info box (opponent-style, slanted right end)
    battleBox(t, INFO_X, INFO_Y, INFO_W, INFO_H, Tab::Right);
    const int TW = INFO_W - 26;
    shadowText(t, fitText(t, st.trackName[0] ? st.trackName : "Track title", TW, 2, true).c_str(),
               INFO_X + 8, INFO_Y + 8, 2, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM);
    shadowText(t, fitText(t, st.artist[0] ? st.artist : "Artist", TW, 2).c_str(),
               INFO_X + 8, INFO_Y + 32, 2, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM);
    char from[80];
    snprintf(from, sizeof(from), "From: %s", st.context[0] ? st.context : "Playlist");
    shadowText(t, fitText(t, from, TW, 2).c_str(),
               INFO_X + 8, INFO_Y + 58, 2, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM);

    // status box (player-style, slanted left end): name + No., HP time, walker, bars
    battleBox(t, STAT_X, STAT_Y, STAT_W, STAT_H, Tab::Left);
    String nm = fitText(t, st.pokeName[0] ? st.pokeName : "Pokemon", 130, 2, true);
    shadowText(t, nm.c_str(), 24, 129, 2, accent, theme::TEXT_SHADOW, TL_DATUM);
    if (st.pokedexNum > 0) {
        char no[12];
        snprintf(no, sizeof(no), "No.%04d", st.pokedexNum);
        shadowText(t, no, 24 + t.textWidth(nm, 2) + 6, 134, 1, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM);
    }
    drawProgressRegion(t, st);

    dialogueBox(t, DLG_X, DLG_Y, DLG_W, DLG_H);
    resetLyricArea();   // box just repainted; main draws the current line next
}

}
```

- [ ] **Step 3: Remove superseded widgets**

In `src/ui/widgets.h` delete the `panel` and `hpBar` declarations; in `src/ui/widgets.cpp` delete their definitions. Keep `Button`, `drawButton`, `hit`.

- [ ] **Step 4: Rewire `src/main.cpp`**

Make these edits:

1. Includes: add `#include "images/walksprite.h"` (if not already from Task 5); remove `#include "images/png.h"`.
2. In the full-redraw branch, after `ui::drawNow(tft, view, accent);` replace `img::drawAlbumArt(tft, 11, 29, 98, 98);` with `img::drawAlbumArt(tft, ui::ART_X, ui::ART_Y, ui::ART_W, ui::ART_H);`. Keep the walk load + heap log from Task 5 **before** `drawNow`, and keep `ui::drawLyricArea(tft, currentLyric(view.progressMs));` after it.
3. Replace the `else { … }` steady-state branch (progress + walker timers) with:

```cpp
    } else {
        static uint32_t lastDraw = 0, lastWalk = 0, lastCd = 0, lastTick = 0, animMs = 0;
        static int walkStep = 0, cdFrame = 0;
        uint32_t now = millis();
        uint32_t dt = now - lastTick;
        lastTick = now;
        if (g_state.isPlaying) animMs += dt;   // walk cycle runs only while playing

        // top strip: redraw only when device / shuffle / repeat change
        char sig[96];
        snprintf(sig, sizeof(sig), "%s|%s|%d|%d", g_state.deviceName, g_state.deviceType,
                 (int)g_state.shuffle, g_state.repeat);
        if (strcmp(sig, g_topSig) != 0) {
            strcpy(g_topSig, sig);
            ui::drawTopStrip(tft, g_state);
        }
        if (g_state.isPlaying && now - lastCd >= 160) {   // spinning CD ~6 fps
            lastCd = now;
            ui::drawCdFrame(tft, cdFrame = (cdFrame + 1) & 3);
        }
        if (now - lastDraw >= 250) {
            lastDraw = now;
            ui::drawProgressRegion(tft, view);
            ui::drawLyricArea(tft, currentLyric(view.progressMs));
        }
        if (now - lastWalk >= 120) {                       // ~8 fps walker
            lastWalk = now;
            if (g_state.isPlaying) walkStep++;
            ui::drawWalker(tft, view, animMs, walkStep);
        }
    }
```

4. Add at file scope, next to `g_lrcLines`: `static char g_topSig[96] = "";  // last drawn top-strip state`. In the full-redraw branch, right after `ui::drawNow(tft, view, accent);`, add `g_topSig[0] = '\0';` so the steady-state loop re-checks the top strip on its first pass.

- [ ] **Step 5: All host suites pass**

Run each (all must print `OK`):
```
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_theme\test_theme.cpp src\ui\theme.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_icon_map\test_icon_map.cpp src\ui\icon_map.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_animdata\test_animdata.cpp src\util\animdata.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walkanim\test_walkanim.cpp src\util\walkanim.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_textfit\test_textfit.cpp src\util\textfit.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walkrect\test_walkrect.cpp src\util\walkrect.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walk\test_walk.cpp src\util\interp.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_interp\test_interp.cpp src\util\interp.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lrc\test_lrc.cpp src\util\lrc.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_text\test_text.cpp src\util\text.cpp
```

- [ ] **Step 6: Build, flash, observe**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`
Expected on screen (compare side by side with a FireRed/Emerald battle screenshot):
- sky/grass background; dark top strip with "NOW PLAYING", a CD that spins while playing and freezes on pause, shuffle/repeat icons (lit when on; "1" for repeat-one), device icon + name;
- album art in a framed box; info box with slanted right end: TRACK TITLE (caps), artist, From: context;
- status box with slanted left end: POKEMON NAME in type colour + No.NNNN, `HP m:ss/m:ss` right, the Pokémon **walking with real frames** along the HP bar (dark "HP" tag, shine line, drains from the left), thin blue volume bar;
- dialogue box at the bottom with ♪ lyric ♪ (1–2 lines, `...` when too long; empty when no lyric).
Toggle shuffle/repeat and switch device on the phone → top strip updates within ~12 s. Pause → walker and CD stop. Serial shows `[walk] …` and `[heap] …` per track, no reboot. Note any colour that looks off on the panel and adjust its `theme::` constant (re-run Step 5 for `test_theme` only if `rgb` itself changes).

- [ ] **Step 7: Status screens**

Turn the phone hotspot off. Expected: "No signal..." in a dialogue box on the battle background; turn it back on → full deck redraws and resumes. Stop playback on the phone → "Nothing playing" in the same style.

- [ ] **Step 8: Commit**

```bash
git add src/ui/screen_now.h src/ui/screen_now.cpp src/ui/widgets.h src/ui/widgets.cpp src/main.cpp src/ui/theme.h
git commit -m "Gen-3 battle layout: info/status/dialogue boxes, icons, PMD walk timing"
```

---

### Task 8: Credits and documentation

**Files:**
- Create: `CREDITS.md`
- Modify: `docs/superpowers/plans/2026-10-04-spotify-pokemon-deck.md` (As-Built Status)

- [ ] **Step 1: Create `CREDITS.md`**

```markdown
# Credits

- **Walking sprites:** [PMDCollab SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
  (Pokémon Mystery Dungeon sprite archive), licensed
  [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/). Per-sprite artist
  credits are in each Pokémon's `credits.txt` in that repository; many sprites are
  original Spike Chunsoft assets. Downloaded at runtime, not redistributed here.
- **Pokémon data and fallback sprites:** [PokéAPI](https://pokeapi.co/) and
  [PokeAPI/sprites](https://github.com/PokeAPI/sprites).
- **Lyrics:** [LRCLIB](https://lrclib.net/).
- **Inspiration:** [intellij-pokemon-progress](https://github.com/kagof/intellij-pokemon-progress).
- Pokémon © Nintendo / Creatures Inc. / GAME FREAK inc. This is a personal,
  non-commercial project.
```

- [ ] **Step 2: Update the old plan's As-Built Status**

In `docs/superpowers/plans/2026-10-04-spotify-pokemon-deck.md`, under `## As-Built Status`, append:

```markdown
- **2026-10-06 redesign:** superseded by
  `docs/superpowers/plans/2026-10-06-gen3-battle-ui-pmd-walker.md` — Gen-3 battle UI
  (info/status/dialogue boxes, pixel icons) and a PMD SpriteCollab walk cycle with the
  PokeAPI walker as fallback. The left Pokémon box and static sprite are gone.
```

- [ ] **Step 3: Commit**

```bash
git add CREDITS.md docs/superpowers/plans/2026-10-04-spotify-pokemon-deck.md
git commit -m "docs: add CREDITS (PMDCollab CC BY-NC, PokeAPI, LRCLIB) and as-built note"
```

---

## Self-Review

**Spec coverage:** §1 goal → Tasks 5–7. §2 layout table → Task 7 constants/`drawNow`. §2.0 battle style (background, boxes with tab + shadow, shadowed text, HP tag + shine, EXP bar, dialogue box, top strip, status screens) → Tasks 1, 6, 7. Repeat convention / no `Lv` / no gender → Task 2, Task 7. §2.1 icons (CD, notes, device, shuffle/repeat) → Tasks 2, 6, 7. §3.1 PMD source + CopyOf + row 2 → Tasks 3, 5. §3.2 loading (cache, size guards, shared bbox, native/downscale, 16 KB cap with frame skip, logs incl. heap) → Tasks 4, 5. §3.3 fallback → Task 5. §3.4 animation (walkX, durations, paused frame 0, ~8 fps, walkrect) → Task 7. §4 components → all tasks (`images/cache` generalised in Task 5). §5 errors (degrade, delete corrupt cache, width guards) → Task 5 (`SHEET_MAX_W`, `FULL_MAX`). §6 credits → Task 8. §7 tests → test_animdata, test_walkanim, test_icon_map, test_theme (+ test_textfit). §8 out of scope respected.

**Placeholder scan:** none.

**Type consistency:** `walk::Info{ready,pmd,w,h,frames}` used identically in Tasks 5 and 7; `walkanim::frameAt(uint32_t,const uint16_t*,int)` matches its Task 7 call; `icons::forDevice/pixel/row/SIZE/Icon` match Tasks 2, 6, 7; `ui::Tab`, `HP_TAG_W`, `hpBarBattle(x,y,w,h,frac)` match Tasks 6–7; `fetch::httpsGet` and `cache::readAll` share the NUL-terminated contract used by `animdata::parseWalk`.

---

## Part 2 — Smooth deck (performance)

**Spec:** `docs/superpowers/specs/2026-10-06-perf-network-task-design.md` (measurements in its §1).
Execution order: 9 → 10 → 11 → 12 → 13, then Task 8 (credits) if still open.

Additional Global Constraints for Part 2:
- Only the UI loop (core 1) touches the TFT; only the network task (core 0) does HTTPS, SD
  and PNG decoding. Album-art JPEG decode stays on the UI side.
- Everything shared between the two crosses through `core/shared` under its mutex.
- Network task: `xTaskCreatePinnedToCore(..., "net", 16384, nullptr, 1, nullptr, 0)`.

Review Focus for Part 2 (one test or device step each):
1. **Rapid skipping** — art/lyrics/walker fetched for an older track must never appear on the
   newer one. Covered: generation check in `shared::post*` (Task 11) + device step skipping 3×.
2. **Same track name twice / empty name while stopped** — must not trigger a new Pokémon.
   Covered: `test_netplan` TrackGen cases.
3. **Walker read while the network task promotes a new one** — no torn frames or crash.
   Covered: `drawWalker` holds the shared lock; promote only under the lock (Task 11).
4. **Dex names with non-ASCII / punctuation** (Nidoran♀, Farfetch’d, Flabébé) — readable ASCII.
   Covered: `test_dex`.
5. **Heap after two 16 KB walk pools** — TLS still has room. Covered: `[heap]` log checked in
   Task 11/12 device steps (max alloc must stay > 40 KB).

---

### Task 9: Bundled Pokédex (replaces the 4.2 s PokéAPI call)

**Files:**
- Create: `tools/gen_dex.py`, `src/pokemon/dex_data.inc` (generated), `src/pokemon/dex.h`, `src/pokemon/dex.cpp`
- Create: `src/pokemon/pick.h`, `src/pokemon/pick.cpp`
- Create: `test/test_dex/test_dex.cpp`
- Delete: `src/pokemon/pokeapi.h`, `src/pokemon/pokeapi.cpp`
- Modify: `src/main.cpp` (pick from dex)

**Interfaces:**
- Produces: `dex::COUNT = 1025`, `const char* dex::name(int n)`, `const char* dex::type(int n)` (lowercase, `""` out of range), `int dex::fromRandom(uint32_t r)` (1..1025), `void dex::spriteUrl(int n, char* out, size_t len)`; `void pick::choose(AppState& st, int n)`.

- [ ] **Step 1: Create `tools/gen_dex.py`**

```python
"""Generate src/pokemon/dex_data.inc from PokeAPI CSV data (dev-time; output is committed).

Why: the deck used to call https://pokeapi.co/api/v2/pokemon/<n> on every track change
just to get a name and a type: ~4.2 s of TLS + a multi-hundred-KB JSON body on a board
without PSRAM. 1025 names + primary types fit in ~20 KB of flash instead.
Run:  python tools/gen_dex.py
"""
import csv
import io
import os
import unicodedata
import urllib.request

BASE = "https://raw.githubusercontent.com/PokeAPI/pokeapi/master/data/v2/csv/"
COUNT = 1025
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "pokemon", "dex_data.inc")


def rows(name):
    data = urllib.request.urlopen(BASE + name).read().decode("utf-8")
    return list(csv.DictReader(io.StringIO(data)))


def ascii_name(s):
    s = s.replace("♀", " F").replace("♂", " M").replace("’", "'")
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode()
    return s.replace('"', "'")


types = {int(r["id"]): r["identifier"] for r in rows("types.csv")}
names = {int(r["pokemon_species_id"]): r["name"]
         for r in rows("pokemon_species_names.csv") if r["local_language_id"] == "9"}
first_type = {int(r["pokemon_id"]): int(r["type_id"])
              for r in rows("pokemon_types.csv") if r["slot"] == "1"}

type_ids = list(range(1, 19))                      # normal .. fairy
assert all(first_type[n] in type_ids for n in range(1, COUNT + 1))

with open(OUT, "w", encoding="ascii", newline="\n") as f:
    f.write("// Generated by tools/gen_dex.py from PokeAPI CSV data. Do not edit.\n")
    f.write("static const char* const TYPE_NAMES[] = {\n")
    for t in type_ids:
        f.write('    "%s",\n' % types[t])
    f.write("};\n")
    f.write("static const DexEntry DEX[%d] = {\n" % COUNT)
    for n in range(1, COUNT + 1):
        f.write('    {"%s", %d},\n' % (ascii_name(names[n]), first_type[n] - 1))
    f.write("};\n")
print("wrote", OUT)
```

- [ ] **Step 2: Generate the table**

Run: `python tools/gen_dex.py`
Expected: `wrote ...dex_data.inc`; the file starts with the generated header, has 18 type names and 1025 `{"Name", idx},` lines (`{"Bulbasaur", 11},` first, `{"Pecharunt", 3},` last).

- [ ] **Step 3: Write the failing test `test/test_dex/test_dex.cpp`**

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/pokemon/dex.h"

void setUp() {}
void tearDown() {}

void test_known_names() {
    TEST_ASSERT_EQUAL_STRING("Bulbasaur", dex::name(1));
    TEST_ASSERT_EQUAL_STRING("Pikachu", dex::name(25));
    TEST_ASSERT_EQUAL_STRING("Pecharunt", dex::name(1025));
}
void test_names_are_folded_to_ascii() {
    TEST_ASSERT_EQUAL_STRING("Nidoran F", dex::name(29));
    TEST_ASSERT_EQUAL_STRING("Nidoran M", dex::name(32));
    TEST_ASSERT_EQUAL_STRING("Farfetch'd", dex::name(83));
    TEST_ASSERT_EQUAL_STRING("Mr. Mime", dex::name(122));
    TEST_ASSERT_EQUAL_STRING("Flabebe", dex::name(669));
}
void test_primary_types() {
    TEST_ASSERT_EQUAL_STRING("grass", dex::type(1));
    TEST_ASSERT_EQUAL_STRING("electric", dex::type(25));
    TEST_ASSERT_EQUAL_STRING("poison", dex::type(1025));
}
void test_out_of_range_is_empty() {
    TEST_ASSERT_EQUAL_STRING("", dex::name(0));
    TEST_ASSERT_EQUAL_STRING("", dex::name(1026));
    TEST_ASSERT_EQUAL_STRING("", dex::type(-5));
}
void test_every_entry_is_printable_ascii_and_fits_state() {
    for (int n = 1; n <= dex::COUNT; ++n) {
        const char* s = dex::name(n);
        TEST_ASSERT_TRUE_MESSAGE(s[0] != 0, "empty name");
        TEST_ASSERT_TRUE_MESSAGE(strlen(s) < 24, "longer than AppState::pokeName");
        for (const char* p = s; *p; ++p) TEST_ASSERT_TRUE(*p >= 0x20 && *p <= 0x7E);
        TEST_ASSERT_TRUE(dex::type(n)[0] != 0);
    }
}
void test_from_random_covers_range() {
    TEST_ASSERT_EQUAL_INT(1, dex::fromRandom(0));
    TEST_ASSERT_EQUAL_INT(1025, dex::fromRandom(1024));
    TEST_ASSERT_EQUAL_INT(1, dex::fromRandom(1025));
    int n = dex::fromRandom(0xFFFFFFFFu);
    TEST_ASSERT_TRUE(n >= 1 && n <= dex::COUNT);
}
void test_sprite_url() {
    char url[160];
    dex::spriteUrl(25, url, sizeof(url));
    TEST_ASSERT_EQUAL_STRING(
        "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/25.png", url);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_known_names);
    RUN_TEST(test_names_are_folded_to_ascii);
    RUN_TEST(test_primary_types);
    RUN_TEST(test_out_of_range_is_empty);
    RUN_TEST(test_every_entry_is_printable_ascii_and_fits_state);
    RUN_TEST(test_from_random_covers_range);
    RUN_TEST(test_sprite_url);
    return UNITY_END();
}
```

- [ ] **Step 4: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_dex\test_dex.cpp`
Expected: `COMPILE FAILED` (`dex.h` missing).

- [ ] **Step 5: Create `src/pokemon/dex.h` and `src/pokemon/dex.cpp`**

```cpp
// dex.h
#pragma once
#include <cstddef>
#include <cstdint>
// Bundled national Pokédex (#1-1025): ASCII name + primary type, generated by
// tools/gen_dex.py. PURE, host-tested. Replaced a ~4.2 s PokeAPI request per
// track (see README "Design & performance history").
namespace dex {
constexpr int COUNT = 1025;
const char* name(int n);    // "" if out of range
const char* type(int n);    // lowercase PokeAPI type name ("grass"), "" if out of range
int fromRandom(uint32_t r); // 1..COUNT
// Fallback sprite (PokeAPI sprites repo) for dex n.
void spriteUrl(int n, char* out, size_t len);
}
```

```cpp
// dex.cpp
#include "dex.h"
#include <cstdio>

namespace dex {

struct DexEntry { const char* name; uint8_t type; };
#include "dex_data.inc"

const char* name(int n) { return (n >= 1 && n <= COUNT) ? DEX[n - 1].name : ""; }
const char* type(int n) { return (n >= 1 && n <= COUNT) ? TYPE_NAMES[DEX[n - 1].type] : ""; }
int fromRandom(uint32_t r) { return (int)(r % COUNT) + 1; }
void spriteUrl(int n, char* out, size_t len) {
    snprintf(out, len,
             "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png", n);
}

}
```

- [ ] **Step 6: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_dex\test_dex.cpp src\pokemon\dex.cpp`
Expected: `7 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 7: Create `src/pokemon/pick.h` / `pick.cpp` and use them in `main.cpp`**

```cpp
// pick.h
#pragma once
#include "app_state.h"
namespace pick {
// Sets st's Pokémon fields (number, name, type, fallback sprite URL) from the bundled dex.
void choose(AppState& st, int n);
}
```

```cpp
// pick.cpp
#include "pick.h"
#include <string.h>
#include "dex.h"

namespace pick {

void choose(AppState& st, int n) {
    st.pokedexNum = n;
    strncpy(st.pokeName, dex::name(n), sizeof(st.pokeName) - 1);
    st.pokeName[sizeof(st.pokeName) - 1] = '\0';
    strncpy(st.pokeType, dex::type(n), sizeof(st.pokeType) - 1);
    st.pokeType[sizeof(st.pokeType) - 1] = '\0';
    dex::spriteUrl(n, st.pokeSpriteUrl, sizeof(st.pokeSpriteUrl));
    Serial.printf("[poke] #%d %s (%s)\n", n, st.pokeName, st.pokeType);
}

}
```

In `src/main.cpp`: replace `#include "pokemon/pokeapi.h"` with `#include "pokemon/pick.h"`, `#include "pokemon/dex.h"` and `#include <esp_system.h>`, and replace `pokeapi::pickRandom(g_state);    // fresh random Pokemon each play` with:

```cpp
        pick::choose(g_state, dex::fromRandom(esp_random()));   // bundled dex: no network
```

Delete `src/pokemon/pokeapi.h` and `src/pokemon/pokeapi.cpp` (`git rm`).

- [ ] **Step 8: Build, flash, observe**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`, then `python .devtools\serial_read.py COM11 60` and skip a track.
Expected: `[poke] #N Name (type)` printed immediately on the track change (no PokéAPI request); the status box shows the name; no reset. `grep -rn pokeapi src` prints nothing.

- [ ] **Step 9: Commit**

```bash
git add tools/gen_dex.py src/pokemon/dex_data.inc src/pokemon/dex.h src/pokemon/dex.cpp src/pokemon/pick.h src/pokemon/pick.cpp test/test_dex/test_dex.cpp src/main.cpp
git rm src/pokemon/pokeapi.h src/pokemon/pokeapi.cpp
git commit -m "perf(pokemon): bundle the Pokedex in flash, drop the 4.2 s PokeAPI call per track"
```

---

### Task 10: Network schedule logic (PURE)

**Files:**
- Create: `src/util/netplan.h`, `src/util/netplan.cpp`, `test/test_netplan/test_netplan.cpp`

**Interfaces:**
- Produces (namespace `netplan`): `class TrackGen { bool update(const char* track); uint32_t gen() const; }`; `enum class Step { None, Walk, Art, Lyrics, Prefetch };` `struct Work { bool walk, art, lyrics, prefetch; };` (true = still to do) `Work freshWork(bool walkerReady);` `Step next(const Work& w);` `void done(Work& w, Step s);`

- [ ] **Step 1: Write the failing test `test/test_netplan/test_netplan.cpp`**

```cpp
#include <unity.h>
#include "../../src/util/netplan.h"

using netplan::Step;
void setUp() {}
void tearDown() {}

void test_first_track_bumps_generation() {
    netplan::TrackGen g;
    TEST_ASSERT_EQUAL_UINT32(0, g.gen());
    TEST_ASSERT_TRUE(g.update("Song A"));
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_same_track_does_not_bump() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_FALSE(g.update("Song A"));
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_new_track_bumps() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_TRUE(g.update("Song B"));
    TEST_ASSERT_EQUAL_UINT32(2, g.gen());
}
void test_empty_name_is_ignored_and_keeps_last() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_FALSE(g.update(""));
    TEST_ASSERT_FALSE(g.update(nullptr));
    TEST_ASSERT_FALSE(g.update("Song A"));   // resumed same song: no new Pokemon
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_order_without_prefetched_walker() {
    netplan::Work w = netplan::freshWork(false);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w));
    netplan::done(w, Step::Walk);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
    netplan::done(w, Step::Art);
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w));
    netplan::done(w, Step::Lyrics);
    TEST_ASSERT_EQUAL_INT((int)Step::Prefetch, (int)netplan::next(w));
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w));
}
void test_prefetched_walker_skips_walk_step() {
    netplan::Work w = netplan::freshWork(true);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
}
void test_idle_work_is_none() {
    netplan::Work w{};
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_track_bumps_generation);
    RUN_TEST(test_same_track_does_not_bump);
    RUN_TEST(test_new_track_bumps);
    RUN_TEST(test_empty_name_is_ignored_and_keeps_last);
    RUN_TEST(test_order_without_prefetched_walker);
    RUN_TEST(test_prefetched_walker_skips_walk_step);
    RUN_TEST(test_idle_work_is_none);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp`
Expected: `COMPILE FAILED` (`netplan.h` missing).

- [ ] **Step 3: Create `src/util/netplan.h` / `netplan.cpp`**

```cpp
// netplan.h
#pragma once
#include <cstdint>
// What the network task does next. PURE, host-tested.
// The task polls Spotify on a fixed cadence and, between polls, does ONE per-track
// work step at a time so a slow download never delays the next poll by more than
// one step (see README "Design & performance history").
namespace netplan {

// Track-change detection from successive now-playing names.
class TrackGen {
public:
    // True (and the generation increments) when `track` is non-empty and differs
    // from the last non-empty name seen.
    bool update(const char* track);
    uint32_t gen() const { return gen_; }
private:
    char last_[96] = {0};
    uint32_t gen_ = 0;
};

enum class Step { None, Walk, Art, Lyrics, Prefetch };
struct Work { bool walk, art, lyrics, prefetch; };   // true = still to do

// Work list for a new track. walkerReady = a prefetched walker was promoted.
Work freshWork(bool walkerReady);
// Next step in priority order: Walk, Art, Lyrics, Prefetch; None when all done.
Step next(const Work& w);
void done(Work& w, Step s);
}
```

```cpp
// netplan.cpp
#include "netplan.h"
#include <cstring>

namespace netplan {

bool TrackGen::update(const char* track) {
    if (!track || !track[0]) return false;
    if (strncmp(track, last_, sizeof(last_) - 1) == 0) return false;
    strncpy(last_, track, sizeof(last_) - 1);
    last_[sizeof(last_) - 1] = '\0';
    ++gen_;
    return true;
}

Work freshWork(bool walkerReady) { return {!walkerReady, true, true, true}; }

Step next(const Work& w) {
    if (w.walk) return Step::Walk;
    if (w.art) return Step::Art;
    if (w.lyrics) return Step::Lyrics;
    if (w.prefetch) return Step::Prefetch;
    return Step::None;
}

void done(Work& w, Step s) {
    switch (s) {
        case Step::Walk:     w.walk = false; break;
        case Step::Art:      w.art = false; break;
        case Step::Lyrics:   w.lyrics = false; break;
        case Step::Prefetch: w.prefetch = false; break;
        case Step::None:     break;
    }
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `7 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Commit**

```bash
git add src/util/netplan.h src/util/netplan.cpp test/test_netplan/test_netplan.cpp
git commit -m "feat(net): add tested network schedule for track generations and work order"
```

---

### Task 11: Network task on core 0 + shared state + progressive track change

**Files:**
- Create: `src/core/shared.h`, `src/core/shared.cpp`, `src/core/nettask.h`, `src/core/nettask.cpp`
- Modify: `include/app_state.h` (`trackGen`)
- Modify: `src/images/jpeg.h`, `src/images/jpeg.cpp` (download vs. draw split)
- Rewrite: `src/images/walksprite.h`, `src/images/walksprite.cpp` (active + staged slots)
- Modify: `src/ui/screen_now.cpp` (`drawWalker` reads frames under the shared lock)
- Rewrite: `src/main.cpp` (UI loop only)

**Interfaces:**
- Consumes: `netplan::*` (Task 10), `pick::choose`, `dex::*` (Task 9), `walkanim`, `animdata`, `lyricsvc::fetch`, `lrc::parse`, `spclient::poll/pollPlayerDetails`, `net::loop`.
- Produces:
  - `AppState::trackGen` (uint32_t; 0 = no track yet).
  - namespace `shared`: `begin()`, `lock()`, `unlock()`, `struct Guard`, `publish(const AppState&)`, `snapshot(AppState&)`, `postArt(uint32_t gen, uint8_t* jpeg, int len)`, `postLyrics(uint32_t gen, std::vector<lrc::LrcLine>* lines)`, `postWalker(uint32_t gen)`, `takeArt(uint32_t gen, uint8_t** jpeg, int* len)`, `takeLyrics(uint32_t gen, std::vector<lrc::LrcLine>** lines)`, `takeWalker(uint32_t gen)`.
  - `nettask::start()`.
  - `img::downloadAlbumArt(const char* url, uint8_t** out, int* len)`, `img::setAlbumArt(uint8_t* jpeg, int len)` (replaces `cacheAlbumArt`).
  - namespace `walk`: adds `bool begin()`, `int stagedDex()`, `void promote()`; loaders write the **staged** slot, readers read the **active** slot.

- [ ] **Step 1: Add `trackGen` to `include/app_state.h`**

Add as the last member before `PlaybackStatus status;`:

```cpp
    uint32_t trackGen;     // increments on every track change (network task); 0 = none yet
```

- [ ] **Step 2: Create `src/core/shared.h` / `shared.cpp`**

```cpp
// shared.h
#pragma once
#include <stdint.h>
#include <vector>
#include "app_state.h"
#include "../util/lrc.h"

// State shared between the network task (core 0) and the UI loop (core 1), behind one
// FreeRTOS mutex. Why two tasks: with all HTTPS inline, every Spotify poll froze the
// display for ~1.5 s and a track change for ~19 s (README "Design & performance history").
namespace shared {
void begin();
void lock();
void unlock();
struct Guard { Guard() { lock(); } ~Guard() { unlock(); } };

// AppState: written by the network task after every poll, copied by the UI each frame.
void publish(const AppState& st);
void snapshot(AppState& out);

// Per-track media mailbox. Each result is tagged with the trackGen it was fetched for;
// post* frees it if the published track has already moved on (rapid skipping).
void postArt(uint32_t gen, uint8_t* jpeg, int len);                 // takes ownership
void postLyrics(uint32_t gen, std::vector<lrc::LrcLine>* lines);    // takes ownership
void postWalker(uint32_t gen);                                       // walker promoted
// UI side: take a result for `gen`; ownership moves to the caller. False if none.
bool takeArt(uint32_t gen, uint8_t** jpeg, int* len);
bool takeLyrics(uint32_t gen, std::vector<lrc::LrcLine>** lines);
bool takeWalker(uint32_t gen);
}
```

```cpp
// shared.cpp
#include "shared.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace shared {

static SemaphoreHandle_t g_mtx = nullptr;
static AppState g_state{};
static uint8_t* g_art = nullptr;
static int g_artLen = 0;
static uint32_t g_artGen = 0;
static std::vector<lrc::LrcLine>* g_lyrics = nullptr;
static uint32_t g_lyricsGen = 0;
static uint32_t g_walkerGen = 0;

void begin() { if (!g_mtx) g_mtx = xSemaphoreCreateMutex(); }
void lock() { xSemaphoreTake(g_mtx, portMAX_DELAY); }
void unlock() { xSemaphoreGive(g_mtx); }

void publish(const AppState& st) { Guard g; g_state = st; }
void snapshot(AppState& out) { Guard g; out = g_state; }

void postArt(uint32_t gen, uint8_t* jpeg, int len) {
    Guard g;
    if (gen != g_state.trackGen) { free(jpeg); return; }   // stale: track moved on
    free(g_art);
    g_art = jpeg; g_artLen = len; g_artGen = gen;
}

void postLyrics(uint32_t gen, std::vector<lrc::LrcLine>* lines) {
    Guard g;
    if (gen != g_state.trackGen) { delete lines; return; }
    delete g_lyrics;
    g_lyrics = lines; g_lyricsGen = gen;
}

void postWalker(uint32_t gen) { Guard g; if (gen == g_state.trackGen) g_walkerGen = gen; }

bool takeArt(uint32_t gen, uint8_t** jpeg, int* len) {
    Guard g;
    if (!g_art || g_artGen != gen) return false;
    *jpeg = g_art; *len = g_artLen;
    g_art = nullptr; g_artLen = 0;
    return true;
}

bool takeLyrics(uint32_t gen, std::vector<lrc::LrcLine>** lines) {
    Guard g;
    if (!g_lyrics || g_lyricsGen != gen) return false;
    *lines = g_lyrics;
    g_lyrics = nullptr;
    return true;
}

bool takeWalker(uint32_t gen) {
    Guard g;
    if (gen == 0 || g_walkerGen != gen) return false;
    g_walkerGen = 0;
    return true;
}

}
```

- [ ] **Step 3: Split album art into download (network) and draw (UI) in `src/images/jpeg.*`**

In `jpeg.h`, replace the `cacheAlbumArt` declaration and its comment with:

```cpp
// Network side: downloads the JPEG at `url` (<= 60000 bytes) into a malloc'ed buffer.
bool downloadAlbumArt(const char* url, uint8_t** out, int* len);

// UI side: takes ownership of JPEG bytes for drawAlbumArt() (frees the previous ones).
// Pass nullptr to clear (new track, nothing to draw yet).
void setAlbumArt(uint8_t* jpeg, int len);
```

In `jpeg.cpp`, add `#include "fetch.h"` and replace the whole `cacheAlbumArt` function with:

```cpp
bool downloadAlbumArt(const char* url, uint8_t** out, int* len) {
    uint8_t* data = nullptr;
    size_t n = 0;
    if (!fetch::httpsGet(url, 60000, &data, &n)) return false;   // guard RAM; no PSRAM
    *out = data;
    *len = (int)n;
    return true;
}

void setAlbumArt(uint8_t* jpeg, int len) {
    free(g_buf);
    g_buf = jpeg;
    g_len = jpeg ? len : 0;
}
```

Remove the now-unused `#include <WiFiClientSecure.h>` / `#include <HTTPClient.h>` from `jpeg.cpp` if nothing else uses them.

- [ ] **Step 4: Replace `src/images/walksprite.h`**

```cpp
#pragma once
#include <stdint.h>

// The walker's frames. Two slots so the network task can load the next walker while
// the UI keeps drawing the current one:
//  - loaders (loadPmd / loadFallback) always write the STAGED slot (network task only);
//  - info / pixels / mask / durationMs read the ACTIVE slot (UI, under shared::lock);
//  - promote() swaps them (network task, under shared::lock).
// Sources: PMD SpriteCollab walk cycle (Right row, several frames) or, as a fallback,
// the PokeAPI sprite cropped to one frame (the UI adds bob + mirror).
namespace walk {
constexpr int BAND_H     = 32;      // walk band height in the status box
constexpr int MAX_W      = 64;      // widest frame kept
constexpr int CAP_BYTES  = 16384;   // per slot: RGB565 + 1-byte mask per pixel, all frames
constexpr int MAX_FRAMES = 16;

struct Info { bool ready; bool pmd; int w, h, frames; };

// Allocates both slots once (call early in setup, before the heap fragments).
bool begin();

bool loadPmd(int dex);                            // -> staged
bool loadFallback(const char* spriteUrl, int dex); // -> staged
int stagedDex();                                  // dex in the staged slot, 0 if none
void promote();                                   // staged <-> active

const Info& info();                  // active slot
const uint16_t* pixels(int frame);   // big-endian RGB565, w*h
const uint8_t*  mask(int frame);     // 1 = opaque, w*h
uint16_t durationMs(int frame);      // 0 for the fallback frame
}
```

- [ ] **Step 5: Replace `src/images/walksprite.cpp`**

```cpp
#include "walksprite.h"
#include <Arduino.h>
#include "png.h"
#include "cache.h"
#include "fetch.h"
#include "../util/animdata.h"
#include "../util/walkanim.h"

namespace walk {

// Slot layout: all frames' pixels first (keeps uint16 access aligned), then masks.
// Pools are heap-allocated once in begin(): static DRAM had no room for them.
static uint8_t* g_pool[2] = {nullptr, nullptr};
static Info g_info[2] = {};
static uint16_t g_dur[2][MAX_FRAMES];
static int g_dex[2] = {0, 0};
static int g_active = 0;
static inline int staged() { return 1 - g_active; }

bool begin() {
    for (int i = 0; i < 2; ++i)
        if (!g_pool[i]) g_pool[i] = (uint8_t*)malloc(CAP_BYTES);   // malloc is 4-byte aligned
    bool ok = g_pool[0] && g_pool[1];
    if (!ok) Serial.println("[walk] no heap for frame pools");
    return ok;
}

static uint16_t* pixelsAt(int s, int f) {
    return (uint16_t*)g_pool[s] + f * g_info[s].w * g_info[s].h;
}
static uint8_t* maskAt(int s, int f) {
    return g_pool[s] + g_info[s].frames * g_info[s].w * g_info[s].h * 2 + f * g_info[s].w * g_info[s].h;
}

const Info& info() { return g_info[g_active]; }
const uint16_t* pixels(int f) { return pixelsAt(g_active, f); }
const uint8_t* mask(int f) { return maskAt(g_active, f); }
uint16_t durationMs(int f) {
    return (f >= 0 && f < g_info[g_active].frames) ? g_dur[g_active][f] : 0;
}
int stagedDex() { return g_info[staged()].ready ? g_dex[staged()] : 0; }
void promote() { g_active = staged(); }

static inline bool bitAt(const uint8_t* bits, int x) { return (bits[x >> 3] >> (7 - (x & 7))) & 1; }

// ---------------------------------------------------------------- PMD sheet
static const int DIR_RIGHT = 2;          // PMD row order: Down, DownRight, Right, ...
static const int SHEET_MAX_W = 512;
static int s_fw, s_fh, s_rowY0, s_frames, s_pass, s_k, s_kept, s_slot;
static walkanim::Box s_box;
static walkanim::Fit s_fit;
static uint16_t s_line[SHEET_MAX_W];     // static: keep the PNGdec callback stack small
static uint8_t s_bits[SHEET_MAX_W / 8];

static int pmdDraw(PNGDRAW* d) {
    int ry = d->y - s_rowY0;
    if (ry < 0 || ry >= s_fh) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    if (s_pass == 1) {   // measure the bounding box shared by every frame of the row
        for (int x = 0; x < s_frames * s_fw; ++x)
            if (bitAt(s_bits, x)) walkanim::include(s_box, x % s_fw, ry);
        return 1;
    }
    int bw = s_box.x1 - s_box.x0 + 1, bh = s_box.y1 - s_box.y0 + 1;
    for (int oy = 0; oy < s_fit.h; ++oy) {
        if (walkanim::srcIndex(oy, s_fit.h, s_box.y0, bh) != ry) continue;
        for (int fi = 0; fi < s_kept; ++fi) {
            int f = fi * s_k;
            uint16_t* px = pixelsAt(s_slot, fi);
            uint8_t* m = maskAt(s_slot, fi);
            for (int ox = 0; ox < s_fit.w; ++ox) {
                int sx = f * s_fw + walkanim::srcIndex(ox, s_fit.w, s_box.x0, bw);
                px[oy * s_fit.w + ox] = s_line[sx];
                m[oy * s_fit.w + ox] = bitAt(s_bits, sx);
            }
        }
    }
    return 1;
}

static bool decodeRow(uint8_t* data, size_t n, const animdata::WalkAnim& a) {
    PNG& png = img::decoder();
    s_fw = a.frameW; s_fh = a.frameH; s_rowY0 = DIR_RIGHT * a.frameH;
    for (s_pass = 1; s_pass <= 2; ++s_pass) {
        if (png.openRAM(data, (int)n, pmdDraw) != PNG_SUCCESS) return false;
        if (s_pass == 1) {
            int sw = png.getWidth();
            if (sw > SHEET_MAX_W || png.getHeight() < s_rowY0 + s_fh) { png.close(); return false; }
            s_frames = sw / s_fw;
            if (a.frames < s_frames) s_frames = a.frames;
            if (s_frames < 1) { png.close(); return false; }
            s_box = walkanim::emptyBox();
        }
        int rc = png.decode(nullptr, 0);
        png.close();
        if (rc != PNG_SUCCESS) return false;
        if (s_pass == 1) {
            if (walkanim::isEmpty(s_box)) return false;
            s_fit = walkanim::fitBand(s_box.x1 - s_box.x0 + 1, s_box.y1 - s_box.y0 + 1, BAND_H, MAX_W);
            s_k = walkanim::keepEvery(s_frames, s_fit.w, s_fit.h, CAP_BYTES);
            if (s_k == 0) return false;
            s_kept = walkanim::keptCount(s_frames, s_k);
            g_info[s_slot] = {false, true, s_fit.w, s_fit.h, s_kept};   // layout for pass 2
        }
    }
    walkanim::mergedDurationsMs(a.ticks, s_frames, s_k, g_dur[s_slot]);
    g_info[s_slot].ready = true;
    return true;
}

// Cache-or-download into a NUL-terminated heap buffer.
static bool getCached(const String& path, const String& url, size_t maxLen,
                      uint8_t** out, size_t* n, int* code) {
    *code = 0;
    if (cache::readAll(path, maxLen, out, n)) return true;
    if (!fetch::httpsGet(url.c_str(), maxLen, out, n, code)) return false;
    cache::savePath(path, *out, *n);
    return true;
}

bool loadPmd(int dex) {
    s_slot = staged();
    g_info[s_slot].ready = false;
    g_dex[s_slot] = 0;
    if (dex < 1 || !g_pool[s_slot]) return false;
    char base[96];
    snprintf(base, sizeof(base),
             "https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/", dex);
    String xmlPath = "/pmd/" + String(dex) + ".xml";
    String pngPath = "/pmd/" + String(dex) + ".png";

    uint8_t* xml = nullptr; size_t xn = 0; int code = 0;
    if (!getCached(xmlPath, String(base) + "AnimData.xml", 32768, &xml, &xn, &code)) {
        Serial.printf("[walk] fallback (xml http %d)\n", code);
        return false;
    }
    animdata::WalkAnim a = animdata::parseWalk((const char*)xml);
    free(xml);
    if (!a.ok) {
        cache::removePath(xmlPath);
        Serial.println("[walk] fallback (no Walk anim)");
        return false;
    }
    uint8_t* png = nullptr; size_t pn = 0;
    if (!getCached(pngPath, String(base) + "Walk-Anim.png", 40000, &png, &pn, &code)) {
        Serial.printf("[walk] fallback (png http %d)\n", code);
        return false;
    }
    bool ok = decodeRow(png, pn, a);
    free(png);
    if (!ok) {   // corrupt or unusable: drop the cache so the next play re-downloads
        cache::removePath(xmlPath);
        cache::removePath(pngPath);
        g_info[s_slot].ready = false;
        Serial.println("[walk] fallback (sheet decode)");
        return false;
    }
    g_dex[s_slot] = dex;
    Serial.printf("[walk] pmd #%d %d frames %dx%d (k=%d)\n", dex, g_info[s_slot].frames,
                  g_info[s_slot].w, g_info[s_slot].h, s_k);
    return true;
}

// ---------------------------------------------------------------- fallback
static const int FULL_MAX = 128;
static uint16_t* f_px = nullptr;
static uint8_t* f_mask = nullptr;
static int f_w = 0, f_h = 0;

static int fullDraw(PNGDRAW* d) {
    if (d->y >= f_h) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    for (int x = 0; x < f_w; ++x) {
        f_px[d->y * f_w + x] = s_line[x];
        f_mask[d->y * f_w + x] = bitAt(s_bits, x);
    }
    return 1;
}

bool loadFallback(const char* spriteUrl, int dex) {
    int s = staged();
    g_info[s].ready = false;
    g_dex[s] = 0;
    if (!g_pool[s]) return false;
    uint8_t* data = nullptr; size_t n = 0;
    if (!img::loadSpriteBytes(dex, spriteUrl, &data, &n)) {
        Serial.println("[walk] no sprite");
        return false;
    }
    PNG& png = img::decoder();
    bool ok = false;
    if (png.openRAM(data, (int)n, fullDraw) == PNG_SUCCESS) {
        f_w = png.getWidth(); f_h = png.getHeight();
        bool alpha = png.hasAlpha();
        if (f_w <= FULL_MAX && f_h <= FULL_MAX) {
            f_px = (uint16_t*)malloc(f_w * f_h * 2);
            f_mask = (uint8_t*)calloc(f_w * f_h, 1);       // undecoded rows = clear
            if (f_px && f_mask && png.decode(nullptr, 0) == PNG_SUCCESS) {
                if (!alpha) {   // no alpha channel: key out the corner colour
                    uint16_t key = f_px[0];
                    for (int i = 0; i < f_w * f_h; ++i) f_mask[i] = f_px[i] != key;
                }
                walkanim::Box b = walkanim::emptyBox();
                for (int y = 0; y < f_h; ++y)
                    for (int x = 0; x < f_w; ++x)
                        if (f_mask[y * f_w + x]) walkanim::include(b, x, y);
                if (!walkanim::isEmpty(b)) {
                    int bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
                    walkanim::Fit fit = walkanim::fitBand(bw, bh, BAND_H, MAX_W);
                    g_info[s] = {false, false, fit.w, fit.h, 1};
                    uint16_t* px = pixelsAt(s, 0);
                    uint8_t* m = maskAt(s, 0);
                    for (int y = 0; y < fit.h; ++y) {
                        int sy = walkanim::srcIndex(y, fit.h, b.y0, bh);
                        for (int x = 0; x < fit.w; ++x) {
                            int sx = walkanim::srcIndex(x, fit.w, b.x0, bw);
                            px[y * fit.w + x] = f_px[sy * f_w + sx];
                            m[y * fit.w + x] = f_mask[sy * f_w + sx];
                        }
                    }
                    g_dur[s][0] = 0;
                    g_dex[s] = dex;
                    g_info[s].ready = ok = true;
                }
            } else if (!f_px || !f_mask) {
                Serial.printf("[walk] no heap for %dx%d sprite\n", f_w, f_h);
            }
            free(f_px); f_px = nullptr;
            free(f_mask); f_mask = nullptr;
        }
        png.close();
    }
    free(data);
    if (ok) Serial.printf("[walk] fallback #%d sprite %dx%d\n", dex, g_info[s].w, g_info[s].h);
    return ok;
}

}
```

- [ ] **Step 6: Create `src/core/nettask.h` / `nettask.cpp`**

```cpp
// nettask.h
#pragma once
// The network task (core 0): Spotify polls, per-track downloads, walker loading.
// It never draws; results reach the UI through core/shared.
namespace nettask {
// Starts the task. Call once from setup(), after WiFi and Spotify auth.
void start();
}
```

```cpp
// nettask.cpp
#include "nettask.h"
#include <Arduino.h>
#include <esp_system.h>
#include <vector>
#include "shared.h"
#include "../net/wifi.h"
#include "../spotify/client.h"
#include "../images/jpeg.h"
#include "../images/walksprite.h"
#include "../lyrics/lrclib.h"
#include "../pokemon/pick.h"
#include "../pokemon/dex.h"
#include "../util/lrc.h"
#include "../util/netplan.h"

// History (README "Design & performance history"): these calls used to run inline in
// loop(). Each is a fresh TLS handshake (poll ~1.5 s, player ~1.4 s, PokeAPI 4.2 s, art
// 2.5 s, lyrics 3.8 s, walk sheet 3.5 s), so the display froze during every poll and a
// track change took ~19 s to appear. Here they block only this task.
namespace nettask {

static AppState s_st{};            // this task's private copy; published after changes
static netplan::TrackGen s_gen;
static netplan::Work s_work{};
static int s_prefetchDex = 0;      // dex whose walker sits in the staged slot (Task 12)
static uint32_t s_t0 = 0;

static void tick() { s_t0 = millis(); }
static void tock(const char* what) {
    Serial.printf("[net] %s %lums\n", what, (unsigned long)(millis() - s_t0));
}

static void onTrackChange() {
    s_st.trackGen = s_gen.gen();
    pick::choose(s_st, dex::fromRandom(esp_random()));
    s_prefetchDex = 0;
    shared::publish(s_st);                 // UI shows the new title + Pokemon right away
    s_work = netplan::freshWork(false);
    Serial.printf("[net] track gen=%u\n", (unsigned)s_st.trackGen);
}

static void doStep(netplan::Step step) {
    const uint32_t gen = s_st.trackGen;
    switch (step) {
        case netplan::Step::Walk: {
            tick();
            bool ok = walk::loadPmd(s_st.pokedexNum) ||
                      walk::loadFallback(s_st.pokeSpriteUrl, s_st.pokedexNum);
            tock("walk");
            if (ok) {
                { shared::Guard g; walk::promote(); }
                shared::postWalker(gen);
            }
            break;
        }
        case netplan::Step::Art: {
            uint8_t* jpeg = nullptr;
            int len = 0;
            tick();
            if (img::downloadAlbumArt(s_st.albumArtUrl, &jpeg, &len)) shared::postArt(gen, jpeg, len);
            tock("art");
            break;
        }
        case netplan::Step::Lyrics: {
            tick();
            lyricsvc::Result r = lyricsvc::fetch(s_st);
            auto* lines = new std::vector<lrc::LrcLine>();
            if (r.kind == lyricsvc::Kind::Synced) *lines = lrc::parse(r.text);
            Serial.printf("[lyrics] kind=%d lines=%u\n", (int)r.kind, (unsigned)lines->size());
            shared::postLyrics(gen, lines);
            tock("lyrics");
            break;
        }
        case netplan::Step::Prefetch:   // implemented in Task 12
        case netplan::Step::None:
            break;
    }
    netplan::done(s_work, step);
}

static void run(void*) {
    uint32_t lastPoll = 0, lastPlayer = 0;
    bool first = true;
    for (;;) {
        net::loop();
        uint32_t now = millis();
        if (first || now - lastPoll >= 4000) {
            first = false;
            lastPoll = now;
            tick();
            spclient::poll(s_st);
            tock("poll");
            if (s_gen.update(s_st.trackName)) onTrackChange();
            else shared::publish(s_st);
        } else if (now - lastPlayer >= 12000) {
            lastPlayer = now;
            tick();
            spclient::pollPlayerDetails(s_st);
            tock("player");
            shared::publish(s_st);
        } else {
            netplan::Step step = netplan::next(s_work);
            if (step != netplan::Step::None) doStep(step);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void start() { xTaskCreatePinnedToCore(run, "net", 16384, nullptr, 1, nullptr, 0); }

}
```

- [ ] **Step 7: Read the walker under the shared lock in `src/ui/screen_now.cpp`**

Add `#include "../core/shared.h"` to the includes. In `drawWalker`, insert as the **first** statement after the three `static` declarations:

```cpp
    // The network task may promote() a new walker at any time; read frames under the lock.
    shared::Guard lockWalker;
```

- [ ] **Step 8: Rewrite `src/main.cpp` as the UI loop**

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <vector>
#include "pins.h"
#include "net/wifi.h"
#include "spotify/auth.h"
#include "spotify/client.h"
#include "app_state.h"
#include "core/shared.h"
#include "core/nettask.h"
#include "ui/theme.h"
#include "ui/screen_now.h"
#include "util/interp.h"
#include "util/lrc.h"
#include "images/jpeg.h"
#include "images/walksprite.h"
#include "images/cache.h"

// UI loop (core 1): draws only. All network/SD work runs in core/nettask on core 0 and
// arrives through core/shared (README "Design & performance history").
TFT_eSPI tft = TFT_eSPI();

static std::vector<lrc::LrcLine> g_lrcLines;   // synced lyric lines for the shown track
static char g_topSig[96] = "";                 // last drawn top-strip state

// Current synced lyric line for a playback position ("" if none).
static const char* currentLyric(uint32_t posMs) {
    if (g_lrcLines.empty()) return "";
    int idx = lrc::currentIndex(g_lrcLines, posMs);
    return (idx >= 0) ? g_lrcLines[idx].text.c_str() : "";
}

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);
    shared::begin();
    walk::begin();            // allocate both walker slots before the heap fragments
    cache::begin();
    tft.init(); tft.invertDisplay(true); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting WiFi...", 10, 10, 2);

    if (net::connectAny()) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Spotify auth...", 10, 10, 2);
        if (spauth::loadRefreshToken().isEmpty()) {
            tft.drawString("Open http://" + net::deviceIp() + "/", 10, 40, 2);
        }
        spauth::runSetupPortalIfNeeded();
        spclient::begin();
        nettask::start();
    } else {
        tft.drawString("WiFi FAILED", 10, 40, 2);
    }
}

void loop() {
    AppState st;
    shared::snapshot(st);
    AppState view = st;   // interpolate progress for a smooth bar between polls
    view.progressMs = interp::currentProgressMs(st.progressMs, st.durationMs, st.isPlaying,
                                                millis() - st.lastPollMs);

    // Screen mode: deck when playing/paused; a status screen when offline or stopped.
    int mode = 0;   // 0 = deck, 1 = offline, 2 = nothing playing
    if (!net::isOnline() || st.status == PlaybackStatus::Offline) mode = 1;
    else if (st.status == PlaybackStatus::Stopped) mode = 2;

    static int lastMode = -1;
    static uint32_t shownGen = 0;
    static bool walkerOn = false;
    if (mode != 0) {
        if (lastMode != mode) {   // draw the status screen once (no flicker)
            ui::drawOffline(tft, mode == 1 ? "No signal..." : "Nothing playing");
            lastMode = mode;
        }
    } else if (lastMode != 0 || st.trackGen != shownGen) {
        // Full deck redraw: immediately on track change (text + Pokemon name; art, lyric
        // and walker fill in as the network task delivers them), or back from a status screen.
        bool newTrack = st.trackGen != shownGen;
        lastMode = 0;
        if (newTrack) {
            shownGen = st.trackGen;
            walkerOn = false;
            g_lrcLines.clear();
            img::setAlbumArt(nullptr, 0);
        }
        ui::drawNow(tft, view, theme::typeColor(st.pokeType));
        g_topSig[0] = '\0';
        img::drawAlbumArt(tft, ui::ART_X, ui::ART_Y, ui::ART_W, ui::ART_H);   // no-op if none yet
        ui::drawLyricArea(tft, currentLyric(view.progressMs));
        Serial.printf("[heap] free=%u max=%u\n", (unsigned)ESP.getFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap());
    } else {
        // Media arriving from the network task for the track on screen.
        uint8_t* jpeg = nullptr;
        int jlen = 0;
        if (shared::takeArt(shownGen, &jpeg, &jlen)) {
            img::setAlbumArt(jpeg, jlen);
            img::drawAlbumArt(tft, ui::ART_X, ui::ART_Y, ui::ART_W, ui::ART_H);
        }
        std::vector<lrc::LrcLine>* lines = nullptr;
        if (shared::takeLyrics(shownGen, &lines)) {
            g_lrcLines = std::move(*lines);
            delete lines;
        }
        if (shared::takeWalker(shownGen)) walkerOn = true;

        static uint32_t lastDraw = 0, lastWalk = 0, lastCd = 0, lastTick = 0, animMs = 0;
        static int walkStep = 0, cdFrame = 0;
        uint32_t now = millis();
        uint32_t dt = now - lastTick;
        lastTick = now;
        if (st.isPlaying) animMs += dt;   // walk cycle runs only while playing

        // top strip: redraw only when device / shuffle / repeat change
        char sig[96];
        snprintf(sig, sizeof(sig), "%s|%s|%d|%d", st.deviceName, st.deviceType,
                 (int)st.shuffle, st.repeat);
        if (strcmp(sig, g_topSig) != 0) {
            strcpy(g_topSig, sig);
            ui::drawTopStrip(tft, st);
        }
        if (st.isPlaying && now - lastCd >= 160) {   // spinning CD ~6 fps
            lastCd = now;
            ui::drawCdFrame(tft, cdFrame = (cdFrame + 1) & 3);
        }
        if (now - lastDraw >= 250) {
            lastDraw = now;
            ui::drawProgressRegion(tft, view);
            ui::drawLyricArea(tft, currentLyric(view.progressMs));
        }
        if (walkerOn && now - lastWalk >= 120) {     // ~8 fps walker
            lastWalk = now;
            if (st.isPlaying) walkStep++;
            ui::drawWalker(tft, view, animMs, walkStep);
        }
    }
    delay(10);
}
```

- [ ] **Step 9: All host suites pass; device build compiles**

Run the 10 suites from Task 7 Step 5 plus:
```
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_dex\test_dex.cpp src\pokemon\dex.cpp
powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp
```
Expected: all `OK`. Then `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev` → `[SUCCESS]`; `grep -rn "cacheAlbumArt\|spclient::poll" src/main.cpp` prints nothing.

- [ ] **Step 10: Flash and observe**

Flash, then `python .devtools\serial_read.py COM11 120` while watching the screen; skip tracks a few times, including 3 skips in quick succession.
Expected:
- `[net] poll ~1500ms` lines keep appearing, and **the walker, CD and HP bar keep moving smoothly while they print** (no freeze).
- On a skip: `[net] track gen=N` and the new title/artist/Pokémon name appear on screen within one poll; then `[net] walk …`, `[net] art …` (cover appears), `[net] lyrics …` (lyric appears), each filling in progressively.
- After 3 rapid skips, the deck settles on the last song with **its** cover and lyric (never a previous song's).
- `[heap] … max=` stays above 40000. No `Guru Meditation`/reset.
Record the observed per-step times in the ledger (used by Task 13's README table).

- [ ] **Step 11: Commit**

```bash
git add include/app_state.h src/core/shared.h src/core/shared.cpp src/core/nettask.h src/core/nettask.cpp src/images/jpeg.h src/images/jpeg.cpp src/images/walksprite.h src/images/walksprite.cpp src/ui/screen_now.cpp src/main.cpp
git commit -m "perf(net): move networking to a core-0 task with progressive track change"
```

---

### Task 12: Prefetch the next Pokémon's walker

**Files:**
- Modify: `src/core/nettask.cpp`

**Interfaces:**
- Consumes: `walk::stagedDex()`, `walk::promote()`, `netplan::Step::Prefetch` / `freshWork(bool)`, `dex::fromRandom`, `dex::spriteUrl` (Tasks 9–11).

- [ ] **Step 1: Implement the Prefetch step**

In `nettask.cpp` `doStep`, replace

```cpp
        case netplan::Step::Prefetch:   // implemented in Task 12
        case netplan::Step::None:
            break;
```

with

```cpp
        case netplan::Step::Prefetch: {
            // The Pokemon is random and independent of the song, so the next one can be
            // chosen and its walker loaded now, while this song plays.
            int n = dex::fromRandom(esp_random());
            char url[160];
            dex::spriteUrl(n, url, sizeof(url));
            tick();
            bool ok = walk::loadPmd(n) || walk::loadFallback(url, n);
            s_prefetchDex = ok ? n : 0;
            tock(ok ? "prefetch" : "prefetch failed");
            break;
        }
        case netplan::Step::None:
            break;
```

- [ ] **Step 2: Use the prefetched walker on track change**

Replace the body of `onTrackChange` with:

```cpp
static void onTrackChange() {
    s_st.trackGen = s_gen.gen();
    bool prefetched = s_prefetchDex > 0 && walk::stagedDex() == s_prefetchDex;
    pick::choose(s_st, prefetched ? s_prefetchDex : dex::fromRandom(esp_random()));
    s_prefetchDex = 0;
    shared::publish(s_st);                 // UI shows the new title + Pokemon right away
    if (prefetched) {
        { shared::Guard g; walk::promote(); }
        shared::postWalker(s_st.trackGen); // walker appears together with the title
    }
    s_work = netplan::freshWork(prefetched);
    Serial.printf("[net] track gen=%u%s\n", (unsigned)s_st.trackGen,
                  prefetched ? " (prefetched walker)" : "");
}
```

- [ ] **Step 3: Host suites pass; build; flash; observe**

Run all suites (Task 11 Step 9 list) → `OK`; build + flash.
Expected on serial after the first song's art and lyrics: `[walk] pmd #N …` then `[net] prefetch ~3500ms`. On the next track change: `[poke] #N …` with the **same N**, `[net] track gen=… (prefetched walker)`, and on screen the walker appears together with the new title (no `[net] walk` step for that track). A skip made before the prefetch finishes still works (no "(prefetched walker)", `[net] walk …` runs instead).

- [ ] **Step 4: Commit**

```bash
git add src/core/nettask.cpp
git commit -m "perf(walk): prefetch the next Pokemon's walker during the current song"
```

---

### Task 13: README and design/performance history

**Files:**
- Create: `README.md`
- Modify: `docs/superpowers/plans/2026-10-04-spotify-pokemon-deck.md` (As-Built pointer)

- [ ] **Step 1: Write `README.md`**

Write the README with these sections, filling the "After" column of the table with the times observed in Task 11 Step 10 / Task 12 Step 3 (from the ledger):

```markdown
# PokeDeck — Spotify now-playing deck in Gen-3 Pokémon battle style

An ESP32 "Cheap Yellow Display" (ESP32-2432S028R, 320×240) that shows what's playing on
Spotify as a FireRed/Emerald battle screen: the song's random Pokémon walks along the HP
bar (remaining time), with album art, synced lyrics in the battle dialogue box, and pixel
icons for the playing device, shuffle and repeat. Built for the car, running off a phone
hotspot.

## Hardware
- ESP32-2432S028R (ESP32-WROOM-32, no PSRAM), ILI9341 320×240, XPT2046 touch, microSD slot.
- Optional microSD card: caches walk sheets and sprites so repeats load instantly.

## Build & flash
- Copy `include/config.example.h` to `src/config.h`, fill WiFi networks and Spotify app
  credentials; get a refresh token with `python .devtools/spotify_auth.py`.
- Build/flash: `.devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`.
- Host tests: `.devtools\ntest.ps1 test\<suite>\<suite>.cpp <module.cpp>` (see the plan for the list).
- Regenerate the bundled Pokédex: `python tools/gen_dex.py`.

## Architecture
- **UI loop (core 1, `src/main.cpp`)** — draws only: battle layout (`ui/screen_now`,
  `ui/battle`, `ui/icons`), album art JPEG decode, walker animation, CD, lyric.
- **Network task (core 0, `src/core/nettask.cpp`)** — Spotify polls (4 s now-playing,
  12 s player), then one per-track step at a time: walker, album art, lyrics, prefetch of
  the next walker (`util/netplan`).
- **Shared state (`src/core/shared.cpp`)** — one mutex; `AppState` snapshots and a
  per-track mailbox tagged with a track generation so stale results are dropped.
- **Walker (`images/walksprite`)** — PMD SpriteCollab walk cycle (Right row) cropped and
  fitted to a 32 px band, two slots (active/staged) so the next one loads in the
  background; fallback = PokeAPI sprite with a bob/flip fake walk.
- **Pure, host-tested modules** — `ui/theme`, `ui/icon_map`, `pokemon/dex`,
  `util/{animdata,walkanim,walkrect,netplan,textfit,interp,lrc,text}`.

## Design & performance history
1. **v1 (single loop):** every HTTPS call ran inline in `loop()`. Album art, PokéAPI and
   lyrics were fetched on track change; the screen was redrawn between polls.
2. **Walker v1:** the PokeAPI sprite bobbed and flipped along the HP bar (fake walk).
3. **Gen-3 battle UI + PMD walk cycle:** real multi-frame walk from PMDCollab
   SpriteCollab, battle boxes, HP tag, dialogue box, pixel icons. The 16 KB frame pool had
   to move from static DRAM to the heap (static segment overflowed by 4 KB).
4. **Measured stutter (2026-10-06)** — instrumentation around each blocking call:

   | Step | Before (inline) | After |
   |---|---|---|
   | Spotify poll (every 4 s) | 1.5 s UI freeze | 0 s UI freeze (runs on core 0) |
   | Player poll (every 12 s) | 1.4 s UI freeze | 0 s UI freeze |
   | PokéAPI name/type | 4.2 s | 0 s (bundled dex) |
   | Album art | 2.5 s | <fill from ledger> (in background) |
   | Lyrics | 3.8 s | <fill from ledger> (in background) |
   | PMD walk sheet | 3.5 s | 0 s when prefetched |
   | New song visible after skip | ~19 s | <fill from ledger> |

   Root cause: one task did both drawing and synchronous TLS requests.
5. **Fixes:** network task on core 0 + shared mailbox (no UI freezes); progressive track
   change (title first, media as it arrives); bundled Pokédex (no PokéAPI per track);
   prefetch of the next walker; HTTP/1.0 streaming JSON parse (earlier heap fix).

## Credits
See `CREDITS.md`.
```

Replace each `<fill from ledger>` with the measured value before committing (the plan text
is a template; the committed README must contain real numbers).

- [ ] **Step 2: As-Built pointer**

Append to the `## As-Built Status` section of `docs/superpowers/plans/2026-10-04-spotify-pokemon-deck.md`:

```markdown
- **2026-10-06 performance:** networking moved to a core-0 task with progressive track
  change, bundled Pokédex and walker prefetch — see `README.md` "Design & performance
  history" and `docs/superpowers/specs/2026-10-06-perf-network-task-design.md`.
```

- [ ] **Step 3: Commit**

```bash
git add README.md docs/superpowers/plans/2026-10-04-spotify-pokemon-deck.md
git commit -m "docs: add README with architecture and design/performance history"
```

---

## Part 3 — Deterministic memory + resilience

**Spec:** `docs/superpowers/specs/2026-10-06-memory-resilience-design.md` (measurements in §1).
Execution order: 14 → 15 → 16 → 17 → 18 → 19 → 20. Commits: Conventional Commits.

Additional Global Constraints for Part 3:
- "Free memory" means **byte-addressable internal RAM**: `mem::byteFree()` / `mem::byteLargest()`
  (`MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL`). `ESP.getFreeHeap()`/`getMaxAllocHeap()` include IRAM
  and must not be used for decisions or logs.
- Large buffers are allocated once in `setup()` **before WiFi starts**: walker slots + scratch
  (`walk::begin`), album-art bitmap + tjpgd workspace (`art::begin`). The lyrics arena is static.
- No `malloc(len)` per download in steady state.

Review Focus for Part 3 (one test or device step each):
1. **Allocation failures during a long session** — none. Covered: Task 20 10-minute session,
   `[allocfail]` count must be 0.
2. **Art for a quickly skipped track drawn on the next one / torn while redrawing** — never.
   Covered: invalidate-before-write + push-under-lock (Task 15) + device skip step.
3. **Sheets/sprites PNGdec can't buffer** — skipped without deleting the cache entry. Covered:
   `test_walkanim` `pngFits` cases.
4. **Stuck with WiFi up and no good poll** — restarts after 180 s; WiFi down never restarts.
   Covered: `test_netplan` Health cases + Task 18 forced-failure device step.
5. **Two different songs with the same title** — new Pokémon/art/lyrics. Covered: Task 19 uses
   the track URI (device step with two same-title songs if available; otherwise code review).

---

### Task 14: Measure the right memory; gate steps on it

**Files:**
- Create: `src/core/mem.h`, `src/core/mem.cpp`
- Modify: `src/util/netplan.h`, `src/util/netplan.cpp`, `test/test_netplan/test_netplan.cpp`
- Modify: `src/core/nettask.cpp`, `src/main.cpp`

**Interfaces:**
- Produces: `size_t mem::byteFree()`, `size_t mem::byteLargest()`, `void mem::log(const char* where)`,
  `void mem::installFailHook()`; `constexpr unsigned netplan::TLS_NEED = 20000;`
  `bool netplan::canRun(Step s, unsigned largest)`.

- [ ] **Step 1: Add failing tests to `test/test_netplan/test_netplan.cpp`** (register with `RUN_TEST`)

```cpp
void test_can_run_gates_on_largest_block() {
    using netplan::canRun;
    TEST_ASSERT_TRUE(canRun(Step::Art, 6000));          // plain HTTP: small
    TEST_ASSERT_FALSE(canRun(Step::Art, 5999));
    TEST_ASSERT_TRUE(canRun(Step::Lyrics, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Lyrics, netplan::TLS_NEED - 1));
    TEST_ASSERT_TRUE(canRun(Step::Walk, netplan::TLS_NEED + 8000));
    TEST_ASSERT_FALSE(canRun(Step::Walk, netplan::TLS_NEED + 7999));
    TEST_ASSERT_FALSE(canRun(Step::Prefetch, netplan::TLS_NEED + 7999));
    TEST_ASSERT_TRUE(canRun(Step::None, 0));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `COMPILE FAILED` (`canRun` / `TLS_NEED` not declared).

- [ ] **Step 3: Implement in `netplan.h` / `netplan.cpp`**

In `netplan.h`, after `void done(Work& w, Step s);` add:

```cpp
// Memory gate (largest free byte-addressable block, mem::byteLargest()). mbedTLS needs a
// 16.7 KB input buffer plus ~3 KB per handshake, so TLS steps wait below TLS_NEED; optional
// TLS steps (walker, prefetch) also leave 8 KB for the WiFi driver. Art is plain HTTP.
constexpr unsigned TLS_NEED = 20000;
bool canRun(Step s, unsigned largest);
```

In `netplan.cpp`, add before the closing `}` of the namespace:

```cpp
bool canRun(Step s, unsigned largest) {
    switch (s) {
        case Step::Art:      return largest >= 6000;
        case Step::Lyrics:   return largest >= TLS_NEED;
        case Step::Walk:
        case Step::Prefetch: return largest >= TLS_NEED + 8000;
        case Step::None:     return true;
    }
    return true;
}
```

- [ ] **Step 4: Run to verify it passes**

Run the Step 2 command. Expected: `10 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Create `src/core/mem.h` / `mem.cpp`**

```cpp
// mem.h
#pragma once
#include <stddef.h>
// Byte-addressable internal RAM — what byte buffers, mbedTLS and the WiFi driver can use.
// ESP.getFreeHeap()/getMaxAllocHeap() also count 32-bit-only IRAM and read ~60 KB too high;
// that hid an exhausted heap (README "Design & performance history").
namespace mem {
size_t byteFree();
size_t byteLargest();
void log(const char* where);   // "[mem] <where> free=... largest=..."
void installFailHook();        // "[allocfail] <size> caps=... largest=... free=..." on every failed malloc
}
```

```cpp
// mem.cpp
#include "mem.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace mem {

static const uint32_t CAPS = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL;

size_t byteFree() { return heap_caps_get_free_size(CAPS); }
size_t byteLargest() { return heap_caps_get_largest_free_block(CAPS); }

void log(const char* where) {
    Serial.printf("[mem] %s free=%u largest=%u\n", where, (unsigned)byteFree(), (unsigned)byteLargest());
}

static void onAllocFail(size_t size, uint32_t caps, const char* fn) {
    ets_printf("[allocfail] %u bytes caps=0x%x in %s; largest=%u free=%u\n", (unsigned)size,
               (unsigned)caps, fn ? fn : "?", (unsigned)heap_caps_get_largest_free_block(caps),
               (unsigned)heap_caps_get_free_size(caps));
}

void installFailHook() { heap_caps_register_failed_alloc_callback(onAllocFail); }

}
```

- [ ] **Step 6: Use it**

`src/main.cpp`:
- remove the `// DIAG` `onAllocFail` function and the `heap_caps_register_failed_alloc_callback(onAllocFail);   // DIAG` line; add `#include "core/mem.h"` and, as the first line after `delay(200);` in `setup()`, `mem::installFailHook();`;
- right after `if (net::connectAny()) {` add `mem::log("boot+wifi");`;
- replace the `Serial.printf("[heap] free=%u max=%u\n", ...ESP.getFreeHeap()..., ...ESP.getMaxAllocHeap()...);` statement in the full-redraw branch with `mem::log("track");`.

`src/core/nettask.cpp`:
- add `#include "mem.h"`;
- replace the `[heap] poll failed` printf (3 lines) with `if (!ok) mem::log("poll failed");`;
- replace the `[heap] art len=...` printf (2 lines) with `mem::log("art");`;
- in `run()`, replace

```cpp
            netplan::Step step = netplan::next(s_work);
            if (step != netplan::Step::None) doStep(step);
```

with

```cpp
            netplan::Step step = netplan::next(s_work);
            static bool deferred = false;
            if (step != netplan::Step::None) {
                if (netplan::canRun(step, (unsigned)mem::byteLargest())) {
                    deferred = false;
                    doStep(step);
                } else if (!deferred) {            // log once per deferral, retry after polls
                    deferred = true;
                    mem::log("defer step");
                }
            }
```

- [ ] **Step 7: Build, flash, measure the budget**

All host suites (Task 11 Step 9 list) → `OK`; build + flash; `python .devtools\serial_read.py COM11 120` with one skip.
Expected: `[mem] boot+wifi free=… largest=…`, `[mem] track …`, `[mem] art …`. Record the numbers in the ledger — they are the Part 3 baseline (this firmware still holds the JPEG).

- [ ] **Step 8: Commit**

```bash
git add src/core/mem.h src/core/mem.cpp src/util/netplan.h src/util/netplan.cpp test/test_netplan/test_netplan.cpp src/core/nettask.cpp src/main.cpp
git commit -m "fix(mem): measure byte-addressable RAM and gate network steps on it"
```

---

### Task 15: Album art streamed into a fixed 92×92 bitmap (plain HTTP)

**Files:**
- Create: `src/util/artmap.h`, `src/util/artmap.cpp`, `test/test_artmap/test_artmap.cpp`
- Create: `src/images/art.h`, `src/images/art.cpp`
- Delete: `src/images/jpeg.h`, `src/images/jpeg.cpp`
- Modify: `src/core/shared.h`, `src/core/shared.cpp`, `src/core/nettask.cpp`, `src/main.cpp`

**Interfaces:**
- Produces: `artmap::Map{x0,y0,side,outW,outH}`, `artmap::cover(srcW,srcH,outW,outH)`, `artmap::srcX(m,ox)`, `artmap::srcY(m,oy)`, `artmap::pickScale(w,h,minSide)`, `artmap::plainHttpUrl(in,out,len)`; `art::W=92`, `art::H=92`, `bool art::begin()`, `bool art::fetch(const char* url)`, `const uint16_t* art::bitmap()`; `shared::artInvalidate()`, `shared::postArt(uint32_t gen)`, `shared::takeArt(uint32_t gen)`, `shared::artValidLocked(uint32_t gen)` (caller holds the lock).

- [ ] **Step 1: Write the failing test `test/test_artmap/test_artmap.cpp`**

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/artmap.h"

void setUp() {}
void tearDown() {}

void test_cover_square_maps_full_range() {
    artmap::Map m = artmap::cover(150, 150, 92, 92);
    TEST_ASSERT_EQUAL_INT(0, artmap::srcX(m, 0));
    TEST_ASSERT_EQUAL_INT(148, artmap::srcX(m, 91));
    TEST_ASSERT_EQUAL_INT(148, artmap::srcY(m, 91));
}
void test_cover_landscape_crops_center() {
    artmap::Map m = artmap::cover(200, 150, 92, 92);
    TEST_ASSERT_EQUAL_INT(25, artmap::srcX(m, 0));
    TEST_ASSERT_EQUAL_INT(0, artmap::srcY(m, 0));
    TEST_ASSERT_EQUAL_INT(25 + 91 * 150 / 92, artmap::srcX(m, 91));
}
void test_pick_scale_keeps_short_side_at_least_box() {
    TEST_ASSERT_EQUAL_INT(1, artmap::pickScale(300, 300, 92));   // 150 px
    TEST_ASSERT_EQUAL_INT(2, artmap::pickScale(640, 640, 92));   // 160 px
    TEST_ASSERT_EQUAL_INT(0, artmap::pickScale(64, 64, 92));     // smaller than the box
    TEST_ASSERT_EQUAL_INT(3, artmap::pickScale(2000, 1000, 92)); // 125 px short side
}
void test_plain_http_url() {
    char out[128];
    artmap::plainHttpUrl("https://i.scdn.co/image/abc", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("http://i.scdn.co/image/abc", out);
    artmap::plainHttpUrl("https://example.com/x.jpg", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://example.com/x.jpg", out);
    char tiny[8];
    artmap::plainHttpUrl("https://i.scdn.co/image/abc", tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_INT(7, (int)strlen(tiny));   // truncated, terminated
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cover_square_maps_full_range);
    RUN_TEST(test_cover_landscape_crops_center);
    RUN_TEST(test_pick_scale_keeps_short_side_at_least_box);
    RUN_TEST(test_plain_http_url);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_artmap\test_artmap.cpp`
Expected: `COMPILE FAILED` (`artmap.h` missing).

- [ ] **Step 3: Create `src/util/artmap.h` / `artmap.cpp`**

```cpp
// artmap.h
#pragma once
#include <cstddef>
// Album-art geometry and URL helpers. PURE, host-tested.
namespace artmap {
// Nearest-neighbour cover-fill: the centred square of side min(srcW,srcH) maps onto outW x outH.
struct Map { int x0, y0, side, outW, outH; };
Map cover(int srcW, int srcH, int outW, int outH);
inline int srcX(const Map& m, int ox) { return m.x0 + ox * m.side / m.outW; }
inline int srcY(const Map& m, int oy) { return m.y0 + oy * m.side / m.outH; }
// Largest tjpgd scale (0..3 = 1/1..1/8) whose shorter side is still >= minSide.
int pickScale(int w, int h, int minSide);
// "https://i.scdn.co/..." -> "http://i.scdn.co/..." (the CDN serves plain HTTP: no TLS needed);
// any other URL is copied unchanged. Always NUL-terminates.
void plainHttpUrl(const char* in, char* out, size_t len);
}
```

```cpp
// artmap.cpp
#include "artmap.h"
#include <cstring>

namespace artmap {

Map cover(int srcW, int srcH, int outW, int outH) {
    int side = srcW < srcH ? srcW : srcH;
    return {(srcW - side) / 2, (srcH - side) / 2, side, outW, outH};
}

int pickScale(int w, int h, int minSide) {
    int s = w < h ? w : h;
    for (int k = 3; k > 0; --k)
        if ((s >> k) >= minSide) return k;
    return 0;
}

void plainHttpUrl(const char* in, char* out, size_t len) {
    static const char* PFX = "https://i.scdn.co/";
    if (!len) return;
    const char* src = in ? in : "";
    if (strncmp(src, PFX, strlen(PFX)) == 0) {
        strncpy(out, "http://", len - 1);
        out[len - 1] = '\0';
        size_t used = strlen(out);
        if (used < len - 1) strncat(out, src + 8, len - 1 - used);   // skip "https://"
    } else {
        strncpy(out, src, len - 1);
        out[len - 1] = '\0';
    }
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_artmap\test_artmap.cpp src\util\artmap.cpp`
Expected: `4 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Create `src/images/art.h` / `art.cpp`**

```cpp
// art.h
#pragma once
#include <stdint.h>
// Album art as a fixed 92x92 RGB565 bitmap, allocated once. The network task streams the
// cover JPEG over plain HTTP straight into the decoder and writes the bitmap; the UI pushes it.
// Replaced holding the whole JPEG (20-60 KB) plus a 21.6 KB UI decode buffer, which exhausted
// byte-addressable RAM (README "Design & performance history").
namespace art {
constexpr int W = 92, H = 92;
bool begin();                 // bitmap + tjpgd workspace; call in setup() before WiFi
bool fetch(const char* url);  // network task only; caller invalidated the art first
const uint16_t* bitmap();     // big-endian RGB565, W*H (pushImage with swap off)
}
```

```cpp
// art.cpp
#include "art.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "tjpgd.h"
#include "../util/artmap.h"

namespace art {

static uint16_t* g_bmp = nullptr;
static uint8_t* g_work = nullptr;
static WiFiClient* s_in = nullptr;
static uint32_t s_deadline = 0;
static artmap::Map s_map;

bool begin() {
    if (!g_bmp) g_bmp = (uint16_t*)malloc(W * H * 2);
    if (!g_work) g_work = (uint8_t*)malloc(TJPGD_WORKSPACE_SIZE);
    if (g_bmp) memset(g_bmp, 0, W * H * 2);
    bool ok = g_bmp && g_work;
    if (!ok) Serial.println("[art] no heap for bitmap");
    return ok;
}

const uint16_t* bitmap() { return g_bmp; }

// tjpgd input: read (or skip, when buf == nullptr) up to len bytes from the HTTP stream.
static size_t jdIn(JDEC*, uint8_t* buf, size_t len) {
    size_t got = 0;
    uint8_t sink[64];
    while (got < len && (int32_t)(s_deadline - millis()) > 0) {
        int a = s_in->available();
        if (a <= 0) {
            if (!s_in->connected()) break;
            delay(1);
            continue;
        }
        size_t want = len - got;
        if ((size_t)a < want) want = (size_t)a;
        if (!buf && want > sizeof(sink)) want = sizeof(sink);
        int r = s_in->read(buf ? buf + got : sink, want);
        if (r <= 0) break;
        got += (size_t)r;
    }
    return got;
}

// tjpgd output: one decoded block (native RGB565); copy the pixels the cover map picks.
static int jdOut(JDEC*, void* block, JRECT* r) {
    const uint16_t* px = (const uint16_t*)block;
    int bw = r->right - r->left + 1;
    for (int oy = 0; oy < H; ++oy) {
        int sy = artmap::srcY(s_map, oy);
        if (sy < r->top || sy > r->bottom) continue;
        for (int ox = 0; ox < W; ++ox) {
            int sx = artmap::srcX(s_map, ox);
            if (sx < r->left || sx > r->right) continue;
            uint16_t c = px[(sy - r->top) * bw + (sx - r->left)];
            g_bmp[oy * W + ox] = (uint16_t)((c >> 8) | (c << 8));   // store big-endian
        }
    }
    return 1;
}

bool fetch(const char* url) {
    if (!g_bmp || !g_work || !url || !url[0]) return false;
    char plain[200];
    artmap::plainHttpUrl(url, plain, sizeof(plain));
    WiFiClient client;
    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(8000);
    if (!http.begin(client, plain)) return false;
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[art] http %d\n", code);
        http.end();
        return false;
    }
    s_in = http.getStreamPtr();
    s_deadline = millis() + 10000;
    JDEC jd;
    JRESULT rc = jd_prepare(&jd, jdIn, g_work, TJPGD_WORKSPACE_SIZE, nullptr);
    if (rc == JDR_OK) {
        int s = artmap::pickScale(jd.width, jd.height, W);
        s_map = artmap::cover(jd.width >> s, jd.height >> s, W, H);
        rc = jd_decomp(&jd, jdOut, (uint8_t)s);
    }
    http.end();
    if (rc != JDR_OK) Serial.printf("[art] decode error %d\n", (int)rc);
    return rc == JDR_OK;
}

}
```

- [ ] **Step 6: Replace the art mailbox in `src/core/shared.*`**

In `shared.h`, replace the `postArt(uint32_t gen, uint8_t* jpeg, int len)` and `takeArt(uint32_t gen, uint8_t** jpeg, int* len)` declarations with:

```cpp
// Album art lives in art::bitmap(). The network task calls artInvalidate() before writing it
// and postArt(gen) after; the UI pushes it only while it is valid for the track on screen.
void artInvalidate();
void postArt(uint32_t gen);
bool takeArt(uint32_t gen);            // true once per arrival (UI)
bool artValidLocked(uint32_t gen);     // caller holds the lock (UI redraw + push)
```

In `shared.cpp`, replace the `g_art`, `g_artLen`, `g_artGen` statics with
`static uint32_t g_artGen = 0;  // gen the bitmap is valid for (0 = invalid)` and
`static bool g_artNew = false;`, and replace the `postArt` and `takeArt` definitions with:

```cpp
void artInvalidate() { Guard g; g_artGen = 0; g_artNew = false; }

void postArt(uint32_t gen) {
    Guard g;
    if (gen != g_state.trackGen) return;   // stale: track moved on (bitmap stays invalid)
    g_artGen = gen;
    g_artNew = true;
}

bool takeArt(uint32_t gen) {
    Guard g;
    if (!g_artNew || g_artGen != gen) return false;
    g_artNew = false;
    return true;
}

bool artValidLocked(uint32_t gen) { return gen != 0 && g_artGen == gen; }
```

- [ ] **Step 7: Network side (`src/core/nettask.cpp`)**

Replace `#include "../images/jpeg.h"` with `#include "../images/art.h"`, and the whole `case netplan::Step::Art: { ... }` block with:

```cpp
        case netplan::Step::Art: {
            tick();
            shared::artInvalidate();          // UI stops pushing the bitmap before we overwrite it
            bool ok = art::fetch(s_st.albumArtUrl);
            if (ok) shared::postArt(gen);
            tock(ok ? "art" : "art failed");
            mem::log("art");
            break;
        }
```

- [ ] **Step 8: UI side (`src/main.cpp`)**

- Replace `#include "images/jpeg.h"` with `#include "images/art.h"`.
- In `setup()`, right after `walk::begin();` add `art::begin();            // fixed album-art bitmap (before WiFi)`.
- Add above `setup()`:

```cpp
// Pushes the album-art bitmap if it is valid for the track on screen. Check and push happen
// under one lock so the network task cannot start overwriting the bitmap in between.
static void pushArtIfValid(uint32_t gen) {
    shared::Guard g;
    if (shared::artValidLocked(gen))
        tft.pushImage(ui::ART_X, ui::ART_Y, art::W, art::H, art::bitmap());
}
```

- In the full-redraw branch, delete `img::setAlbumArt(nullptr, 0);` and replace
  `img::drawAlbumArt(tft, ui::ART_X, ui::ART_Y, ui::ART_W, ui::ART_H);   // no-op if none yet` with
  `pushArtIfValid(shownGen);   // back from a status screen: same track's art is still valid`.
- In the steady-state branch, replace the whole `uint8_t* jpeg ... if (shared::takeArt(...)) { ... }` block with:

```cpp
        if (shared::takeArt(shownGen)) pushArtIfValid(shownGen);
```

- [ ] **Step 9: Delete the old JPEG path**

`git rm src/images/jpeg.h src/images/jpeg.cpp`. `grep -rn "drawAlbumArt\|setAlbumArt\|downloadAlbumArt\|TJpgDec" src` prints nothing.

- [ ] **Step 10: Host suites, build, flash, observe**

All suites + `test_artmap` → `OK`; build + flash; serial 120 s with 3 skips (one burst of 3 quick skips).
Expected: `[net] art ~0.5–1.5 s` (no TLS), cover appears on screen for each track and never shows the previous track's cover after the quick burst; `[mem] art` largest stays ≥ 25 000; no `[allocfail]`.

- [ ] **Step 11: Commit**

```bash
git add src/util/artmap.h src/util/artmap.cpp test/test_artmap/test_artmap.cpp src/images/art.h src/images/art.cpp src/core/shared.h src/core/shared.cpp src/core/nettask.cpp src/main.cpp
git rm src/images/jpeg.h src/images/jpeg.cpp
git commit -m "perf(art): stream album art over plain HTTP into a fixed 92x92 bitmap"
```

---

### Task 16: Walker loaders without large transients; PNG pitch guard; cache rules

**Files:**
- Modify: `src/util/walkanim.h`, `src/util/walkanim.cpp`, `test/test_walkanim/test_walkanim.cpp`
- Modify: `src/images/fetch.h`, `src/images/fetch.cpp`, `src/images/cache.h`, `src/images/cache.cpp`, `src/images/png.h`, `src/images/png.cpp`
- Rewrite: `src/images/walksprite.cpp`; modify `src/images/walksprite.h`

**Interfaces:**
- Produces: `bool walkanim::pngFits(int width, int pixelType, int bpp, int maxBuffered = 2562)`;
  `bool fetch::httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen, int* httpCode = nullptr)`
  (needs `len + 1 <= cap`; NUL-terminates); `bool cache::readInto(const String& path, uint8_t* buf, size_t cap, size_t* outLen)`;
  `bool img::loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen, bool* fromCache)`;
  `walk::SCRATCH = 16384`.

- [ ] **Step 1: Add failing tests to `test/test_walkanim/test_walkanim.cpp`** (register with `RUN_TEST`)

```cpp
// PNGdec 1.1.6 keeps the current and previous line (+16 B alignment each) in a 2562-byte buffer.
void test_png_fits_rgba_up_to_316px() {
    TEST_ASSERT_TRUE(pngFits(128, 6, 8));    // pitch 512
    TEST_ASSERT_TRUE(pngFits(316, 6, 8));    // pitch 1264 -> 2560
    TEST_ASSERT_FALSE(pngFits(317, 6, 8));   // pitch 1268 -> 2568
    TEST_ASSERT_FALSE(pngFits(320, 6, 8));
}
void test_png_fits_indexed_wide() {
    TEST_ASSERT_TRUE(pngFits(512, 3, 8));    // pitch 512
    TEST_ASSERT_TRUE(pngFits(512, 3, 4));    // pitch 256
}
void test_png_fits_truecolor_and_gray_alpha() {
    TEST_ASSERT_TRUE(pngFits(400, 2, 8));    // RGB pitch 1200 -> 2432
    TEST_ASSERT_FALSE(pngFits(500, 2, 8));
    TEST_ASSERT_TRUE(pngFits(600, 4, 8));    // gray+alpha pitch 1200
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_walkanim\test_walkanim.cpp src\util\walkanim.cpp`
Expected: `COMPILE FAILED` (`pngFits` not declared).

- [ ] **Step 3: Implement `pngFits`**

`walkanim.h`, before the closing `}`:

```cpp
// True if PNGdec 1.1.6 can decode a `width`-pixel-wide PNG of this colour type (0 gray, 2 RGB,
// 3 indexed, 4 gray+alpha, 6 RGBA) and bit depth: it keeps two lines (+16 B each) in a
// maxBuffered-byte buffer and does not check RGBA widths above ~316 px itself.
bool pngFits(int width, int pixelType, int bpp, int maxBuffered = 2562);
```

`walkanim.cpp`, before the closing `}`:

```cpp
bool pngFits(int width, int pixelType, int bpp, int maxBuffered) {
    int ch = 4;
    switch (pixelType) {
        case 0: ch = 1; break;
        case 2: ch = 3; break;
        case 3: ch = 1; break;
        case 4: ch = 2; break;
        case 6: ch = 4; break;
    }
    int pitch = (width * ch * bpp + 7) / 8;
    return 2 * (pitch + 16) <= maxBuffered;
}
```

- [ ] **Step 4: Run to verify it passes**

Run the Step 2 command. Expected: `14 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Caller-buffer downloads and cache reads**

Replace `src/images/fetch.h` with:

```cpp
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace fetch {
// HTTPS GET of `url` into the caller's buffer (no per-download malloc while TLS is open).
// Fails on non-200, unknown length, len + 1 > cap, or a stalled/short read (8 s handshake
// and read timeouts). On success buf holds *outLen bytes plus a trailing NUL.
bool httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                  int* httpCode = nullptr);
}
```

Replace `src/images/fetch.cpp` with:

```cpp
#include "fetch.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace fetch {

bool httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen, int* httpCode) {
    *outLen = 0;
    if (httpCode) *httpCode = 0;
    if (!url || !url[0] || !buf || cap < 2) return false;
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);   // seconds; default 120 s blocks a half-dead link
    HTTPClient https;
    https.useHTTP10(true);           // plain (non-chunked) body so getSize() is the length
    https.setTimeout(8000);
    if (!https.begin(client, url)) return false;
    int code = https.GET();
    if (httpCode) *httpCode = code;
    if (code != 200) {
        Serial.printf("[fetch] http %d\n", code);
        https.end();
        return false;
    }
    int len = https.getSize();
    if (len <= 0 || (size_t)len + 1 > cap) {
        Serial.printf("[fetch] bad length %d (cap %u)\n", len, (unsigned)cap);
        https.end();
        return false;
    }
    WiFiClient* s = https.getStreamPtr();
    int got = 0;
    uint32_t last = millis();
    while (got < len && (https.connected() || s->available())) {
        size_t a = s->available();
        if (a) {
            size_t want = (size_t)(len - got);
            if (a < want) want = a;
            got += s->readBytes(buf + got, want);
            last = millis();
        } else {
            if (millis() - last > 8000) break;   // stalled
            delay(1);
        }
    }
    https.end();
    if (got != len) return false;
    buf[len] = 0;
    *outLen = (size_t)len;
    return true;
}

}
```

In `src/images/cache.h`, replace the `readAll` declaration and its comment with:

```cpp
// Reads the whole file into the caller's buffer if it fits (n + 1 <= cap); NUL-terminates.
bool readInto(const String& path, uint8_t* buf, size_t cap, size_t* outLen);
```

In `src/images/cache.cpp`, replace the `readAll` function with:

```cpp
bool readInto(const String& path, uint8_t* buf, size_t cap, size_t* outLen) {
    *outLen = 0;
    if (!ready) return false;
    fs::File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t n = f.size();
    if (n == 0 || n + 1 > cap) { f.close(); return false; }
    size_t got = f.read(buf, n);
    f.close();
    if (got != n) return false;
    buf[n] = 0;
    *outLen = n;
    return true;
}
```

Replace `src/images/png.h`'s `loadSpriteBytes` declaration and comment with:

```cpp
// PokeAPI sprite bytes for dex `dex` into the caller's buffer: SD cache if present, else HTTPS
// download of `url` (saved to the cache). *fromCache tells the caller where they came from.
bool loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                    bool* fromCache);
```

and `src/images/png.cpp`'s `loadSpriteBytes` definition with:

```cpp
bool loadSpriteInto(int dex, const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                    bool* fromCache) {
    *fromCache = cache::readInto(cache::spritePath(dex), buf, cap, outLen);
    if (*fromCache) return true;
    if (!fetch::httpsGetInto(url, buf, cap, outLen)) return false;
    cache::save(dex, buf, *outLen);
    return true;
}
```

- [ ] **Step 6: Add the scratch constant to `src/images/walksprite.h`**

After `constexpr int MAX_FRAMES = 16;` add:

```cpp
constexpr int SCRATCH    = 16384;   // one download buffer for XML / sheet / sprite (begin())
```

and change the `begin()` comment to `// Allocates both slots and the download scratch once (setup(), before WiFi).`

- [ ] **Step 7: Rewrite `src/images/walksprite.cpp`**

```cpp
#include "walksprite.h"
#include <Arduino.h>
#include "png.h"
#include "cache.h"
#include "fetch.h"
#include "../util/animdata.h"
#include "../util/walkanim.h"

namespace walk {

// Slot layout: all frames' pixels first (keeps uint16 access aligned), then masks.
// Pools and the download scratch are allocated once in begin(), before WiFi; loaders never
// malloc (README "Design & performance history": per-download mallocs with TLS open
// exhausted byte-addressable RAM).
static uint8_t* g_pool[2] = {nullptr, nullptr};
static uint8_t* g_scratch = nullptr;
static Info g_info[2] = {};
static uint16_t g_dur[2][MAX_FRAMES];
static int g_dex[2] = {0, 0};
static int g_active = 0;
static inline int staged() { return 1 - g_active; }

bool begin() {
    for (int i = 0; i < 2; ++i)
        if (!g_pool[i]) g_pool[i] = (uint8_t*)malloc(CAP_BYTES);   // malloc is 4-byte aligned
    if (!g_scratch) g_scratch = (uint8_t*)malloc(SCRATCH);
    bool ok = g_pool[0] && g_pool[1] && g_scratch;
    if (!ok) Serial.println("[walk] no heap for frame pools");
    return ok;
}

static uint16_t* pixelsAt(int s, int f) {
    return (uint16_t*)g_pool[s] + f * g_info[s].w * g_info[s].h;
}
static uint8_t* maskAt(int s, int f) {
    return g_pool[s] + g_info[s].frames * g_info[s].w * g_info[s].h * 2 + f * g_info[s].w * g_info[s].h;
}

const Info& info() { return g_info[g_active]; }
const uint16_t* pixels(int f) { return pixelsAt(g_active, f); }
const uint8_t* mask(int f) { return maskAt(g_active, f); }
uint16_t durationMs(int f) {
    return (f >= 0 && f < g_info[g_active].frames) ? g_dur[g_active][f] : 0;
}
int stagedDex() { return g_info[staged()].ready ? g_dex[staged()] : 0; }
void promote() { g_active = staged(); }

static inline bool bitAt(const uint8_t* bits, int x) { return (bits[x >> 3] >> (7 - (x & 7))) & 1; }

// ------------------------------------------------- two-pass region decode (PMD + fallback)
// Decodes rows [rowY0, rowY0+rowH) of a sheet holding `frames` frames of frameW pixels side
// by side. Pass 1 finds the opaque bounding box shared by all frames; pass 2 writes the
// cropped, band-fitted frames straight into the staged slot. No full-size copy.
enum class Res { Ok, Unsupported, Corrupt };
static const int SHEET_MAX_W = 512;
static int s_frameW, s_rowH, s_rowY0, s_frames, s_pass, s_k, s_kept, s_slot;
static bool s_keyMode;                   // no alpha channel: top-left colour is transparent
static uint16_t s_key;
static walkanim::Box s_box;
static walkanim::Fit s_fit;
static uint16_t s_line[SHEET_MAX_W];     // static: keep the PNGdec callback stack small
static uint8_t s_bits[SHEET_MAX_W / 8];

static inline bool opaqueAt(int x) {
    return s_keyMode ? s_line[x] != s_key : bitAt(s_bits, x);
}

static int regionDraw(PNGDRAW* d) {
    int ry = d->y - s_rowY0;
    if (ry < 0 || ry >= s_rowH) return 1;
    PNG& png = img::decoder();
    memset(s_bits, 0xff, sizeof(s_bits));
    png.getLineAsRGB565(d, s_line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    png.getAlphaMask(d, s_bits, 128);
    if (s_pass == 1) {
        if (s_keyMode && ry == 0) s_key = s_line[0];
        for (int x = 0; x < s_frames * s_frameW; ++x)
            if (opaqueAt(x)) walkanim::include(s_box, x % s_frameW, ry);
        return 1;
    }
    int bw = s_box.x1 - s_box.x0 + 1, bh = s_box.y1 - s_box.y0 + 1;
    for (int oy = 0; oy < s_fit.h; ++oy) {
        if (walkanim::srcIndex(oy, s_fit.h, s_box.y0, bh) != ry) continue;
        for (int fi = 0; fi < s_kept; ++fi) {
            int f = fi * s_k;
            uint16_t* px = pixelsAt(s_slot, fi);
            uint8_t* m = maskAt(s_slot, fi);
            for (int ox = 0; ox < s_fit.w; ++ox) {
                int sx = f * s_frameW + walkanim::srcIndex(ox, s_fit.w, s_box.x0, bw);
                px[oy * s_fit.w + ox] = s_line[sx];
                m[oy * s_fit.w + ox] = opaqueAt(sx);
            }
        }
    }
    return 1;
}

// frameW <= 0 means "the whole image width is one frame" (fallback sprite).
static Res decodeRegion(uint8_t* data, size_t n, int rowY0, int rowH, int frameW, int maxFrames,
                        bool pmd) {
    PNG& png = img::decoder();
    s_rowY0 = rowY0;
    for (s_pass = 1; s_pass <= 2; ++s_pass) {
        if (png.openRAM(data, (int)n, regionDraw) != PNG_SUCCESS) return Res::Corrupt;
        if (s_pass == 1) {
            int w = png.getWidth(), h = png.getHeight();
            if (w > SHEET_MAX_W || !walkanim::pngFits(w, png.getPixelType(), png.getBpp())) {
                png.close();
                return Res::Unsupported;
            }
            s_frameW = frameW > 0 ? frameW : w;
            s_rowH = rowH > 0 ? rowH : h;
            if (h < s_rowY0 + s_rowH) { png.close(); return Res::Corrupt; }
            s_frames = w / s_frameW;
            if (maxFrames < s_frames) s_frames = maxFrames;
            if (s_frames < 1) { png.close(); return Res::Corrupt; }
            s_keyMode = !pmd && !png.hasAlpha();
            s_box = walkanim::emptyBox();
        }
        int rc = png.decode(nullptr, 0);
        png.close();
        if (rc != PNG_SUCCESS) return Res::Corrupt;
        if (s_pass == 1) {
            if (walkanim::isEmpty(s_box)) return Res::Corrupt;
            s_fit = walkanim::fitBand(s_box.x1 - s_box.x0 + 1, s_box.y1 - s_box.y0 + 1, BAND_H, MAX_W);
            s_k = walkanim::keepEvery(s_frames, s_fit.w, s_fit.h, CAP_BYTES);
            if (s_k == 0) return Res::Unsupported;
            s_kept = walkanim::keptCount(s_frames, s_k);
            g_info[s_slot] = {false, pmd, s_fit.w, s_fit.h, s_kept};   // layout for pass 2
        }
    }
    return Res::Ok;
}

// ---------------------------------------------------------------- PMD sheet
static const int DIR_RIGHT = 2;          // PMD row order: Down, DownRight, Right, ...

// Cache-or-download into the scratch buffer (NUL-terminated).
static bool getCached(const String& path, const String& url, size_t* n, int* code) {
    *code = 0;
    if (cache::readInto(path, g_scratch, SCRATCH, n)) return true;
    if (!fetch::httpsGetInto(url.c_str(), g_scratch, SCRATCH, n, code)) return false;
    cache::savePath(path, g_scratch, *n);
    return true;
}

bool loadPmd(int dex) {
    s_slot = staged();
    g_info[s_slot].ready = false;
    g_dex[s_slot] = 0;
    if (dex < 1 || !g_pool[s_slot] || !g_scratch) return false;
    char base[96];
    snprintf(base, sizeof(base),
             "https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/", dex);
    String xmlPath = "/pmd/" + String(dex) + ".xml";
    String pngPath = "/pmd/" + String(dex) + ".png";

    size_t n = 0; int code = 0;
    if (!getCached(xmlPath, String(base) + "AnimData.xml", &n, &code)) {
        Serial.printf("[walk] fallback (xml http %d)\n", code);
        return false;
    }
    animdata::WalkAnim a = animdata::parseWalk((const char*)g_scratch);   // scratch is free after this
    if (!a.ok) {
        cache::removePath(xmlPath);
        Serial.println("[walk] fallback (no Walk anim)");
        return false;
    }
    if (!getCached(pngPath, String(base) + "Walk-Anim.png", &n, &code)) {
        Serial.printf("[walk] fallback (png http %d or > %d B)\n", code, SCRATCH);
        return false;
    }
    Res r = decodeRegion(g_scratch, n, DIR_RIGHT * a.frameH, a.frameH, a.frameW, a.frames, true);
    if (r != Res::Ok) {
        g_info[s_slot].ready = false;
        if (r == Res::Corrupt) {   // corrupt: drop the cache so the next play re-downloads
            cache::removePath(xmlPath);
            cache::removePath(pngPath);
        }
        Serial.printf("[walk] fallback (sheet %s)\n", r == Res::Corrupt ? "corrupt" : "unsupported");
        return false;
    }
    walkanim::mergedDurationsMs(a.ticks, s_frames, s_k, g_dur[s_slot]);
    g_info[s_slot].ready = true;
    g_dex[s_slot] = dex;
    Serial.printf("[walk] pmd #%d %d frames %dx%d (k=%d)\n", dex, g_info[s_slot].frames,
                  g_info[s_slot].w, g_info[s_slot].h, s_k);
    return true;
}

// ---------------------------------------------------------------- fallback
bool loadFallback(const char* spriteUrl, int dex) {
    s_slot = staged();
    g_info[s_slot].ready = false;
    g_dex[s_slot] = 0;
    if (!g_pool[s_slot] || !g_scratch) return false;
    size_t n = 0;
    bool fromCache = false;
    if (!img::loadSpriteInto(dex, spriteUrl, g_scratch, SCRATCH, &n, &fromCache)) {
        Serial.println("[walk] no sprite");
        return false;
    }
    Res r = decodeRegion(g_scratch, n, 0, 0, 0, 1, false);
    if (r != Res::Ok) {
        g_info[s_slot].ready = false;
        if (r == Res::Corrupt && fromCache) cache::removePath(cache::spritePath(dex));
        Serial.printf("[walk] no walker (sprite %s)\n", r == Res::Corrupt ? "corrupt" : "unsupported");
        return false;
    }
    g_dur[s_slot][0] = 0;
    g_info[s_slot].ready = true;
    g_dex[s_slot] = dex;
    Serial.printf("[walk] fallback #%d sprite %dx%d\n", dex, g_info[s_slot].w, g_info[s_slot].h);
    return true;
}

}
```

- [ ] **Step 8: Host suites, build, flash, observe**

All suites → `OK`; `grep -rn "httpsGet(\|readAll\|loadSpriteBytes" src` prints nothing; build + flash; serial 120 s with skips.
Expected: `[walk] pmd #N …` for most tracks, `[walk] fallback …` + `[walk] fallback #N sprite …` when a sheet is missing/too big; `[mem] …` after each step; no `[allocfail]`.

- [ ] **Step 9: Commit**

```bash
git add src/util/walkanim.h src/util/walkanim.cpp test/test_walkanim/test_walkanim.cpp src/images/fetch.h src/images/fetch.cpp src/images/cache.h src/images/cache.cpp src/images/png.h src/images/png.cpp src/images/walksprite.h src/images/walksprite.cpp
git commit -m "fix(walk): decode walkers from a fixed scratch buffer, guard PNG pitch, fix cache rules"
```

---

### Task 17: Lyrics in a fixed arena (synced only, streamed JSON)

**Files:**
- Create: `src/util/lyricbuf.h`, `src/util/lyricbuf.cpp`, `test/test_lyricbuf/test_lyricbuf.cpp`
- Delete: `src/util/lrc.h`, `src/util/lrc.cpp`, `test/test_lrc/test_lrc.cpp`
- Rewrite: `src/lyrics/lrclib.h`, `src/lyrics/lrclib.cpp`
- Modify: `src/core/shared.h`, `src/core/shared.cpp`, `src/core/nettask.cpp`, `src/main.cpp`

**Interfaces:**
- Produces: `lyricbuf::MAX_LINES = 256`, `lyricbuf::TEXT_CAP = 6144`, `struct lyricbuf::Line{uint32_t tMs; uint16_t off;}`, `struct lyricbuf::Lyrics{int n; Line lines[MAX_LINES]; char text[TEXT_CAP];}`, `int lyricbuf::parse(const char* lrc, Lyrics& out)`, `int lyricbuf::currentIndex(const Lyrics&, uint32_t posMs)`, `const char* lyricbuf::lineText(const Lyrics&, int i)`;
  `lyricbuf::Lyrics& lyricsvc::arena()`, `bool lyricsvc::fetchInto(const AppState&, lyricbuf::Lyrics& out)`;
  `shared::lyricsInvalidate()`, `shared::postLyrics(uint32_t gen)`, `bool shared::lyricLine(uint32_t gen, uint32_t posMs, char* out, size_t len)`.

- [ ] **Step 1: Write the failing test `test/test_lyricbuf/test_lyricbuf.cpp`**

```cpp
#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/lyricbuf.h"

static lyricbuf::Lyrics L;   // 8 KB: keep it off the stack
void setUp() {}
void tearDown() {}

void test_parses_timestamped_lines() {
    TEST_ASSERT_EQUAL_INT(2, lyricbuf::parse("[00:01.00]hello\n[00:03.50]world\n", L));
    TEST_ASSERT_EQUAL_UINT32(1000, L.lines[0].tMs);
    TEST_ASSERT_EQUAL_STRING("hello", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_UINT32(3500, L.lines[1].tMs);
    TEST_ASSERT_EQUAL_STRING("world", lyricbuf::lineText(L, 1));
}
void test_skips_malformed() {
    TEST_ASSERT_EQUAL_INT(1, lyricbuf::parse("garbage\n[00:02.00]ok\n[bad]x\n", L));
    TEST_ASSERT_EQUAL_STRING("ok", lyricbuf::lineText(L, 0));
}
void test_crlf_and_empty_text() {
    TEST_ASSERT_EQUAL_INT(2, lyricbuf::parse("[00:01.00]a\r\n[00:02.00]\r\n", L));
    TEST_ASSERT_EQUAL_STRING("a", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_STRING("", lyricbuf::lineText(L, 1));
}
void test_sorts_by_time() {
    lyricbuf::parse("[00:05.00]c\n[00:01.00]a\n[00:03.00]b\n", L);
    TEST_ASSERT_EQUAL_STRING("a", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_STRING("b", lyricbuf::lineText(L, 1));
    TEST_ASSERT_EQUAL_STRING("c", lyricbuf::lineText(L, 2));
}
void test_current_index() {
    lyricbuf::parse("[00:01.00]a\n[00:03.00]b\n[00:05.00]c\n", L);
    TEST_ASSERT_EQUAL_INT(-1, lyricbuf::currentIndex(L, 500));
    TEST_ASSERT_EQUAL_INT(0, lyricbuf::currentIndex(L, 2000));
    TEST_ASSERT_EQUAL_INT(1, lyricbuf::currentIndex(L, 3000));
    TEST_ASSERT_EQUAL_INT(2, lyricbuf::currentIndex(L, 9000));
}
void test_text_overflow_drops_the_rest() {
    std::string s;
    for (int i = 0; i < 200; ++i) {   // 200 lines x 50 chars > 6144 B of text
        char tag[16];
        snprintf(tag, sizeof(tag), "[%02d:%02d.00]", i / 60, i % 60);
        s += tag + std::string(50, 'x') + "\n";
    }
    int n = lyricbuf::parse(s.c_str(), L);
    TEST_ASSERT_TRUE(n > 100 && n < 200);
    TEST_ASSERT_EQUAL_INT(50, (int)strlen(lyricbuf::lineText(L, n - 1)));   // last kept line intact
}
void test_null_and_out_of_range() {
    TEST_ASSERT_EQUAL_INT(0, lyricbuf::parse(nullptr, L));
    TEST_ASSERT_EQUAL_STRING("", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_INT(-1, lyricbuf::currentIndex(L, 1000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_timestamped_lines);
    RUN_TEST(test_skips_malformed);
    RUN_TEST(test_crlf_and_empty_text);
    RUN_TEST(test_sorts_by_time);
    RUN_TEST(test_current_index);
    RUN_TEST(test_text_overflow_drops_the_rest);
    RUN_TEST(test_null_and_out_of_range);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lyricbuf\test_lyricbuf.cpp`
Expected: `COMPILE FAILED` (`lyricbuf.h` missing).

- [ ] **Step 3: Create `src/util/lyricbuf.h` / `lyricbuf.cpp`**

```cpp
// lyricbuf.h
#pragma once
#include <cstdint>
// Synced lyrics in one fixed arena (no per-line heap strings). PURE, host-tested.
// Replaced std::vector<std::string>, whose scattered allocations pinned fragments of
// byte-addressable RAM for the whole song (README "Design & performance history").
namespace lyricbuf {
constexpr int MAX_LINES = 256;
constexpr int TEXT_CAP = 6144;
struct Line { uint32_t tMs; uint16_t off; };
struct Lyrics { int n; Line lines[MAX_LINES]; char text[TEXT_CAP]; };
// Parses "[mm:ss.xx]text" lines into `out` in time order; lines without a valid tag are
// skipped; once the arena is full the rest is dropped. Returns out.n.
int parse(const char* lrc, Lyrics& out);
int currentIndex(const Lyrics& l, uint32_t posMs);   // -1 before the first line
const char* lineText(const Lyrics& l, int i);         // "" if out of range
}
```

```cpp
// lyricbuf.cpp
#include "lyricbuf.h"
#include <cstdlib>
#include <cstring>

namespace lyricbuf {

// Parses the tag at the start of [p, end); on success sets tMs and the text start.
static bool parseTag(const char* p, const char* end, uint32_t* tMs, const char** text) {
    if (end - p < 10 || p[0] != '[') return false;
    const char* close = (const char*)memchr(p, ']', end - p);
    if (!close) return false;
    const char* colon = (const char*)memchr(p, ':', close - p);
    if (!colon) return false;
    char* stop = nullptr;
    long mm = strtol(p + 1, &stop, 10);
    if (stop != colon) return false;
    long ss = strtol(colon + 1, &stop, 10);
    long cs = 0;
    if (stop < close && *stop == '.') cs = strtol(stop + 1, nullptr, 10);
    if (mm < 0 || ss < 0 || ss > 59) return false;
    *tMs = (uint32_t)(mm * 60000 + ss * 1000 + cs * 10);
    *text = close + 1;
    return true;
}

int parse(const char* lrc, Lyrics& out) {
    out.n = 0;
    out.text[0] = '\0';
    if (!lrc) return 0;
    int used = 1;                         // text[0] is the shared "" for out-of-range lookups
    for (const char* p = lrc; *p && out.n < MAX_LINES;) {
        const char* eol = strchr(p, '\n');
        const char* end = eol ? eol : p + strlen(p);
        uint32_t t;
        const char* txt;
        if (parseTag(p, end, &t, &txt)) {
            const char* tend = end;
            while (tend > txt && (tend[-1] == '\r' || tend[-1] == '\n')) --tend;
            int len = (int)(tend - txt);
            if (used + len + 1 > TEXT_CAP) break;   // arena full: drop the rest
            memcpy(out.text + used, txt, len);
            out.text[used + len] = '\0';
            out.lines[out.n++] = {t, (uint16_t)used};
            used += len + 1;
        }
        if (!eol) break;
        p = eol + 1;
    }
    for (int i = 1; i < out.n; ++i) {     // insertion sort by time (input is nearly sorted)
        Line v = out.lines[i];
        int j = i - 1;
        while (j >= 0 && out.lines[j].tMs > v.tMs) { out.lines[j + 1] = out.lines[j]; --j; }
        out.lines[j + 1] = v;
    }
    return out.n;
}

int currentIndex(const Lyrics& l, uint32_t posMs) {
    int idx = -1;
    for (int i = 0; i < l.n; ++i) {
        if (l.lines[i].tMs <= posMs) idx = i; else break;
    }
    return idx;
}

const char* lineText(const Lyrics& l, int i) {
    return (i >= 0 && i < l.n) ? l.text + l.lines[i].off : "";
}

}
```

Note: `parse` sets `out.text[0] = '\0'` and lines start at offset 1, so `lineText` for an
empty-text line points at a real NUL inside the arena. If adding the 8 KB static arena overflows
`dram0_0_seg` at link time (it happened once with a static pool), allocate it instead with `malloc`
in a `lyricsvc::begin()` called from `setup()` right after `art::begin()` (ledger a Ruling).

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lyricbuf\test_lyricbuf.cpp src\util\lyricbuf.cpp`
Expected: `7 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Rewrite `src/lyrics/lrclib.h` / `lrclib.cpp`**

```cpp
// lrclib.h
#pragma once
#include "app_state.h"
#include "../util/lyricbuf.h"
namespace lyricsvc {
// The one lyrics arena (8 KB, static). Written by the network task, read by the UI through
// shared::lyricLine() — never directly.
lyricbuf::Lyrics& arena();
// Fetches synced lyrics from LRCLIB for the current track (original accented names) and
// parses them into `out`. True if at least one synced line was found.
bool fetchInto(const AppState& st, lyricbuf::Lyrics& out);
}
```

```cpp
// lrclib.cpp
#include "lrclib.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cctype>

namespace lyricsvc {

static lyricbuf::Lyrics g_arena;
lyricbuf::Lyrics& arena() { return g_arena; }

static String urlEncode(const char* s) {
    String out;
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) out += (char)c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); out += b; }
    }
    return out;
}

bool fetchInto(const AppState& st, lyricbuf::Lyrics& out) {
    out.n = 0;
    if (!st.trackName[0]) return false;
    String url = "https://lrclib.net/api/get?track_name=" + urlEncode(st.trackName) +
                 "&artist_name=" + urlEncode(st.artist) +
                 "&album_name=" + urlEncode(st.album) +
                 "&duration=" + String(st.durationMs / 1000);

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient https;
    https.useHTTP10(true);          // plain body: parse straight from the stream
    https.setTimeout(8000);
    if (!https.begin(client, url)) return false;
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    int rc = https.GET();
    if (rc != 200) {                 // 404 = no match
        Serial.printf("[lyrics] GET rc=%d\n", rc);
        https.end();
        return false;
    }
    JsonDocument filter;
    filter["syncedLyrics"] = true;   // plain lyrics can't be timed; don't even buffer them
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getStream(),
                                               DeserializationOption::Filter(filter));
    https.end();
    if (err) { Serial.printf("[lyrics] json %s\n", err.c_str()); return false; }
    int n = lyricbuf::parse(doc["syncedLyrics"] | "", out);
    Serial.printf("[lyrics] synced lines=%d\n", n);
    return n > 0;
}

}
```

- [ ] **Step 6: Replace the lyrics mailbox in `src/core/shared.*`**

`shared.h`: remove `#include <vector>` and `#include "../util/lrc.h"`; replace the `postLyrics(uint32_t gen, std::vector<...>*)` and `takeLyrics(...)` declarations with:

```cpp
// Lyrics live in lyricsvc::arena(). The network task calls lyricsInvalidate() before parsing
// into it and postLyrics(gen) after; the UI copies the current line under the lock.
void lyricsInvalidate();
void postLyrics(uint32_t gen);
// Copies the synced line for posMs into out ("" if none / not valid for gen).
void lyricLine(uint32_t gen, uint32_t posMs, char* out, size_t len);
```

`shared.cpp`: add `#include <string.h>` and `#include "../lyrics/lrclib.h"`; replace the `g_lyrics` / `g_lyricsGen` statics with `static uint32_t g_lyricsGen = 0;   // gen the arena is valid for (0 = invalid)`; replace `postLyrics` and `takeLyrics` with:

```cpp
void lyricsInvalidate() { Guard g; g_lyricsGen = 0; }

void postLyrics(uint32_t gen) { Guard g; if (gen == g_state.trackGen) g_lyricsGen = gen; }

void lyricLine(uint32_t gen, uint32_t posMs, char* out, size_t len) {
    Guard g;
    out[0] = '\0';
    if (gen == 0 || g_lyricsGen != gen) return;
    const lyricbuf::Lyrics& l = lyricsvc::arena();
    strncpy(out, lyricbuf::lineText(l, lyricbuf::currentIndex(l, posMs)), len - 1);
    out[len - 1] = '\0';
}
```

- [ ] **Step 7: Network side (`src/core/nettask.cpp`)**

Remove `#include <vector>` and `#include "../util/lrc.h"`; replace the whole `case netplan::Step::Lyrics: { ... }` block with:

```cpp
        case netplan::Step::Lyrics: {
            tick();
            shared::lyricsInvalidate();       // UI stops reading the arena before we overwrite it
            if (lyricsvc::fetchInto(s_st, lyricsvc::arena())) shared::postLyrics(gen);
            tock("lyrics");
            mem::log("lyrics");
            break;
        }
```

- [ ] **Step 8: UI side (`src/main.cpp`)**

- Remove `#include <vector>`, `#include "util/lrc.h"`, the `g_lrcLines` declaration, and the whole `currentLyric(...)` function.
- In the full-redraw branch delete `g_lrcLines.clear();`; replace `ui::drawLyricArea(tft, currentLyric(view.progressMs));` with:

```cpp
        char line[160];
        shared::lyricLine(shownGen, view.progressMs, line, sizeof(line));
        ui::drawLyricArea(tft, line);
```

- In the steady-state branch delete the `std::vector<lrc::LrcLine>* lines ...` / `takeLyrics` block (4 lines), and inside `if (now - lastDraw >= 250) { ... }` replace `ui::drawLyricArea(tft, currentLyric(view.progressMs));` with:

```cpp
            char line[160];
            shared::lyricLine(shownGen, view.progressMs, line, sizeof(line));
            ui::drawLyricArea(tft, line);
```

- [ ] **Step 9: Delete the old LRC module**

`git rm src/util/lrc.h src/util/lrc.cpp test/test_lrc/test_lrc.cpp`; `grep -rn "lrc::\|util/lrc.h" src test` prints nothing.

- [ ] **Step 10: Host suites, build, flash, observe**

All suites (without `test_lrc`, with `test_lyricbuf`, `test_artmap`) → `OK`; build + flash; serial 120 s.
Expected: `[lyrics] synced lines=N` for songs LRCLIB has, the lyric line appears in the dialogue box and advances; `[mem] lyrics …` largest ≥ 25 000; no `[allocfail]`.

- [ ] **Step 11: Commit**

```bash
git add src/util/lyricbuf.h src/util/lyricbuf.cpp test/test_lyricbuf/test_lyricbuf.cpp src/lyrics/lrclib.h src/lyrics/lrclib.cpp src/core/shared.h src/core/shared.cpp src/core/nettask.cpp src/main.cpp
git rm src/util/lrc.h src/util/lrc.cpp test/test_lrc/test_lrc.cpp
git commit -m "perf(lyrics): stream synced lyrics into a fixed arena"
```

---

### Task 18: Self-healing ladder

**Files:**
- Modify: `src/util/netplan.h`, `src/util/netplan.cpp`, `test/test_netplan/test_netplan.cpp`, `src/core/nettask.cpp`

**Interfaces:**
- Produces: `class netplan::Health { enum class Action { None, PauseOptional, Restart }; Action onPoll(bool ok, bool wifiUp, uint32_t nowMs); bool optionalPaused() const; }`, `Health::FAILS_TO_PAUSE = 2`, `Health::RESTART_AFTER_MS = 180000`.

- [ ] **Step 1: Add failing tests** (register with `RUN_TEST`)

```cpp
using Act = netplan::Health::Action;
void test_health_success_is_none() {
    netplan::Health h;
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(true, true, 1000));
    TEST_ASSERT_FALSE(h.optionalPaused());
}
void test_health_two_failures_pause_optional_until_success() {
    netplan::Health h;
    h.onPoll(true, true, 1000);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(false, true, 5000));
    TEST_ASSERT_EQUAL_INT((int)Act::PauseOptional, (int)h.onPoll(false, true, 9000));
    TEST_ASSERT_TRUE(h.optionalPaused());
    h.onPoll(true, true, 13000);
    TEST_ASSERT_FALSE(h.optionalPaused());
}
void test_health_restart_after_180s_with_wifi_up() {
    netplan::Health h;
    h.onPoll(true, true, 1000);
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, 1000 + 179000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, 1000 + 180000));
}
void test_health_wifi_down_never_restarts_and_resets_timer() {
    netplan::Health h;
    h.onPoll(true, true, 1000);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(false, false, 500000));
    // WiFi back at 500 s: the 180 s clock starts again from there
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, 504000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, 500000 + 180000));
}
void test_health_first_call_failing_starts_clock() {
    netplan::Health h;
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, 900000));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `COMPILE FAILED` (`Health` not a member).

- [ ] **Step 3: Implement `Health` in `netplan.h`** (header-only, after `LinkGate`)

```cpp
// Self-healing ladder. A heap so fragmented that TLS can't allocate never recovered by itself
// (README "Design & performance history"), so: 2 failed polls in a row with WiFi up pause the
// optional downloads; 180 s without a good poll while WiFi is up -> restart. WiFi down is the
// reconnect logic's job and restarts the 180 s clock.
class Health {
public:
    enum class Action { None, PauseOptional, Restart };
    static constexpr int FAILS_TO_PAUSE = 2;
    static constexpr uint32_t RESTART_AFTER_MS = 180000;
    Action onPoll(bool ok, bool wifiUp, uint32_t nowMs) {
        if (!started_) { started_ = true; lastOk_ = nowMs; }
        if (ok) { fails_ = 0; lastOk_ = nowMs; paused_ = false; return Action::None; }
        ++fails_;
        if (!wifiUp) { lastOk_ = nowMs; return Action::None; }
        if (nowMs - lastOk_ >= RESTART_AFTER_MS) return Action::Restart;
        if (fails_ >= FAILS_TO_PAUSE) { paused_ = true; return Action::PauseOptional; }
        return Action::None;
    }
    bool optionalPaused() const { return paused_; }
private:
    int fails_ = 0;
    uint32_t lastOk_ = 0;
    bool started_ = false;
    bool paused_ = false;
};
```

- [ ] **Step 4: Run to verify it passes**

Run the Step 2 command. Expected: `15 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Wire it in `src/core/nettask.cpp`**

Add `static netplan::Health s_health;` next to `s_link`. After the poll block's `if (!ok) mem::log("poll failed");` add:

```cpp
            if (s_health.onPoll(ok, net::isOnline(), now) == netplan::Health::Action::Restart) {
                mem::log("restart");
                Serial.println("[net] no good poll for 180 s with WiFi up: restarting");
                delay(200);
                ESP.restart();
            }
```

and in the step selection, gate optional work — replace
`if (netplan::canRun(step, (unsigned)mem::byteLargest())) {` with

```cpp
                bool optional = step != netplan::Step::Art;
                if (!(optional && s_health.optionalPaused()) &&
                    netplan::canRun(step, (unsigned)mem::byteLargest())) {
```

- [ ] **Step 6: Build, flash, force the restart path once**

Temporarily change `RESTART_AFTER_MS` to `30000` and make `spclient::poll` fail by setting `s_st.status = PlaybackStatus::Offline;` right after the `spclient::poll(s_st);` call (both marked `// TEMP`). Flash; serial 90 s.
Expected: `[mem] poll failed …` every 4 s, then `[net] no good poll for 180 s with WiFi up: restarting` ~30 s later and a fresh boot (`rst:`). **Revert both TEMP edits**, re-run the Step 2 tests, rebuild, flash.

- [ ] **Step 7: Commit**

```bash
git add src/util/netplan.h test/test_netplan/test_netplan.cpp src/core/nettask.cpp
git commit -m "fix(net): pause optional work and restart when polls keep failing with WiFi up"
```

---

### Task 19: Robustness — late hotspot, timeouts, staleness, track identity

**Files:**
- Modify: `include/app_state.h`, `src/spotify/client.cpp`, `src/util/netplan.h`, `test/test_netplan/test_netplan.cpp`, `src/core/nettask.h`, `src/core/nettask.cpp`, `src/main.cpp`

**Interfaces:**
- Produces: `AppState::trackUri[64]`, `AppState::lastPollOkMs`; `bool netplan::stale(uint32_t nowMs, uint32_t lastOkMs, uint32_t limitMs)`; `void nettask::start(bool spotifyReady)`.

- [ ] **Step 1: Add failing test** (register with `RUN_TEST`)

```cpp
void test_stale_after_limit_and_handles_wrap() {
    TEST_ASSERT_FALSE(netplan::stale(10000, 0, 20000));        // never polled: not "stale"
    TEST_ASSERT_FALSE(netplan::stale(30000, 10000, 20000));
    TEST_ASSERT_TRUE(netplan::stale(30001, 10000, 20000));
    TEST_ASSERT_TRUE(netplan::stale(5000, 0xFFFFF000u, 2000)); // millis() wrapped
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `COMPILE FAILED` (`stale` not declared).

- [ ] **Step 3: Implement in `netplan.h`** (after `Health`)

```cpp
// True when a successful poll happened (lastOkMs != 0) more than limitMs ago (wrap-safe).
inline bool stale(uint32_t nowMs, uint32_t lastOkMs, uint32_t limitMs) {
    return lastOkMs != 0 && nowMs - lastOkMs > limitMs;
}
```

- [ ] **Step 4: Run to verify it passes**

Expected: `16 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Track URI + poll timestamp**

`include/app_state.h`: after `char albumArtUrl[160];` add `char trackUri[64];     // spotify:track:… — identity for track changes`, and after `uint32_t trackGen;` add `uint32_t lastPollOkMs; // millis() of the last successful poll (0 = none yet)`.

`src/spotify/client.cpp`:
- in `onPlaying`, after the `copyStr(st.trackName, …)` line add `copyStr(st.trackUri, cp.trackUri, sizeof(st.trackUri));`;
- in `begin()`, after `client.setInsecure();` add `client.setHandshakeTimeout(8);   // default 120 s froze polls on a half-dead hotspot`.

- [ ] **Step 6: Network task: late hotspot, identity, staleness (`src/core/nettask.*`)**

`nettask.h`: replace `void start();` and its comment with

```cpp
// Starts the task. spotifyReady = setup() already connected WiFi and ran spclient::begin();
// otherwise the task connects WiFi itself (hotspot that comes up after the deck) first.
void start(bool spotifyReady);
```

`nettask.cpp`:
- add `static bool s_spotifyReady = false;` next to the other statics;
- at the top of the `for (;;)` loop in `run()`, before `net::loop();`, add:

```cpp
        if (!s_spotifyReady) {                 // WiFi wasn't up at boot: keep trying
            if (net::isOnline() || net::connectAny()) {
                spclient::begin();
                s_spotifyReady = true;
                mem::log("late wifi");
            } else {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }
```

- in the poll block, after `bool ok = s_st.status != PlaybackStatus::Offline;` add `if (ok) s_st.lastPollOkMs = millis();`;
- replace `if (s_gen.update(s_st.trackName)) onTrackChange();` with

```cpp
            const char* id = s_st.trackUri[0] ? s_st.trackUri : s_st.trackName;   // URI: same-title songs differ
            if (s_gen.update(id)) onTrackChange();
```

- replace the `void start()` definition with

```cpp
void start(bool spotifyReady) {
    s_spotifyReady = spotifyReady;
    xTaskCreatePinnedToCore(run, "net", 10240, nullptr, 1, nullptr, 0);
}
```

(keep the stack-size comment above it).

- [ ] **Step 7: UI (`src/main.cpp`)**

- In `setup()`, replace the WiFi block with:

```cpp
    bool spotifyReady = false;
    if (net::connectAny()) {
        mem::log("boot+wifi");
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Spotify auth...", 10, 10, 2);
        if (spauth::loadRefreshToken().isEmpty()) {
            tft.drawString("Open http://" + net::deviceIp() + "/", 10, 40, 2);
        }
        spauth::runSetupPortalIfNeeded();
        spclient::begin();
        spotifyReady = true;
    } else {
        tft.drawString("WiFi not found - retrying...", 10, 40, 2);
    }
    nettask::start(spotifyReady);   // always: it keeps retrying WiFi if the hotspot is late
```

- add `#include "util/netplan.h"` and change the offline condition to

```cpp
    if (!net::isOnline() || st.status == PlaybackStatus::Offline ||
        netplan::stale(millis(), st.lastPollOkMs, 20000)) mode = 1;
```

- [ ] **Step 8: Host suites, build, flash, observe**

All suites → `OK`; build + flash. Device checks:
- Boot with the hotspot **off**, turn it on after ~30 s → `[wifi] connected`, `[mem] late wifi`, deck appears without a reboot.
- Normal play: track changes still detected (`[net] track gen=…`).
- Hotspot on but mobile data off (if possible) → "No signal..." within ~20–30 s, recovery when data returns.

- [ ] **Step 9: Commit**

```bash
git add include/app_state.h src/spotify/client.cpp src/util/netplan.h test/test_netplan/test_netplan.cpp src/core/nettask.h src/core/nettask.cpp src/main.cpp
git commit -m "fix(net): retry late hotspot, 8 s TLS timeouts, stale-poll signal, track URI identity"
```

---

### Task 20: Verification session + history

**Files:**
- Create: `tools/capture_serial.py`, `tools/analyze_session.py`
- Modify: `README.md`

- [ ] **Step 1: Add the session tools**

Create `tools/capture_serial.py` (timestamped capture; needs `pyserial`):

```python
"""Capture timestamped serial output: python tools/capture_serial.py COM11 600 out.log"""
import sys
import time

import serial

port, secs, path = sys.argv[1], float(sys.argv[2]), sys.argv[3]
with serial.Serial(port, 115200, timeout=1) as s, open(path, "w", encoding="utf-8") as out:
    t0 = time.time()
    while time.time() - t0 < secs:
        line = s.readline()
        if line:
            out.write("%7.1f %s" % (time.time() - t0, line.decode(errors="replace")))
            out.flush()
print("wrote", path)
```

Create `tools/analyze_session.py` (the analysis used for the Part 2 session, extended): reads a
timestamped capture (`<seconds> <serial line>`), prints per-step duration stats from `[net] <step> <ms>ms`,
per-track arrival times (art / lyrics / walk / prefetch after `[net] track gen=`), `[mem]`
free/largest min/max, and counts of `[allocfail]`, `-32512`, `poll HTTP -1`, `[fetch]`, restarts (`rst:`).

```python
"""Analyze a timestamped serial capture from the deck: python tools/analyze_session.py <log>"""
import collections
import re
import statistics
import sys

lines = []
for raw in open(sys.argv[1], encoding="utf-8", errors="replace"):
    m = re.match(r"\s*([\d.]+) (.*)", raw.rstrip("\n"))
    if m:
        lines.append((float(m.group(1)), m.group(2)))
print("lines:", len(lines), " span: %.0fs" % (lines[-1][0] - lines[0][0] if lines else 0))

steps = collections.defaultdict(list)
for t, s in lines:
    m = re.match(r"\[net\] (poll|player|art|art failed|lyrics|walk|prefetch|prefetch failed) (\d+)ms", s)
    if m:
        steps[m.group(1)].append(int(m.group(2)))
print("\n== step durations (ms) ==")
for k, v in sorted(steps.items()):
    v = sorted(v)
    print("%-16s n=%3d min=%5d median=%5d p90=%5d max=%5d" % (
        k, len(v), v[0], statistics.median(v), v[int(0.9 * (len(v) - 1))], v[-1]))

print("\n== tracks (seconds after the track change) ==")
idx = [(i, t, s) for i, (t, s) in enumerate(lines) if s.startswith("[net] track gen=")]
for k, (i, t, s) in enumerate(idx):
    end = idx[k + 1][0] if k + 1 < len(idx) else len(lines)
    got = {}
    for t2, s2 in lines[i + 1:end]:
        for key in ("art", "lyrics", "walk", "prefetch"):
            if key not in got and re.match(r"\[net\] %s \d+ms" % key, s2):
                got[key] = t2 - t
    print("%-40s %s" % (s[:40], "  ".join("%s=%.1f" % kv for kv in got.items())))

mem = [(int(a), int(b)) for _, s in lines for m in [re.search(r"\[mem\].*free=(\d+) largest=(\d+)", s)]
       if m for a, b in [m.groups()]]
if mem:
    print("\n== byte-addressable RAM ==\nfree min=%d max=%d  largest min=%d max=%d" % (
        min(a for a, _ in mem), max(a for a, _ in mem), min(b for _, b in mem), max(b for _, b in mem)))

print("\n== failures ==")
for name, pat in (("allocfail", r"\[allocfail\]"), ("TLS alloc -32512", "-32512"),
                  ("poll HTTP -1", r"poll HTTP -1"), ("fetch errors", r"\[fetch\]"),
                  ("restarts", r"rst:|restarting"), ("deferred steps", r"\[mem\] defer")):
    print("%-18s %d" % (name, sum(1 for _, s in lines if re.search(pat, s))))
```

- [ ] **Step 2: Run a 10-minute logged session**

Run `python tools/capture_serial.py COM11 600 <scratchpad>/session-part3.log` while music plays with normal skips, one burst of 3 quick skips and a pause/resume;
then `python tools/analyze_session.py <capture>`.
Expected: `allocfail 0`, `TLS alloc -32512 0`, no restarts, every track change shows art and (when
LRCLIB has it) lyrics; byte-addressable largest block never below ~20 000.

- [ ] **Step 3: Update `README.md`**

Append to "Design & performance history" a numbered item **9. Deterministic memory (Part 3)**
with: the failed-allocation table from the spec §1, the misleading-metric finding, the fixes
(fixed art bitmap over plain HTTP, scratch-buffer walker loaders, lyrics arena, memory gate,
self-healing ladder, late hotspot/timeouts/URI identity), and a before/after table from the two
sessions (stuck time, `[allocfail]` count, art/lyrics times, byte-addressable free/largest). Remove
the "Open:" item 8 text that this resolves, or mark it resolved. Mention `tools/analyze_session.py`
under "Build & flash".

- [ ] **Step 4: Commit**

```bash
git add tools/capture_serial.py tools/analyze_session.py README.md
git commit -m "docs: record Part 3 memory/resilience results and add the session analyzer"
```

---

### Part 3 additions (user request, 2026-10-06)

Execution order for Part 3 becomes: 14 → 15 → 16 → 17 → 18 → 19 → **21 → 22** → 20, so the
Task 20 session also covers these. Spec: `2026-10-06-merged-status-panel-pmd-walker-design.md`
§2.0 (Top strip, Status screens).

### Task 21: Pikachu on the status screens (bundled sprite)

**Files:**
- Create: `tools/gen_status_sprite.py`, `src/ui/status_sprite.inc` (generated), `src/ui/status_sprite.h`, `src/ui/status_sprite.cpp`, `test/test_status_sprite/test_status_sprite.cpp`
- Modify: `src/ui/screen_now.cpp` (`drawOffline`)

**Interfaces:**
- Produces: `int status_sprite::width()`, `int status_sprite::height()`, `bool status_sprite::pixel(int x, int y, uint16_t* rgb565)` (true when opaque; native RGB565 for `fillRect`).

- [ ] **Step 1: Create `tools/gen_status_sprite.py`** (standard library only)

```python
"""Generate src/ui/status_sprite.inc: a Pokemon sprite bundled in flash for the status screens
("No signal...", "Nothing playing"), which show exactly when nothing can be downloaded.
Run: python tools/gen_status_sprite.py [dex]   (default 25 = Pikachu; output is committed)
"""
import os
import struct
import sys
import urllib.request
import zlib

DEX = int(sys.argv[1]) if len(sys.argv) > 1 else 25
URL = "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png" % DEX
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "status_sprite.inc")


def decode_png(data):
    """Minimal decoder: non-interlaced indexed (1/2/4/8-bit) or 8-bit RGBA -> rows of (r,g,b,a)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat, plte, trns = 8, b"", [], b""
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            plte = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    assert interlace == 0 and (ctype == 3 or (ctype == 6 and depth == 8)), (ctype, depth)
    bits = depth if ctype == 3 else 32
    stride = (w * bits + 7) // 8
    fbpp = max(1, bits // 8)
    raw = zlib.decompress(idat)
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - fbpp] if i >= fbpp else 0
            b = prev[i]
            c = prev[i - fbpp] if i >= fbpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        px = []
        for x in range(w):
            if ctype == 6:
                px.append(tuple(line[x * 4:x * 4 + 4]))
            else:
                bit = x * depth
                idx = (line[bit // 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1)
                px.append(plte[idx] + (trns[idx] if idx < len(trns) else 255,))
        rows.append(px)
    return w, h, rows


w, h, rows = decode_png(urllib.request.urlopen(URL).read())
opaque = [(x, y) for y in range(h) for x in range(w) if rows[y][x][3] >= 128]
x0, x1 = min(x for x, _ in opaque), max(x for x, _ in opaque)
y0, y1 = min(y for _, y in opaque), max(y for _, y in opaque)
cw, ch = x1 - x0 + 1, y1 - y0 + 1

px, mask = [], bytearray((cw * ch + 7) // 8)
for y in range(ch):
    for x in range(cw):
        r, g, b, a = rows[y0 + y][x0 + x]
        i = y * cw + x
        px.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
        if a >= 128:
            mask[i >> 3] |= 0x80 >> (i & 7)

with open(OUT, "w", encoding="ascii", newline="\n") as f:
    f.write("// Generated by tools/gen_status_sprite.py from PokeAPI sprite #%d. Do not edit.\n" % DEX)
    f.write("static const int SPRITE_W = %d, SPRITE_H = %d;\n" % (cw, ch))
    f.write("static const uint16_t SPRITE_PX[%d] = {\n" % len(px))
    for i in range(0, len(px), 12):
        f.write("    " + ", ".join("0x%04X" % v for v in px[i:i + 12]) + ",\n")
    f.write("};\n")
    f.write("static const uint8_t SPRITE_MASK[%d] = {\n" % len(mask))
    for i in range(0, len(mask), 16):
        f.write("    " + ", ".join("0x%02X" % v for v in mask[i:i + 16]) + ",\n")
    f.write("};\n")
print("wrote %s (%dx%d)" % (OUT, cw, ch))
```

- [ ] **Step 2: Generate**

Run: `python tools/gen_status_sprite.py`
Expected: `wrote ...status_sprite.inc (WxH)` with W, H roughly 30–60.

- [ ] **Step 3: Write the failing test `test/test_status_sprite/test_status_sprite.cpp`**

```cpp
#include <unity.h>
#include <cstdint>
#include "../../src/ui/status_sprite.h"

void setUp() {}
void tearDown() {}

static bool rowHasOpaque(int y) {
    for (int x = 0; x < status_sprite::width(); ++x)
        if (status_sprite::pixel(x, y, nullptr)) return true;
    return false;
}
static bool colHasOpaque(int x) {
    for (int y = 0; y < status_sprite::height(); ++y)
        if (status_sprite::pixel(x, y, nullptr)) return true;
    return false;
}
void test_size_is_reasonable() {
    TEST_ASSERT_TRUE(status_sprite::width() >= 16 && status_sprite::width() <= 96);
    TEST_ASSERT_TRUE(status_sprite::height() >= 16 && status_sprite::height() <= 96);
}
void test_cropped_to_opaque_bounds() {   // every edge of the crop touches the creature
    TEST_ASSERT_TRUE(rowHasOpaque(0));
    TEST_ASSERT_TRUE(rowHasOpaque(status_sprite::height() - 1));
    TEST_ASSERT_TRUE(colHasOpaque(0));
    TEST_ASSERT_TRUE(colHasOpaque(status_sprite::width() - 1));
}
void test_opaque_pixel_reports_colour() {
    int w = status_sprite::width(), h = status_sprite::height();
    uint16_t c = 0;
    bool found = false;
    for (int y = h / 3; y < h && !found; ++y)
        for (int x = 0; x < w && !found; ++x) found = status_sprite::pixel(x, y, &c);
    TEST_ASSERT_TRUE(found);
}
void test_out_of_range_is_transparent() {
    uint16_t c = 0x1234;
    TEST_ASSERT_FALSE(status_sprite::pixel(-1, 0, &c));
    TEST_ASSERT_FALSE(status_sprite::pixel(0, status_sprite::height(), &c));
    TEST_ASSERT_EQUAL_HEX16(0x1234, c);   // untouched
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_size_is_reasonable);
    RUN_TEST(test_cropped_to_opaque_bounds);
    RUN_TEST(test_opaque_pixel_reports_colour);
    RUN_TEST(test_out_of_range_is_transparent);
    return UNITY_END();
}
```

- [ ] **Step 4: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_status_sprite\test_status_sprite.cpp`
Expected: `COMPILE FAILED` (`status_sprite.h` missing).

- [ ] **Step 5: Create `src/ui/status_sprite.h` / `status_sprite.cpp`**

```cpp
// status_sprite.h
#pragma once
#include <cstdint>
// The Pokemon shown on the status screens, bundled in flash (tools/gen_status_sprite.py):
// those screens appear exactly when nothing can be downloaded. PURE, host-tested.
namespace status_sprite {
int width();
int height();
// True when (x, y) is opaque; then *rgb565 (if given) is its native RGB565 colour.
bool pixel(int x, int y, uint16_t* rgb565);
}
```

```cpp
// status_sprite.cpp
#include "status_sprite.h"

namespace status_sprite {

#include "status_sprite.inc"

int width() { return SPRITE_W; }
int height() { return SPRITE_H; }

bool pixel(int x, int y, uint16_t* rgb565) {
    if (x < 0 || y < 0 || x >= SPRITE_W || y >= SPRITE_H) return false;
    int i = y * SPRITE_W + x;
    if (!((SPRITE_MASK[i >> 3] >> (7 - (i & 7))) & 1)) return false;
    if (rgb565) *rgb565 = SPRITE_PX[i];
    return true;
}

}
```

- [ ] **Step 6: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_status_sprite\test_status_sprite.cpp src\ui\status_sprite.cpp`
Expected: `4 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 7: Draw it in `drawOffline` (`src/ui/screen_now.cpp`)**

Add `#include "status_sprite.h"` and replace the body of `drawOffline` with:

```cpp
void drawOffline(TFT_eSPI& t, const char* msg) {
    background(t);
    topStrip(t);
    // Battle platform + the bundled Pokemon (no network needed), 2x nearest-neighbour.
    const int cx = 160, feetY = 136;
    t.fillEllipse(cx, feetY, 60, 12, theme::HORIZON);
    t.drawEllipse(cx, feetY, 60, 12, theme::BOX_SHADOW);
    int w = status_sprite::width(), h = status_sprite::height();
    int s = (h * 2 <= 110 && w * 2 <= 200) ? 2 : 1;
    int x0 = cx - w * s / 2, y0 = feetY - 4 - h * s;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint16_t c;
            if (status_sprite::pixel(x, y, &c)) t.fillRect(x0 + x * s, y0 + y * s, s, s, c);
        }
    dialogueBox(t, 20, 160, 280, 48);
    shadowText(t, msg, 160, 184, 4, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
}
```

- [ ] **Step 8: Build, flash, observe**

Build + flash; turn the hotspot off (or stop playback). Expected: battle background, Pikachu on
a grass platform, "No signal..." / "Nothing playing" in the dialogue box below; deck returns
when the condition clears.

- [ ] **Step 9: Commit**

```bash
git add tools/gen_status_sprite.py src/ui/status_sprite.inc src/ui/status_sprite.h src/ui/status_sprite.cpp test/test_status_sprite/test_status_sprite.cpp src/ui/screen_now.cpp
git commit -m "feat(ui): show a bundled Pikachu on the status screens"
```

---

### Task 22: "PAUSED" title while paused

**Files:**
- Create: `src/ui/labels.h`, `test/test_labels/test_labels.cpp`
- Modify: `src/ui/screen_now.cpp` (`drawTopStrip`), `src/main.cpp` (top-strip signature)

**Interfaces:**
- Produces: `const char* ui::topTitle(bool isPlaying)`.

- [ ] **Step 1: Write the failing test `test/test_labels/test_labels.cpp`**

```cpp
#include <unity.h>
#include "../../src/ui/labels.h"

void setUp() {}
void tearDown() {}

void test_top_title() {
    TEST_ASSERT_EQUAL_STRING("NOW PLAYING", ui::topTitle(true));
    TEST_ASSERT_EQUAL_STRING("PAUSED", ui::topTitle(false));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_top_title);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_labels\test_labels.cpp`
Expected: `COMPILE FAILED` (`labels.h` missing).

- [ ] **Step 3: Create `src/ui/labels.h`**

```cpp
#pragma once
// User-facing labels. PURE, host-tested.
namespace ui {
// Top-strip title: matches the CD, which spins while playing and freezes while paused.
inline const char* topTitle(bool isPlaying) { return isPlaying ? "NOW PLAYING" : "PAUSED"; }
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_labels\test_labels.cpp`
Expected: `1 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Use it**

`src/ui/screen_now.cpp`: add `#include "labels.h"`; in `drawTopStrip` replace

```cpp
    shadowText(t, "NOW PLAYING", 6, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TL_DATUM);
    g_cdX = 6 + t.textWidth("NOW PLAYING", 2) + 10;
```

with

```cpp
    const char* title = topTitle(st.isPlaying);
    shadowText(t, title, 6, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TL_DATUM);
    g_cdX = 6 + t.textWidth(title, 2) + 10;
```

`src/main.cpp`: in the steady-state branch change the signature to include the play state:

```cpp
        snprintf(sig, sizeof(sig), "%s|%s|%d|%d|%d", st.deviceName, st.deviceType,
                 (int)st.shuffle, st.repeat, (int)st.isPlaying);
```

- [ ] **Step 6: Build, flash, observe**

Build + flash. Pause on the phone → within one poll the top strip reads "PAUSED" with a frozen
CD (and the walker stands still); resume → "NOW PLAYING", CD spins.

- [ ] **Step 7: Commit**

```bash
git add src/ui/labels.h test/test_labels/test_labels.cpp src/ui/screen_now.cpp src/main.cpp
git commit -m "feat(ui): show PAUSED in the top strip while playback is paused"
```

---

### Task 23: Portuguese / Latin-1 accents drawn over the ASCII font

Execution order for Part 3: 14 → 15 → 16 → 17 → 18 → 19 → 21 → 22 → **23** → 20.
Spec: `2026-10-06-merged-status-panel-pmd-walker-design.md` §2.0 "Accented letters".
Font facts (TFT_eSPI `Fonts/Font16.c`, our font 2): 16-row cell; capitals occupy rows 3–12
(`A`), lowercase rows 6–12 (`a`), baseline row 12, rows 13–15 empty.

**Files:**
- Modify: `src/util/text.h`, `src/util/text.cpp`, `test/test_text/test_text.cpp`
- Create: `src/ui/accents.h`, `src/ui/accents.cpp`, `test/test_accents/test_accents.cpp`
- Modify: `src/util/textfit.h`, `src/util/textfit.cpp`, `test/test_textfit/test_textfit.cpp`
- Create: `src/ui/textdraw.h`, `src/ui/textdraw.cpp`
- Modify: `src/ui/screen_now.cpp`

**Interfaces:**
- Produces: `enum class txt::Mark : uint8_t { None, Acute, Grave, Circumflex, Tilde, Diaeresis, Cedilla, Ring };`
  `size_t txt::foldMarks(const char* src, char* dst, txt::Mark* marks, size_t n)` (asciiFold + one mark per output char; `marks` may be nullptr);
  `accents::W = 5`, `accents::H = 2`, `const char* accents::row(txt::Mark, int y)`, `int accents::topRow(txt::Mark, char base)`;
  `textfit::TwoLines` gains `size_t bStart` (index of line b's first char in the source) and `size_t bKeep` (how many chars of b come from the source; the rest is "...");
  `int ui::drawText(TFT_eSPI&, const char* utf8, int x, int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum, int maxW, bool upper = false)` (returns drawn width);
  `int ui::drawFolded(TFT_eSPI&, const char* text, const txt::Mark* marks, size_t nMarks, int x, int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum)`.

- [ ] **Step 1: Failing tests for `foldMarks`** — add to `test/test_text/test_text.cpp` (register with `RUN_TEST`; index map for "Coracao": C0 o1 r2 a3 c4 a5 o6):

```cpp
void test_fold_marks_portuguese() {
    char o[32];
    txt::Mark m[32];
    size_t n = txt::foldMarks("Cora\xC3\xA7\xC3\xA3o", o, m, sizeof(o));   // "Coração"
    TEST_ASSERT_EQUAL_STRING("Coracao", o);
    TEST_ASSERT_EQUAL_INT(7, (int)n);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::None, (int)m[3]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Cedilla, (int)m[4]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Tilde, (int)m[5]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::None, (int)m[6]);
}
void test_fold_marks_upper_and_others() {
    char o[32];
    txt::Mark m[32];
    txt::foldMarks("\xC3\x89""POCA voc\xC3\xAA \xC3\xA0 n\xC3\xA3o", o, m, sizeof(o));   // "ÉPOCA você à não"
    TEST_ASSERT_EQUAL_STRING("EPOCA voce a nao", o);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Acute, (int)m[0]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Circumflex, (int)m[9]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Grave, (int)m[11]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Tilde, (int)m[14]);
}
void test_fold_marks_two_letter_folds_have_no_mark() {
    char o[8];
    txt::Mark m[8];
    txt::foldMarks("\xC3\x86\xC3\x9F", o, m, sizeof(o));   // "Æß"
    TEST_ASSERT_EQUAL_STRING("AEss", o);
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT((int)txt::Mark::None, (int)m[i]);
}
void test_fold_marks_null_marks_matches_ascii_fold() {
    char a[32], b[32];
    txt::asciiFold("N\xC3\xA3o \xC3\xA9", a, sizeof(a));
    txt::foldMarks("N\xC3\xA3o \xC3\xA9", b, nullptr, sizeof(b));
    TEST_ASSERT_EQUAL_STRING(a, b);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_text\test_text.cpp src\util\text.cpp`
Expected: `COMPILE FAILED` (`Mark` / `foldMarks` not declared).

- [ ] **Step 3: Implement in `src/util/text.h` / `text.cpp`**

`text.h`, inside `namespace txt` (add `#include <cstdint>`):

```cpp
// Accent carried by a folded character, so the UI can draw it over the ASCII glyph.
enum class Mark : uint8_t { None, Acute, Grave, Circumflex, Tilde, Diaeresis, Cedilla, Ring };
// asciiFold that also records each output character's accent. `marks` (room for n entries)
// may be nullptr. Letters that fold to two characters (Æ, ß) carry no mark. Returns the
// folded length.
size_t foldMarks(const char* src, char* dst, Mark* marks, size_t n);
```

`text.cpp`: add after `foldLatin1`:

```cpp
// Accent of a Latin-1 letter (U+00C0..U+00FF).
static Mark markLatin1(unsigned int cp) {
    switch (cp) {
        case 0xC0: case 0xC8: case 0xCC: case 0xD2: case 0xD9:
        case 0xE0: case 0xE8: case 0xEC: case 0xF2: case 0xF9: return Mark::Grave;
        case 0xC1: case 0xC9: case 0xCD: case 0xD3: case 0xDA: case 0xDD:
        case 0xE1: case 0xE9: case 0xED: case 0xF3: case 0xFA: case 0xFD: return Mark::Acute;
        case 0xC2: case 0xCA: case 0xCE: case 0xD4: case 0xDB:
        case 0xE2: case 0xEA: case 0xEE: case 0xF4: case 0xFB: return Mark::Circumflex;
        case 0xC3: case 0xD1: case 0xD5: case 0xE3: case 0xF1: case 0xF5: return Mark::Tilde;
        case 0xC4: case 0xCB: case 0xCF: case 0xD6: case 0xDC:
        case 0xE4: case 0xEB: case 0xEF: case 0xF6: case 0xFC: case 0xFF: return Mark::Diaeresis;
        case 0xC5: case 0xE5: return Mark::Ring;
        case 0xC7: case 0xE7: return Mark::Cedilla;
        default: return Mark::None;
    }
}
```

and replace the `asciiFold` function with:

```cpp
size_t foldMarks(const char* src, char* dst, Mark* marks, size_t n) {
    size_t o = 0;
    if (n == 0) return 0;
    if (!src) { dst[0] = '\0'; return 0; }
    for (size_t i = 0; src[i] && o + 1 < n; ) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {                         // plain ASCII
            if (marks) marks[o] = Mark::None;
            dst[o++] = (char)c; ++i;
        } else if (c == 0xC3 && src[i + 1]) {   // U+00C0..00FF accented letters
            unsigned int cp = 0xC0 + ((unsigned char)src[i + 1] - 0x80);
            const char* r = foldLatin1(cp);
            Mark m = (r[0] && !r[1]) ? markLatin1(cp) : Mark::None;   // only 1:1 folds keep it
            for (; *r && o + 1 < n; ++r) {
                if (marks) marks[o] = m;
                dst[o++] = *r;
            }
            i += 2;
        } else if (c == 0xC2 && src[i + 1]) {   // U+0080..00BF symbols -> drop
            i += 2;
        } else if ((c & 0xE0) == 0xC0) { i += 2; }   // skip other 2-byte
        else if ((c & 0xF0) == 0xE0) { i += 3; }     // skip 3-byte
        else if ((c & 0xF8) == 0xF0) { i += 4; }     // skip 4-byte
        else { ++i; }
    }
    dst[o] = '\0';
    return o;
}

void asciiFold(const char* src, char* dst, size_t n) { foldMarks(src, dst, nullptr, n); }
```

- [ ] **Step 4: Run to verify it passes**

Run the Step 2 command. Expected: `8 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 5: Failing tests for the mark art `test/test_accents/test_accents.cpp`**

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/ui/accents.h"

using txt::Mark;
void setUp() {}
void tearDown() {}

void test_every_mark_is_5x2() {
    const Mark all[] = {Mark::Acute, Mark::Grave, Mark::Circumflex, Mark::Tilde,
                        Mark::Diaeresis, Mark::Cedilla, Mark::Ring};
    for (Mark m : all)
        for (int y = 0; y < accents::H; ++y) {
            TEST_ASSERT_NOT_NULL(accents::row(m, y));
            TEST_ASSERT_EQUAL_INT(accents::W, (int)strlen(accents::row(m, y)));
        }
}
void test_none_and_out_of_range() {
    TEST_ASSERT_NULL(accents::row(Mark::None, 0));
    TEST_ASSERT_NULL(accents::row(Mark::Acute, 2));
    TEST_ASSERT_NULL(accents::row(Mark::Acute, -1));
}
void test_top_row_by_case_and_cedilla() {
    TEST_ASSERT_EQUAL_INT(0, accents::topRow(Mark::Acute, 'E'));    // capitals start at row 3
    TEST_ASSERT_EQUAL_INT(3, accents::topRow(Mark::Tilde, 'a'));    // lowercase start at row 6
    TEST_ASSERT_EQUAL_INT(13, accents::topRow(Mark::Cedilla, 'c')); // under the baseline (row 12)
    TEST_ASSERT_EQUAL_INT(13, accents::topRow(Mark::Cedilla, 'C'));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_mark_is_5x2);
    RUN_TEST(test_none_and_out_of_range);
    RUN_TEST(test_top_row_by_case_and_cedilla);
    return UNITY_END();
}
```

- [ ] **Step 6: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_accents\test_accents.cpp`
Expected: `COMPILE FAILED` (`accents.h` missing).

- [ ] **Step 7: Create `src/ui/accents.h` / `accents.cpp`**

```cpp
// accents.h
#pragma once
#include "../util/text.h"
// Pixel art for accent marks drawn over the ASCII glyphs of the 16-px font 2. PURE, tested.
namespace accents {
constexpr int W = 5, H = 2;
// Row y of the mark as '#'/'.' (W chars); nullptr for Mark::None or y out of range.
const char* row(txt::Mark m, int y);
// First cell row of the mark: 0 over capitals (glyphs start at row 3), 3 over lowercase
// (row 6), 13 for the cedilla (under the row-12 baseline).
int topRow(txt::Mark m, char base);
}
```

```cpp
// accents.cpp
#include "accents.h"
#include <cctype>

namespace accents {

const char* row(txt::Mark m, int y) {
    if (y < 0 || y >= H) return nullptr;
    static const char* const ART[][H] = {
        {"...#.", "..#.."},   // Acute
        {".#...", "..#.."},   // Grave
        {"..#..", ".#.#."},   // Circumflex
        {".##.#", "#..#."},   // Tilde
        {".#.#.", "....."},   // Diaeresis
        {"..#..", ".##.."},   // Cedilla (below)
        {".###.", ".#.#."},   // Ring
    };
    switch (m) {
        case txt::Mark::Acute:      return ART[0][y];
        case txt::Mark::Grave:      return ART[1][y];
        case txt::Mark::Circumflex: return ART[2][y];
        case txt::Mark::Tilde:      return ART[3][y];
        case txt::Mark::Diaeresis:  return ART[4][y];
        case txt::Mark::Cedilla:    return ART[5][y];
        case txt::Mark::Ring:       return ART[6][y];
        case txt::Mark::None:       return nullptr;
    }
    return nullptr;
}

int topRow(txt::Mark m, char base) {
    if (m == txt::Mark::Cedilla) return 13;
    return isupper((unsigned char)base) ? 0 : 3;
}

}
```

- [ ] **Step 8: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_accents\test_accents.cpp src\ui\accents.cpp`
Expected: `3 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 9: `textfit` reports where line b starts and how much of it is real text**

Add to `test/test_textfit/test_textfit.cpp` (register with `RUN_TEST`):

```cpp
void test_offsets_for_marks() {
    textfit::TwoLines t = textfit::wrapTwo("and I said hey what is going on", 120, w6, nullptr);
    TEST_ASSERT_EQUAL_INT(20, (int)t.bStart);              // "is going on" starts at index 20
    TEST_ASSERT_EQUAL_INT((int)t.b.size(), (int)t.bKeep);  // no ellipsis
    textfit::TwoLines e = textfit::wrapTwo(
        "one two three four five six seven eight nine ten eleven", 60, w6, nullptr);
    TEST_ASSERT_EQUAL_INT(8, (int)e.bStart);               // after "one two "
    TEST_ASSERT_EQUAL_INT((int)e.b.size() - 3, (int)e.bKeep);
    textfit::TwoLines s = textfit::wrapTwo("short", 60, w6, nullptr);
    TEST_ASSERT_EQUAL_INT(0, (int)s.bKeep);
}
```

Run `ntest.ps1 test\test_textfit\test_textfit.cpp src\util\textfit.cpp` → `COMPILE FAILED` (no `bStart`).
Then in `textfit.h` change the struct to
`struct TwoLines { std::string a, b; size_t bStart = 0, bKeep = 0; };`
and in `textfit.cpp` replace `ellipsize` and the final `return` of `wrapTwo`:

```cpp
// Fits s into maxW, appending "..." when cut; *keep = number of chars kept from s.
static std::string ellipsize(std::string s, int maxW, WidthFn width, void* ctx, size_t* keep) {
    if (width(s, ctx) <= maxW) { *keep = s.size(); return s; }
    while (!s.empty() && width(s + "...", ctx) > maxW) s.pop_back();
    *keep = s.size();
    return s + "...";
}
```

```cpp
    TwoLines out;
    out.a = s.substr(0, cut);
    out.bStart = next;
    out.b = ellipsize(s.substr(next), maxW, width, ctx, &out.bKeep);
    return out;
```

(the single-line early return stays `return {s, ""};`, which leaves `bStart`/`bKeep` at 0).
Run again → `6 Tests 0 Failures 0 Ignored` / `OK`.

- [ ] **Step 10: Create `src/ui/textdraw.h` / `textdraw.cpp`**

```cpp
// textdraw.h
#pragma once
#include <TFT_eSPI.h>
#include <stddef.h>
#include "../util/text.h"
// Text with Portuguese / Latin-1 accents. The fonts are ASCII-only, so each character is
// drawn as its ASCII base and the accent is added as pixel art (ui/accents) — same width,
// so fitting and wrapping are unchanged. Marks are drawn for font 2 only.
namespace ui {
// UTF-8 text, optionally upper-cased, truncated with "..." to maxW; 1 px shadow like
// shadowText. Datums: TL, TC, TR, ML, MC. Returns the drawn width.
int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint8_t font, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper = false);
// Already-folded text plus one mark per character (marks beyond nMarks count as None).
int drawFolded(TFT_eSPI& t, const char* text, const txt::Mark* marks, size_t nMarks, int x,
               int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum);
}
```

```cpp
// textdraw.cpp
#include "textdraw.h"
#include <ctype.h>
#include <string.h>
#include "accents.h"
#include "battle.h"

namespace ui {

static void drawMark(TFT_eSPI& t, txt::Mark m, char base, int cx, int top, uint16_t col) {
    int r0 = top + accents::topRow(m, base);
    for (int y = 0; y < accents::H; ++y) {
        const char* r = accents::row(m, y);
        if (!r) return;
        for (int x = 0; x < accents::W; ++x)
            if (r[x] == '#') t.drawPixel(cx - accents::W / 2 + x, r0 + y, col);
    }
}

int drawFolded(TFT_eSPI& t, const char* text, const txt::Mark* marks, size_t nMarks, int x,
               int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum) {
    int w = t.textWidth(text, font), h = t.fontHeight(font);
    int left = x, top = y;
    switch (datum) {
        case TC_DATUM: left = x - w / 2; break;
        case TR_DATUM: left = x - w; break;
        case ML_DATUM: top = y - h / 2; break;
        case MC_DATUM: left = x - w / 2; top = y - h / 2; break;
        default: break;   // TL_DATUM
    }
    shadowText(t, text, left, top, font, fg, shadow, TL_DATUM);
    if (font != 2 || !marks) return w;
    char one[2] = {0, 0};
    int px = left;
    for (size_t i = 0; text[i]; ++i) {
        one[0] = text[i];
        int cw = t.textWidth(one, font);
        txt::Mark m = i < nMarks ? marks[i] : txt::Mark::None;
        if (m != txt::Mark::None) {
            int cx = px + (cw - 1) / 2;
            drawMark(t, m, text[i], cx + 1, top + 1, shadow);   // shadow first, like the glyph
            drawMark(t, m, text[i], cx, top, fg);
        }
        px += cw;
    }
    return w;
}

int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint8_t font, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper) {
    char buf[128];
    txt::Mark marks[128];
    size_t n = txt::foldMarks(utf8 ? utf8 : "", buf, marks, sizeof(buf));
    if (upper)
        for (size_t i = 0; i < n; ++i) buf[i] = (char)toupper((unsigned char)buf[i]);
    if (t.textWidth(buf, font) > maxW) {   // truncate with "..." (marks follow the kept chars)
        while (n > 1) {
            buf[--n] = '\0';
            char tmp[132];
            snprintf(tmp, sizeof(tmp), "%s...", buf);
            if (t.textWidth(tmp, font) <= maxW) break;
        }
        strncat(buf, "...", sizeof(buf) - strlen(buf) - 1);
    }
    return drawFolded(t, buf, marks, n, x, y, font, fg, shadow, datum);
}

}
```

- [ ] **Step 11: Use it in `src/ui/screen_now.cpp`**

Add `#include "textdraw.h"`. Then:
- `drawTopStrip`: replace the two lines `String name = fitText(...)` / `int nameW = ...` and the following `shadowText(t, name.c_str(), 314, 2, ...)` with
  `int nameW = drawText(t, st.deviceName[0] ? st.deviceName : "device", 314, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TR_DATUM, 110);`
- `drawNow` info box: replace the three `shadowText(t, fitText(...).c_str(), ...)` calls with

```cpp
    drawText(t, st.trackName[0] ? st.trackName : "Track title", INFO_X + 8, INFO_Y + 8, 2,
             theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW, true);
    drawText(t, st.artist[0] ? st.artist : "Artist", INFO_X + 8, INFO_Y + 32, 2,
             theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW);
    char from[96];
    snprintf(from, sizeof(from), "From: %s", st.context[0] ? st.context : "Playlist");
    drawText(t, from, INFO_X + 8, INFO_Y + 58, 2, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW);
```

- status box name: replace `String nm = fitText(...)` + its `shadowText` with
  `int nmW = drawText(t, st.pokeName[0] ? st.pokeName : "Pokemon", 24, 129, 2, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, 130, true);`
  and use `24 + nmW + 6` for the `No.` x position.
- `drawLyricArea`: fold with marks and draw each wrapped line with its marks:

```cpp
void drawLyricArea(TFT_eSPI& t, const char* currentLine) {
    char folded[160];
    txt::Mark marks[160];
    size_t n = txt::foldMarks(currentLine ? currentLine : "", folded, marks, sizeof(folded));
    if (g_lastLyric == folded) return;
    g_lastLyric = folded;

    const int ix = DLG_X + 6, iy = DLG_Y + 5, iw = DLG_W - 12, ih = DLG_H - 10;
    t.fillRect(ix, iy, iw, ih, theme::DLG_FILL);
    if (!n) return;
    drawIcon(t, icons::Icon::Note, ix + 2, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    drawIcon(t, icons::Icon::Note, ix + iw - 14, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    const int textW = iw - 2 * 18;
    textfit::TwoLines l = textfit::wrapTwo(folded, textW, tftWidth2, &t);
    int cx = DLG_X + DLG_W / 2;
    if (l.b.empty()) {
        drawFolded(t, l.a.c_str(), marks, n, cx, DLG_Y + 20, 2, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
    } else {
        drawFolded(t, l.a.c_str(), marks, l.a.size(), cx, DLG_Y + 12, 2, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
        drawFolded(t, l.b.c_str(), marks + l.bStart, l.bKeep, cx, DLG_Y + 29, 2, theme::TEXT,
                   theme::DLG_SHADOW, MC_DATUM);
    }
}
```

- delete the now-unused `fitText` helper.

- [ ] **Step 12: Host suites, build, flash, observe**

All suites (+ `test_accents`; `test_text` and `test_textfit` updated) → `OK`; build + flash.
Play a Portuguese song with accents in the title and lyrics (e.g. something with "Coração",
"não", "você"). Expected: accents and cedilla visible and centred on their letters in the info box
(upper-case title), artist, context and the dialogue-box lyric; no overlap with the line above;
shadow consistent. Adjust `accents::topRow` / the art if a mark looks off on the panel (tests
must stay green).

- [ ] **Step 13: Commit**

```bash
git add src/util/text.h src/util/text.cpp test/test_text/test_text.cpp src/ui/accents.h src/ui/accents.cpp test/test_accents/test_accents.cpp src/util/textfit.h src/util/textfit.cpp test/test_textfit/test_textfit.cpp src/ui/textdraw.h src/ui/textdraw.cpp src/ui/screen_now.cpp
git commit -m "feat(ui): draw Portuguese/Latin-1 accents over the ASCII font"
```
