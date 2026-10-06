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
