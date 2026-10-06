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
- Commits: one deliverable per commit, imperative subject, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

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
git commit -m "Add CREDITS (PMDCollab CC BY-NC, PokeAPI, LRCLIB) and as-built note"
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
- Create: `.devtools/gen_dex.py`, `src/pokemon/dex_data.inc` (generated), `src/pokemon/dex.h`, `src/pokemon/dex.cpp`
- Create: `src/pokemon/pick.h`, `src/pokemon/pick.cpp`
- Create: `test/test_dex/test_dex.cpp`
- Delete: `src/pokemon/pokeapi.h`, `src/pokemon/pokeapi.cpp`
- Modify: `src/main.cpp` (pick from dex)

**Interfaces:**
- Produces: `dex::COUNT = 1025`, `const char* dex::name(int n)`, `const char* dex::type(int n)` (lowercase, `""` out of range), `int dex::fromRandom(uint32_t r)` (1..1025), `void dex::spriteUrl(int n, char* out, size_t len)`; `void pick::choose(AppState& st, int n)`.

- [ ] **Step 1: Create `.devtools/gen_dex.py`**

```python
"""Generate src/pokemon/dex_data.inc from PokeAPI CSV data (dev-time; output is committed).

Why: the deck used to call https://pokeapi.co/api/v2/pokemon/<n> on every track change
just to get a name and a type: ~4.2 s of TLS + a multi-hundred-KB JSON body on a board
without PSRAM. 1025 names + primary types fit in ~20 KB of flash instead.
Run:  python .devtools/gen_dex.py
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
    f.write("// Generated by .devtools/gen_dex.py from PokeAPI CSV data. Do not edit.\n")
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

Run: `python .devtools/gen_dex.py`
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
// .devtools/gen_dex.py. PURE, host-tested. Replaced a ~4.2 s PokeAPI request per
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
git add .devtools/gen_dex.py src/pokemon/dex_data.inc src/pokemon/dex.h src/pokemon/dex.cpp src/pokemon/pick.h src/pokemon/pick.cpp test/test_dex/test_dex.cpp src/main.cpp
git rm src/pokemon/pokeapi.h src/pokemon/pokeapi.cpp
git commit -m "Bundle the Pokedex in flash; drop the 4.2 s PokeAPI call per track"
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
git commit -m "Add tested network schedule: track generations and per-track work order"
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
git commit -m "Move networking to a core-0 task; progressive track change via shared mailbox"
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
git commit -m "Prefetch the next Pokemon's walker during the current song"
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
- Regenerate the bundled Pokédex: `python .devtools/gen_dex.py`.

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
git commit -m "Add README with architecture and design/performance history"
```
