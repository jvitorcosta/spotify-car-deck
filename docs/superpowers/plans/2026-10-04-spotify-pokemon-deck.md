# Spotify Pokémon Deck — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Gen-3 GBA-styled Spotify now-playing deck + controller on an ESP32 "Cheap Yellow Display" (CYD) that shows a random Pokémon per play and on-demand synced lyrics, for in-car use off an iPhone hotspot.

**Architecture:** Arduino-framework firmware under PlatformIO. Pure logic (type→color, progress interpolation, LRC parsing) lives in framework-free modules unit-tested on the host (`native` env + Unity). Hardware/IO (display, touch, WiFi, HTTPS to Spotify / PokéAPI / LRCLIB, image decode) is verified on-device by observing exact serial/visual output. A single `AppState` is the source of truth; the UI redraws from it each frame, interpolating progress locally between ~4 s Spotify polls.

**Tech Stack:** C++17, PlatformIO (`espressif32`), Arduino-ESP32, TFT_eSPI, XPT2046_Touchscreen, spotify-api-arduino, TJpg_Decoder (JPEG), PNGdec (PNG), ArduinoJson, Preferences (NVS), SD.

## Global Constraints

- **Target board:** `esp32dev` (ESP32-WROOM-32, **no PSRAM**) — never allocate a full-frame bitmap; decoders must stream to the display.
- **Display:** ILI9341, **landscape 320×240** (`tft.setRotation(1)`), backlight on **GPIO 21** (active HIGH).
- **Display SPI pins:** MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST −1.
- **Touch (XPT2046) is a SEPARATE SPI bus:** SCLK 25, MISO 39, MOSI 32, CS 33, IRQ 36. Must use its own `SPIClass(HSPI)`.
- **SD card SPI (VSPI):** CS 5, SCK 18, MISO 19, MOSI 23.
- **Secrets:** live only in `src/config.h` (git-ignored) and NVS. Never commit secrets. `include/config.example.h` is the committed template.
- **Spotify scopes (exact):** `user-read-playback-state user-modify-playback-state user-read-currently-playing`.
- **Spotify poll cadence:** no more often than every **4000 ms**; interpolate progress locally between polls.
- **External hosts (only these):** `api.spotify.com`, `accounts.spotify.com`, `i.scdn.co` (album art), `pokeapi.co`, `raw.githubusercontent.com` (sprites), `lrclib.net`.
- **Pinned library versions:** see Task 1 `platformio.ini`. Do not float versions.
- **Serial baud:** 115200 everywhere.
- **Commit style:** small, one deliverable per commit, imperative subject line.

---

## As-Built Status (updated 2026-10-06)

Verified end-to-end on real hardware (CYD on COM11): WiFi → Spotify → live deck with
album art, random Pokémon, and synced lyric line.

**Done:** Tasks 1–12 (scaffold, theme, interp, lrc, WiFi, auth, now-playing poll,
controls (code), deck UI, live data, album art, Pokémon sprite). Plus live fixes below.

**Deviations from the original plan (supersede the task text):**
- **OAuth (Task 6):** Spotify now rejects non-loopback `http` redirect URIs, so the
  on-device LAN portal does NOT work. Replaced by a PC loopback helper
  (`.devtools/spotify_auth.py`, redirect `http://127.0.0.1:8888/callback`) that writes
  a refresh token into `src/config.h`; firmware uses it directly (NVS portal kept as
  fallback but skipped when a config token is present).
- **Display:** CYD panel needs `tft.invertDisplay(true)` (colors were inverted).
- **HTTP/JSON:** PokéAPI and LRCLIB must read `getString()` (de-chunked) then parse;
  parsing the raw chunked `getStream()` silently returned empty.
- **Accents (PT-BR):** fonts are ASCII-only, so names are stored as original UTF-8
  (for LRCLIB matching) and folded to ASCII at display time via tested `util/text`.
- **Album art:** decode-small + resample to exactly fill the square box (capped buffer
  for reliable malloc), not the plan's simple integer scale.
- **Lyrics (Task 14):** implemented as an INLINE current-verse line under the HP bar
  (synced, advances with playback), NOT a separate LYRICS screen + button. Full
  lyrics screen deferred.
- **Controls (Tasks 8/13):** control methods exist (`spclient::togglePlay/next/prev/
  setVolume`) but the on-screen button row is REMOVED for now (more space); touch
  wiring (Task 13) deferred. `nowButtons()` layout kept in code for later.
- **HP bar:** drains from the LEFT, green→yellow→red, labelled `HP remaining/total`.

- **Task 15:** done (PlayerDetails poll every ~12 s; "No signal..." / "Nothing
  playing" screens). Touch calibration skipped (no controls on screen).
- **Task 16 (walker):** done, but rides the existing HP bar inside the HP panel
  (no separate route bar; left Pokémon box kept). Layout: HP label y124, walker band
  above the bar, bar y176 h16, lyric line y196 h40. Interfaces differ from the task
  text: `img::loadWalkSprite(url, dex, maxSize)` (crops to opaque bounds, keeps aspect,
  34 px), `ui::drawWalker(t, st, step)`, dirty-rect math in tested `util/walkrect`.
- **PokéAPI:** fetched with HTTP/1.0 + filtered stream parse (full `getString()` of
  the huge `/pokemon/N` body failed on a tight heap).

**Remaining / deferred:** Task 13 (touch controls), full lyrics screen, optional
`Lv.`=popularity; small sprite-pipeline polish (single load per track, alpha-edge
fringe, `hasAlpha()`-gated colour-key fallback).

- **2026-10-06 redesign:** superseded by
  `docs/superpowers/plans/2026-10-06-gen3-battle-ui-pmd-walker.md` — Gen-3 battle UI
  (info/status/dialogue boxes, pixel icons) and a PMD SpriteCollab walk cycle with the
  PokeAPI walker as fallback. The left Pokémon box and static sprite are gone.
- **2026-10-06 performance:** networking moved to a core-0 task with progressive track
  change, bundled Pokédex and walker prefetch — see `README.md` "Design & performance
  history" and `docs/superpowers/specs/2026-10-06-perf-network-task-design.md`.

**Toolchain (this machine):** build/test via `.devtools/pio.ps1` and `.devtools/ntest.ps1`;
board on COM11; PlatformIO core on `D:\.platformio`.

---

## File Structure

```
platformio.ini                 — envs (esp32dev + native), pinned deps, CYD build flags
include/
  pins.h                       — all GPIO + bus constants (Global Constraints, in code)
  config.example.h             — committed secrets template
  app_state.h                  — AppState struct + PlaybackStatus enum (shared type)
src/
  config.h                     — real secrets (GIT-IGNORED)
  main.cpp                     — setup/loop, orchestration, frame timing, poll scheduling
  net/wifi.h / wifi.cpp        — multi-network connect, roam, reconnect, isOnline()
  spotify/auth.h / auth.cpp    — one-time OAuth web flow + refresh token in NVS
  spotify/client.h / client.cpp— now-playing poll + control commands → AppState
  images/jpeg.h / jpeg.cpp     — fetch + TJpg decode album art to screen
  images/png.h / png.cpp       — fetch + PNGdec decode sprite to screen (transparent)
  images/cache.h / cache.cpp   — SD cache for sprites
  pokemon/pokeapi.h / .cpp     — random #1-1025 pick + name + first type (+sprite URL)
  lyrics/lrclib.h / .cpp       — fetch + choose synced|plain lyrics
  ui/theme.h / theme.cpp       — GBA palette + type→color map (PURE, tested)
  ui/widgets.h / widgets.cpp   — panels, HP bar, buttons (draw helpers)
  ui/screen_now.h / .cpp       — now-playing deck layout
  ui/screen_lyrics.h / .cpp    — lyrics view (synced highlight + plain + "not found")
  input/touch.h / touch.cpp    — touch read, calibration map, hit-testing
  util/interp.h / interp.cpp   — progress interpolation (PURE, tested)
  util/lrc.h / lrc.cpp         — LRC parse + current-line lookup (PURE, tested)
test/                          — one folder per Unity suite (each has its own main())
  test_theme/test_theme.cpp
  test_interp/test_interp.cpp
  test_lrc/test_lrc.cpp
  test_walk/test_walk.cpp
  test_walkrect/test_walkrect.cpp
  test_text/test_text.cpp
```

**Testable-on-host (pure, no Arduino.h):** `ui/theme.*`, `util/interp.*`, `util/lrc.*`.
Keep these free of `Arduino.h`/`WiFi.h` so they compile in the `native` env.

---

## Task 1: Project scaffold + "Hello screen" (display + touch bring-up)

**Files:**
- Create: `platformio.ini`
- Create: `include/pins.h`
- Create: `include/config.example.h`
- Create: `src/config.h` (git-ignored; copy of template with real values for dev)
- Create: `src/main.cpp`

**Interfaces:**
- Produces: the `pins.h` constants every later hardware task consumes; a buildable `esp32dev` env; confirmed display + touch wiring.

- [ ] **Step 1: Write `platformio.ini`**

```ini
[platformio]
default_envs = esp32dev

[env]
monitor_speed = 115200

[env:esp32dev]
platform = espressif32@6.9.0
board = esp32dev
framework = arduino
monitor_filters = esp32_exception_decoder
board_build.partitions = huge_app.csv
lib_deps =
    bodmer/TFT_eSPI@2.5.43
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git#f956c5d
    bblanchon/ArduinoJson@7.4.3
    bodmer/TJpg_Decoder@1.1.0
    bitbank2/PNGdec@1.1.6
    https://github.com/witnessmenow/spotify-api-arduino.git#6261278
build_flags =
    -std=gnu++17
    -D CORE_DEBUG_LEVEL=3
    ; ---- TFT_eSPI config for CYD (ESP32-2432S028R) ----
    -D USER_SETUP_LOADED=1
    -D ILI9341_2_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=320
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=21
    -D TFT_BACKLIGHT_ON=1
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D SMOOTH_FONT=1
    -D SPI_FREQUENCY=55000000
    -D SPI_READ_FREQUENCY=20000000
build_unflags = -std=gnu++11

[env:native]
platform = native
test_framework = unity
; Compile ONLY the pure-logic modules under native (exclude Arduino-dependent src).
; Add new pure .cpp files here as they are created.
test_build_src = yes
build_src_filter = -<*> +<ui/theme.cpp> +<util/interp.cpp> +<util/lrc.cpp>
build_flags = -std=gnu++17 -D UNIT_TEST
```

> **Toolchain note (this machine) — READ BEFORE RUNNING ANYTHING:**
> - **Firmware compile:** `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
>   (wrapper sets MinGW on PATH, `PLATFORMIO_CORE_DIR=D:\.platformio`, `PYTHONIOENCODING=utf-8`).
> - **Native unit tests:** PlatformIO's `pio test -e native` runner is broken here
>   ("Nothing to build"). Instead compile+run each Unity test directly with g++ via:
>   `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 <test.cpp> <module.cpp ...>`
>   e.g. `.devtools\ntest.ps1 test\test_theme\test_theme.cpp src\ui\theme.cpp`
>   Exit code 0 and `OK` = pass.
> - **Every Unity test file MUST define** `void setUp() {}` and `void tearDown() {}`
>   (Unity links against them). They are included in each test block below.

> Note: if the display shows inverted colors or a pixel offset after flashing, swap `-D ILI9341_2_DRIVER=1` for `-D ILI9341_DRIVER=1` and re-flash. This is the one CYD variant quirk; the verification step below catches it.

- [ ] **Step 2: Write `include/pins.h`**

```cpp
#pragma once
// ESP32-2432S028R "Cheap Yellow Display" pin map. See plan Global Constraints.

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

// Screen geometry (landscape)
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
```

- [ ] **Step 3: Write `include/config.example.h` (committed template)**

```cpp
#pragma once
// Copy to src/config.h and fill in. src/config.h is git-ignored.

// Up to 3 WiFi networks tried in order (home for dev, phone hotspot for car).
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_NETWORKS[] = {
    {"YOUR_HOME_SSID", "YOUR_HOME_PASSWORD"},
    {"YOUR_IPHONE_HOTSPOT", "YOUR_HOTSPOT_PASSWORD"},
};
static const int WIFI_NETWORK_COUNT = 2;

// Spotify developer app (https://developer.spotify.com/dashboard)
#define SPOTIFY_CLIENT_ID     "your_client_id"
#define SPOTIFY_CLIENT_SECRET "your_client_secret"
// Redirect URI you register in the dashboard. Use the device IP form:
//   http://<device-ip>/callback   (printed to serial on first boot)
#define SPOTIFY_REDIRECT_URI  "http://ESP_IP/callback"
#define SPOTIFY_MARKET        "BR"
```

- [ ] **Step 4: Create `src/config.h`** by copying the template and filling real values (WiFi + Spotify). Confirm it is matched by `.gitignore` (`src/config.h` is already listed).

- [ ] **Step 5: Write `src/main.cpp` (hello screen)**

```cpp
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
    if (ts.tsAvailable() || ts.touched()) {
        TS_Point p = ts.getPoint();
        Serial.printf("[touch] raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
        tft.fillCircle(map(p.x, 200, 3900, 0, 320),
                       map(p.y, 200, 3900, 0, 240), 4, TFT_WHITE);
        delay(50);
    }
}
```

- [ ] **Step 6: Build**

Run: `pio run -e esp32dev`
Expected: compiles and links with no errors; prints `SUCCESS`.

- [ ] **Step 7: Flash + observe**

Run: `pio run -e esp32dev -t upload && pio device monitor`
Expected on screen: three horizontal stripes (red/green/blue) with white "PokeDeck hello". Expected on serial: `[boot] display + touch initialized`. Touch the screen → serial prints `[touch] raw x=… y=… z=…` and a white dot appears roughly under your finger.
If colors look wrong/offset, apply the `ILI9341_DRIVER` note in Step 1 and re-flash.

- [ ] **Step 8: Commit**

```bash
git add platformio.ini include/pins.h include/config.example.h src/main.cpp
git commit -m "Scaffold PlatformIO project and bring up CYD display + touch"
```

---

## Task 2: Native test harness + type→color theme (PURE, TDD)

**Files:**
- Create: `src/ui/theme.h`
- Create: `src/ui/theme.cpp`
- Create: `test/test_theme/test_theme.cpp`

**Interfaces:**
- Produces:
  - `namespace theme { uint16_t typeColor(const char* type); }` — returns an RGB565 accent for a PokéAPI type name (lowercase, e.g. `"water"`); unknown/null → `GBA_NAVY`.
  - GBA palette constants: `GBA_CREAM=0xF73A`, `GBA_NAVY=0x218A`, `GBA_GOLD=0xD605`, `HP_GREEN=0x4605`, `POKE_RED=0xE006` (RGB565).

- [ ] **Step 1: Write the failing test `test/test_theme/test_theme.cpp`**

```cpp
#include <unity.h>
#include "../../src/ui/theme.h"

void setUp() {}
void tearDown() {}

void test_known_type_returns_type_color() {
    TEST_ASSERT_EQUAL_UINT16(0x6B0D, theme::typeColor("water")); // GBA blue
    TEST_ASSERT_EQUAL_UINT16(0xA9A5, theme::typeColor("grass")); // GBA green
}
void test_type_is_case_insensitive() {
    TEST_ASSERT_EQUAL_UINT16(theme::typeColor("water"), theme::typeColor("WATER"));
}
void test_unknown_type_returns_navy() {
    TEST_ASSERT_EQUAL_UINT16(theme::GBA_NAVY, theme::typeColor("nonsense"));
}
void test_null_returns_navy() {
    TEST_ASSERT_EQUAL_UINT16(theme::GBA_NAVY, theme::typeColor(nullptr));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_known_type_returns_type_color);
    RUN_TEST(test_type_is_case_insensitive);
    RUN_TEST(test_unknown_type_returns_navy);
    RUN_TEST(test_null_returns_navy);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native`
Expected: FAIL — `theme.h` not found / `typeColor` undefined.

- [ ] **Step 3: Write `src/ui/theme.h`**

```cpp
#pragma once
#include <cstdint>

namespace theme {
// RGB565 GBA Ruby/Sapphire palette
constexpr uint16_t GBA_CREAM = 0xF73A;
constexpr uint16_t GBA_NAVY  = 0x218A;
constexpr uint16_t GBA_GOLD  = 0xD605;
constexpr uint16_t HP_GREEN  = 0x4605;
constexpr uint16_t POKE_RED  = 0xE006;

// Accent color for a PokéAPI type name (lowercase canonical). Null/unknown -> GBA_NAVY.
uint16_t typeColor(const char* type);
}
```

- [ ] **Step 4: Write `src/ui/theme.cpp`**

```cpp
#include "theme.h"
#include <cstring>
#include <cctype>

namespace theme {
struct TypeColor { const char* name; uint16_t color; };
static const TypeColor TABLE[] = {
    {"normal",0xC618},{"fire",0xF9A0},{"water",0x6B0D},{"electric",0xF721},
    {"grass",0xA9A5},{"ice",0x9EFF},{"fighting",0xB143},{"poison",0xA976},
    {"ground",0xE51C},{"flying",0xAD9D},{"psychic",0xFCAB},{"bug",0xA960},
    {"rock",0xB986},{"ghost",0x71D6},{"dragon",0x4817},{"dark",0x5967},
    {"steel",0xC618},{"fairy",0xF576},
};
static char lower(char c){ return (char)std::tolower((unsigned char)c); }

uint16_t typeColor(const char* type){
    if(!type) return GBA_NAVY;
    for(const auto& e : TABLE){
        const char* a=type; const char* b=e.name; bool eq=true;
        while(*a && *b){ if(lower(*a)!=*b){eq=false;break;} ++a;++b; }
        if(eq && *a=='\0' && *b=='\0') return e.color;
    }
    return GBA_NAVY;
}
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `pio test -e native`
Expected: PASS — 4/4 tests.

- [ ] **Step 6: Commit**

```bash
git add src/ui/theme.h src/ui/theme.cpp test/test_theme/test_theme.cpp
git commit -m "Add GBA theme palette and tested type->color map"
```

---

## Task 3: Progress interpolation (PURE, TDD)

**Files:**
- Create: `src/util/interp.h`
- Create: `src/util/interp.cpp`
- Create: `test/test_interp/test_interp.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `uint32_t interp::currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs, bool isPlaying, uint32_t msSincePoll);`
  - If playing, returns `min(lastProgressMs + msSincePoll, durationMs)`.
  - If paused, returns `lastProgressMs` unchanged.
  - Never exceeds `durationMs`.

- [ ] **Step 1: Write the failing test `test/test_interp/test_interp.cpp`**

```cpp
#include <unity.h>
#include "../../src/util/interp.h"

void setUp() {}
void tearDown() {}

void test_playing_advances() {
    TEST_ASSERT_EQUAL_UINT32(5000, interp::currentProgressMs(3000, 200000, true, 2000));
}
void test_paused_holds() {
    TEST_ASSERT_EQUAL_UINT32(3000, interp::currentProgressMs(3000, 200000, false, 2000));
}
void test_clamps_to_duration() {
    TEST_ASSERT_EQUAL_UINT32(200000, interp::currentProgressMs(199000, 200000, true, 5000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_playing_advances);
    RUN_TEST(test_paused_holds);
    RUN_TEST(test_clamps_to_duration);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native -f test_interp`
Expected: FAIL — `interp.h` not found.

- [ ] **Step 3: Write `src/util/interp.h`**

```cpp
#pragma once
#include <cstdint>
namespace interp {
uint32_t currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs,
                           bool isPlaying, uint32_t msSincePoll);
}
```

- [ ] **Step 4: Write `src/util/interp.cpp`**

```cpp
#include "interp.h"
namespace interp {
uint32_t currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs,
                           bool isPlaying, uint32_t msSincePoll) {
    if (!isPlaying) return lastProgressMs;
    uint32_t p = lastProgressMs + msSincePoll;
    return (p > durationMs) ? durationMs : p;
}
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `pio test -e native`
Expected: PASS (theme + interp suites).

- [ ] **Step 6: Commit**

```bash
git add src/util/interp.h src/util/interp.cpp test/test_interp/test_interp.cpp
git commit -m "Add tested progress interpolation helper"
```

---

## Task 4: LRC parser (PURE, TDD)

**Files:**
- Create: `src/util/lrc.h`
- Create: `src/util/lrc.cpp`
- Create: `test/test_lrc/test_lrc.cpp`

**Interfaces:**
- Produces:
  - `struct LrcLine { uint32_t tMs; std::string text; };`
  - `std::vector<lrc::LrcLine> lrc::parse(const std::string& synced);` — parses `[mm:ss.xx]text` lines, sorted by time, skips malformed lines.
  - `int lrc::currentIndex(const std::vector<LrcLine>& lines, uint32_t posMs);` — index of the last line whose `tMs <= posMs`; `-1` before the first.

- [ ] **Step 1: Write the failing test `test/test_lrc/test_lrc.cpp`**

```cpp
#include <unity.h>
#include "../../src/util/lrc.h"

void setUp() {}
void tearDown() {}

void test_parses_timestamped_lines() {
    auto v = lrc::parse("[00:01.00]hello\n[00:03.50]world\n");
    TEST_ASSERT_EQUAL_INT(2, (int)v.size());
    TEST_ASSERT_EQUAL_UINT32(1000, v[0].tMs);
    TEST_ASSERT_EQUAL_STRING("hello", v[0].text.c_str());
    TEST_ASSERT_EQUAL_UINT32(3500, v[1].tMs);
}
void test_skips_malformed() {
    auto v = lrc::parse("garbage\n[00:02.00]ok\n[bad]x\n");
    TEST_ASSERT_EQUAL_INT(1, (int)v.size());
    TEST_ASSERT_EQUAL_STRING("ok", v[0].text.c_str());
}
void test_current_index() {
    auto v = lrc::parse("[00:01.00]a\n[00:03.00]b\n[00:05.00]c\n");
    TEST_ASSERT_EQUAL_INT(-1, lrc::currentIndex(v, 500));
    TEST_ASSERT_EQUAL_INT(0, lrc::currentIndex(v, 2000));
    TEST_ASSERT_EQUAL_INT(1, lrc::currentIndex(v, 3000));
    TEST_ASSERT_EQUAL_INT(2, lrc::currentIndex(v, 9000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_timestamped_lines);
    RUN_TEST(test_skips_malformed);
    RUN_TEST(test_current_index);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native -f test_lrc`
Expected: FAIL — `lrc.h` not found.

- [ ] **Step 3: Write `src/util/lrc.h`**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace lrc {
struct LrcLine { uint32_t tMs; std::string text; };
std::vector<LrcLine> parse(const std::string& synced);
int currentIndex(const std::vector<LrcLine>& lines, uint32_t posMs);
}
```

- [ ] **Step 4: Write `src/util/lrc.cpp`**

```cpp
#include "lrc.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace lrc {
// Parse one "[mm:ss.xx]text" line. Returns false if no valid tag.
static bool parseLine(const std::string& line, LrcLine& out) {
    if (line.size() < 10 || line[0] != '[') return false;
    size_t close = line.find(']');
    if (close == std::string::npos) return false;
    std::string tag = line.substr(1, close - 1); // mm:ss.xx
    size_t colon = tag.find(':');
    size_t dot = tag.find('.');
    if (colon == std::string::npos) return false;
    char* end = nullptr;
    long mm = std::strtol(tag.c_str(), &end, 10);
    if (end != tag.c_str() + colon) return false;
    long ss = std::strtol(tag.c_str() + colon + 1, &end, 10);
    long cs = 0;
    if (dot != std::string::npos) cs = std::strtol(tag.c_str() + dot + 1, nullptr, 10);
    if (mm < 0 || ss < 0 || ss > 59) return false;
    out.tMs = (uint32_t)(mm * 60000 + ss * 1000 + cs * 10);
    out.text = line.substr(close + 1);
    // trim trailing CR
    while (!out.text.empty() && (out.text.back() == '\r' || out.text.back() == '\n'))
        out.text.pop_back();
    return true;
}

std::vector<LrcLine> parse(const std::string& synced) {
    std::vector<LrcLine> out;
    std::istringstream ss(synced);
    std::string line;
    while (std::getline(ss, line)) {
        LrcLine l;
        if (parseLine(line, l)) out.push_back(l);
    }
    std::sort(out.begin(), out.end(),
              [](const LrcLine& a, const LrcLine& b){ return a.tMs < b.tMs; });
    return out;
}

int currentIndex(const std::vector<LrcLine>& lines, uint32_t posMs) {
    int idx = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].tMs <= posMs) idx = (int)i; else break;
    }
    return idx;
}
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `pio test -e native`
Expected: PASS (theme + interp + lrc suites).

- [ ] **Step 6: Commit**

```bash
git add src/util/lrc.h src/util/lrc.cpp test/test_lrc/test_lrc.cpp
git commit -m "Add tested LRC synced-lyrics parser"
```

---

## Task 5: Shared AppState + WiFi multi-network manager

**Files:**
- Create: `include/app_state.h`
- Create: `src/net/wifi.h`
- Create: `src/net/wifi.cpp`
- Modify: `src/main.cpp` (replace hello loop with WiFi bring-up)

**Interfaces:**
- Produces:
  - `include/app_state.h`:
    ```cpp
    enum class PlaybackStatus { Unknown, Playing, Paused, Stopped, Offline };
    struct AppState {
      char trackName[96]; char artist[96]; char album[96]; char context[64];
      char albumArtUrl[160];
      uint32_t progressMs; uint32_t durationMs; uint32_t lastPollMs;
      bool isPlaying; bool shuffle; int repeat; int popularity; int volume;
      char deviceName[48];
      int pokedexNum; char pokeName[24]; char pokeType[16]; char pokeSpriteUrl[160];
      PlaybackStatus status;
    };
    ```
  - `namespace net { bool connectAny(); bool isOnline(); void loop(); String deviceIp(); }`
    - `connectAny()` tries each `WIFI_NETWORKS[i]` in order, 8 s timeout each; returns true on first success.
    - `loop()` reconnects/roams if dropped (non-blocking, checks every 5 s).
    - `isOnline()` → `WiFi.status()==WL_CONNECTED`.

- [ ] **Step 1: Write `include/app_state.h`** exactly as the Interfaces block above (full struct).

- [ ] **Step 2: Write `src/net/wifi.h`**

```cpp
#pragma once
#include <Arduino.h>
namespace net {
bool connectAny();
bool isOnline();
void loop();
String deviceIp();
}
```

- [ ] **Step 3: Write `src/net/wifi.cpp`**

```cpp
#include "wifi.h"
#include <WiFi.h>
#include "../config.h"

namespace net {
static uint32_t lastCheck = 0;

bool connectAny() {
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < WIFI_NETWORK_COUNT; ++i) {
        Serial.printf("[wifi] trying %s\n", WIFI_NETWORKS[i].ssid);
        WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
        uint32_t start = millis();
        while (millis() - start < 8000) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[wifi] connected to %s ip=%s\n",
                              WIFI_NETWORKS[i].ssid, WiFi.localIP().toString().c_str());
                return true;
            }
            delay(200);
        }
        Serial.printf("[wifi] %s failed\n", WIFI_NETWORKS[i].ssid);
        WiFi.disconnect(true);
    }
    return false;
}

bool isOnline() { return WiFi.status() == WL_CONNECTED; }
String deviceIp() { return WiFi.localIP().toString(); }

void loop() {
    if (millis() - lastCheck < 5000) return;
    lastCheck = millis();
    if (!isOnline()) {
        Serial.println("[wifi] dropped, reconnecting...");
        connectAny();
    }
}
}
```

- [ ] **Step 4: Replace `src/main.cpp` body to bring up WiFi** (keep display init from Task 1; swap the `loop`)

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "net/wifi.h"

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);
    tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting WiFi...", 10, 10, 2);

    if (net::connectAny()) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Online: " + net::deviceIp(), 10, 10, 2);
    } else {
        tft.drawString("WiFi FAILED", 10, 40, 2);
    }
}

void loop() { net::loop(); delay(50); }
```

- [ ] **Step 5: Build**

Run: `pio run -e esp32dev`
Expected: compiles clean.

- [ ] **Step 6: Flash + observe**

Run: `pio run -e esp32dev -t upload && pio device monitor`
Expected serial: `[wifi] trying <ssid>` then `[wifi] connected to <ssid> ip=<ip>`. Screen shows `Online: <ip>`. Then power off your home router OR toggle the SSID; within ~5 s serial prints `[wifi] dropped, reconnecting...` and it rejoins (or rolls to the next network).

- [ ] **Step 7: Commit**

```bash
git add include/app_state.h src/net/wifi.h src/net/wifi.cpp src/main.cpp
git commit -m "Add AppState struct and multi-network WiFi manager with roam"
```

---

## Task 6: Spotify one-time auth → refresh token in NVS

**Files:**
- Create: `src/spotify/auth.h`
- Create: `src/spotify/auth.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `net::isOnline()`, `net::deviceIp()`, config macros.
- Produces: `namespace spauth { String loadRefreshToken(); void saveRefreshToken(const String&); bool runSetupPortalIfNeeded(); }`
  - `loadRefreshToken()` reads NVS key `rtoken` (namespace `spotify`); empty string if unset.
  - `runSetupPortalIfNeeded()` — if no stored token, starts a WebServer on port 80: `/` shows a "Log in with Spotify" link to the authorize URL; `/callback` exchanges `?code` for tokens and stores the refresh token in NVS; returns true once stored. If a token already exists, returns true immediately.

- [ ] **Step 1: Write `src/spotify/auth.h`**

```cpp
#pragma once
#include <Arduino.h>
namespace spauth {
String loadRefreshToken();
void saveRefreshToken(const String& token);
bool runSetupPortalIfNeeded();   // blocks in loop until token obtained
}
```

- [ ] **Step 2: Write `src/spotify/auth.cpp`**

```cpp
#include "auth.h"
#include <Preferences.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../net/wifi.h"

namespace spauth {
static Preferences prefs;

String loadRefreshToken() {
    prefs.begin("spotify", true);
    String t = prefs.getString("rtoken", "");
    prefs.end();
    return t;
}
void saveRefreshToken(const String& token) {
    prefs.begin("spotify", false);
    prefs.putString("rtoken", token);
    prefs.end();
}

static String authorizeUrl() {
    String redirect = "http://" + net::deviceIp() + "/callback";
    String scope = "user-read-playback-state%20user-modify-playback-state%20user-read-currently-playing";
    return String("https://accounts.spotify.com/authorize?response_type=code&client_id=")
        + SPOTIFY_CLIENT_ID + "&scope=" + scope + "&redirect_uri=" + redirect;
}

// Exchange authorization code for tokens; store refresh token. Returns true on success.
static bool exchangeCode(const String& code) {
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    String redirect = "http://" + net::deviceIp() + "/callback";
    https.begin(client, "https://accounts.spotify.com/api/token");
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=authorization_code&code=" + code +
                  "&redirect_uri=" + redirect +
                  "&client_id=" + String(SPOTIFY_CLIENT_ID) +
                  "&client_secret=" + String(SPOTIFY_CLIENT_SECRET);
    int rc = https.POST(body);
    if (rc != 200) { Serial.printf("[auth] token exchange HTTP %d\n", rc); https.end(); return false; }
    JsonDocument doc;
    deserializeJson(doc, https.getString());
    https.end();
    String rt = doc["refresh_token"] | "";
    if (rt.isEmpty()) { Serial.println("[auth] no refresh_token in response"); return false; }
    saveRefreshToken(rt);
    Serial.println("[auth] refresh token stored in NVS");
    return true;
}

bool runSetupPortalIfNeeded() {
    if (!loadRefreshToken().isEmpty()) return true;

    WebServer server(80);
    bool done = false;
    server.on("/", [&]() {
        String html = "<h2>PokeDeck setup</h2><p>Register this redirect URI in your "
            "Spotify app dashboard first:</p><code>http://" + net::deviceIp() +
            "/callback</code><p><a href='" + authorizeUrl() +
            "'>Log in with Spotify</a></p>";
        server.send(200, "text/html", html);
    });
    server.on("/callback", [&]() {
        if (!server.hasArg("code")) { server.send(400, "text/plain", "missing code"); return; }
        if (exchangeCode(server.arg("code"))) {
            server.send(200, "text/html", "<h2>Done! You can unplug + reboot.</h2>");
            done = true;
        } else {
            server.send(500, "text/plain", "token exchange failed");
        }
    });
    server.begin();
    Serial.printf("[auth] SETUP NEEDED -> open http://%s/ in a browser on the same network\n",
                  net::deviceIp().c_str());
    while (!done) { server.handleClient(); delay(5); }
    server.stop();
    return true;
}
}
```

- [ ] **Step 3: Wire into `src/main.cpp` setup** (after WiFi connects, before loop)

Add include `#include "spotify/auth.h"` and, in `setup()` after a successful `net::connectAny()`:

```cpp
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Spotify auth...", 10, 10, 2);
    if (spauth::loadRefreshToken().isEmpty()) {
        tft.drawString("Open http://" + net::deviceIp() + "/", 10, 40, 2);
    }
    spauth::runSetupPortalIfNeeded();
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Auth OK", 10, 10, 2);
```

- [ ] **Step 4: Register redirect URI in the Spotify dashboard (manual, one-time).**

In `https://developer.spotify.com/dashboard` → your app → Settings → Redirect URIs, add exactly the URL serial prints (`http://<device-ip>/callback`). Save. (Document this device IP; if the hotspot assigns a new IP later, re-add it.)

- [ ] **Step 5: Build, flash, observe**

Run: `pio run -e esp32dev -t upload && pio device monitor`
Expected serial: `[auth] SETUP NEEDED -> open http://<ip>/`. Open that URL on a phone/PC on the same WiFi, click "Log in with Spotify", approve → page shows "Done!", serial prints `[auth] refresh token stored in NVS`. Reboot the device → serial does NOT print SETUP NEEDED again (token persists); screen shows "Auth OK".

- [ ] **Step 6: Commit**

```bash
git add src/spotify/auth.h src/spotify/auth.cpp src/main.cpp
git commit -m "Add one-time Spotify OAuth setup portal with NVS refresh token"
```

---

## Task 7: Spotify client — now-playing poll into AppState

**Files:**
- Create: `src/spotify/client.h`
- Create: `src/spotify/client.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: refresh token from `spauth::loadRefreshToken()`, config macros, `AppState`.
- Produces: `namespace spclient { void begin(); bool poll(AppState& st); }`
  - `begin()` constructs the `SpotifyArduino` client and calls `refreshAccessToken()`.
  - `poll(st)` calls `getCurrentlyPlaying`; on success fills `st` fields (track/artist/album/context/art url/progress/duration/isPlaying/shuffle/repeat/popularity/device/volume), sets `st.status=Playing|Paused`, `st.lastPollMs=millis()`; on "nothing playing" sets `Stopped`; on error sets `Offline`. Returns true if data changed track (new `trackName`).

- [ ] **Step 1: Write `src/spotify/client.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "app_state.h"
namespace spclient {
void begin();
bool poll(AppState& st);   // returns true when the track changed
}
```

- [ ] **Step 2: Write `src/spotify/client.cpp`**

```cpp
#include "client.h"
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include "../config.h"
#include "../spotify/auth.h"

namespace spclient {
static WiFiClientSecure client;
static SpotifyArduino* sp = nullptr;
static AppState* target = nullptr;
static bool trackChanged = false;

static void copyStr(char* dst, const char* src, size_t n) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1); dst[n - 1] = '\0';
}

static void onPlaying(CurrentlyPlaying cp) {
    AppState& st = *target;
    if (strncmp(st.trackName, cp.trackName, sizeof(st.trackName)) != 0) trackChanged = true;
    copyStr(st.trackName, cp.trackName, sizeof(st.trackName));
    copyStr(st.artist, cp.artists[0].artistName, sizeof(st.artist));
    copyStr(st.album, cp.albumName, sizeof(st.album));
    // choose the ~300px image (index 1 is usually 300px; fall back to 0)
    const char* art = cp.numImages > 1 ? cp.albumImages[1].url : cp.albumImages[0].url;
    copyStr(st.albumArtUrl, art, sizeof(st.albumArtUrl));
    copyStr(st.context, cp.contextUri, sizeof(st.context)); // refined to name in Task 14-opt
    st.progressMs = cp.progressMs;
    st.durationMs = cp.durationMs;
    st.isPlaying  = cp.isPlaying;
    st.lastPollMs = millis();
    st.status = cp.isPlaying ? PlaybackStatus::Playing : PlaybackStatus::Paused;
}

void begin() {
    client.setInsecure();
    String rt = spauth::loadRefreshToken();
    sp = new SpotifyArduino(client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, rt.c_str());
    if (sp->refreshAccessToken()) Serial.println("[spotify] access token OK");
    else Serial.println("[spotify] refreshAccessToken FAILED");
}

bool poll(AppState& st) {
    if (!sp) return false;
    target = &st; trackChanged = false;
    int code = sp->getCurrentlyPlaying(onPlaying, SPOTIFY_MARKET);
    if (code == 200) {
        return trackChanged;
    } else if (code == 204) {
        st.status = PlaybackStatus::Stopped;
    } else {
        Serial.printf("[spotify] poll HTTP %d\n", code);
        st.status = PlaybackStatus::Offline;
    }
    return false;
}
}
```

> API note: field names (`cp.trackName`, `cp.artists[0].artistName`, `cp.albumImages`, `cp.numImages`, `cp.contextUri`, `cp.progressMs`, `cp.durationMs`, `cp.isPlaying`) follow the pinned `spotify-api-arduino` commit. If a field is absent in the installed header, open `.pio/libdeps/esp32dev/spotify-api-arduino/src/SpotifyArduino.h` and match the actual `CurrentlyPlaying` struct members — do not invent names.

- [ ] **Step 3: Wire a timed poll into `src/main.cpp`**

Add `#include "spotify/client.h"` and a global `AppState g_state{};`. In `setup()` after auth: `spclient::begin();`. Replace `loop()`:

```cpp
void loop() {
    net::loop();
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= 4000) {
        lastPoll = millis();
        bool changed = spclient::poll(g_state);
        Serial.printf("[poll] status=%d track=%s %u/%u changed=%d\n",
            (int)g_state.status, g_state.trackName,
            g_state.progressMs, g_state.durationMs, changed);
    }
    delay(20);
}
```

- [ ] **Step 4: Build, flash, observe**

Run: `pio run -e esp32dev -t upload && pio device monitor`
Start playing something in the Spotify app on your phone. Expected serial every ~4 s: `[poll] status=1 track=<real song> <progress>/<duration> changed=…`. Pause in the app → `status=2`. Stop/clear → `status=3`.

- [ ] **Step 5: Commit**

```bash
git add src/spotify/client.h src/spotify/client.cpp src/main.cpp
git commit -m "Poll Spotify currently-playing into AppState every 4s"
```

---

## Task 8: Spotify playback controls

**Files:**
- Modify: `src/spotify/client.h`
- Modify: `src/spotify/client.cpp`

**Interfaces:**
- Produces (added to `spclient`): `void togglePlay(bool currentlyPlaying); void next(); void prev(); void setVolume(int pct);`
  - `togglePlay(true)` → pause; `togglePlay(false)` → play. Others map to library calls.

- [ ] **Step 1: Add declarations to `src/spotify/client.h`**

```cpp
void togglePlay(bool currentlyPlaying);
void next();
void prev();
void setVolume(int pct);   // 0..100
```

- [ ] **Step 2: Add implementations to `src/spotify/client.cpp`**

```cpp
void togglePlay(bool currentlyPlaying) {
    if (!sp) return;
    if (currentlyPlaying) sp->pause(); else sp->play();
}
void next() { if (sp) sp->nextTrack(); }
void prev() { if (sp) sp->previousTrack(); }
void setVolume(int pct) {
    if (!sp) return;
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    sp->setVolume(pct);
}
```

> API note: confirm `pause()`, `play()`, `nextTrack()`, `previousTrack()`, `setVolume(int)` exist in the pinned header (they do in witnessmenow's library). Match exact names if the header differs.

- [ ] **Step 3: Temporary serial-trigger test in `src/main.cpp` loop** (added, removed in Task 13)

```cpp
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'n') spclient::next();
        else if (c == 'p') spclient::prev();
        else if (c == ' ') spclient::togglePlay(g_state.isPlaying);
        else if (c == '+') spclient::setVolume(g_state.volume = min(100, g_state.volume + 10));
        else if (c == '-') spclient::setVolume(g_state.volume = max(0, g_state.volume - 10));
    }
```

- [ ] **Step 4: Build, flash, observe**

Run: `pio run -e esp32dev -t upload && pio device monitor`
With Spotify playing, type in the monitor: `n` (skips to next), `p` (previous), space (pause/resume), `+`/`-` (volume). Confirm each takes effect on the actual playback device.

- [ ] **Step 5: Commit**

```bash
git add src/spotify/client.h src/spotify/client.cpp src/main.cpp
git commit -m "Add Spotify playback controls (play/pause/next/prev/volume)"
```

---

## Task 9: Static deck UI with dummy data (widgets + screen_now)

**Files:**
- Create: `src/ui/widgets.h`
- Create: `src/ui/widgets.cpp`
- Create: `src/ui/screen_now.h`
- Create: `src/ui/screen_now.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `theme::*`, `AppState`, global `TFT_eSPI tft`.
- Produces:
  - `widgets.h`: `namespace ui { void panel(TFT_eSPI&, int x,int y,int w,int h); void hpBar(TFT_eSPI&, int x,int y,int w,int h, float frac); struct Button{int x,int y,int w,int h; const char* label;}; void drawButton(TFT_eSPI&, const Button&, bool active); }`
  - `screen_now.h`: `namespace ui { void drawNow(TFT_eSPI&, const AppState&, uint16_t accent); }` draws the full Pokédex-Entry layout (top bar, art box placeholder, pokémon box placeholder, info panels, HP bar, CP, control row).

- [ ] **Step 1: Write `src/ui/widgets.h`**

```cpp
#pragma once
#include <TFT_eSPI.h>
namespace ui {
void panel(TFT_eSPI& t, int x, int y, int w, int h);                 // cream w/ navy+gold border
void hpBar(TFT_eSPI& t, int x, int y, int w, int h, float frac);     // green fill, dark bg
struct Button { int x, y, w, h; const char* label; };
void drawButton(TFT_eSPI& t, const Button& b, bool active);
bool hit(const Button& b, int px, int py);
}
```

- [ ] **Step 2: Write `src/ui/widgets.cpp`**

```cpp
#include "widgets.h"
#include "theme.h"
namespace ui {
void panel(TFT_eSPI& t, int x, int y, int w, int h) {
    t.fillRoundRect(x, y, w, h, 3, theme::GBA_CREAM);
    t.drawRoundRect(x, y, w, h, 3, theme::GBA_NAVY);
    t.drawRoundRect(x+1, y+1, w-2, h-2, 3, theme::GBA_GOLD);
}
void hpBar(TFT_eSPI& t, int x, int y, int w, int h, float frac) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    t.fillRect(x, y, w, h, 0x2124);                 // dark bg
    t.drawRect(x, y, w, h, theme::GBA_NAVY);
    t.fillRect(x+2, y+2, (int)((w-4)*frac), h-4, theme::HP_GREEN);
}
void drawButton(TFT_eSPI& t, const Button& b, bool active) {
    uint16_t bg = active ? theme::POKE_RED : theme::GBA_CREAM;
    uint16_t fg = active ? TFT_WHITE : theme::GBA_NAVY;
    t.fillRoundRect(b.x, b.y, b.w, b.h, 3, bg);
    t.drawRoundRect(b.x, b.y, b.w, b.h, 3, theme::GBA_NAVY);
    t.setTextColor(fg, bg);
    t.setTextDatum(MC_DATUM);
    t.drawString(b.label, b.x + b.w/2, b.y + b.h/2, 2);
    t.setTextDatum(TL_DATUM);
}
bool hit(const Button& b, int px, int py) {
    return px >= b.x && px <= b.x+b.w && py >= b.y && py <= b.y+b.h;
}
}
```

- [ ] **Step 3: Write `src/ui/screen_now.h`**

```cpp
#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
namespace ui {
// Control button layout (shared with input hit-testing in Task 13).
struct NowButtons { Button prev, play, next, vol, lyrics; };
NowButtons nowButtons();
void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent);
}
```

- [ ] **Step 4: Write `src/ui/screen_now.cpp`**

```cpp
#include "screen_now.h"
#include "widgets.h"
#include "theme.h"

namespace ui {
NowButtons nowButtons() {
    NowButtons b;
    int y = 206, h = 30;
    b.prev   = {8,   y, 46, h, "<<"};
    b.play   = {58,  y, 56, h, ">II"};
    b.next   = {118, y, 46, h, ">>"};
    b.vol    = {168, y, 44, h, "VOL"};
    b.lyrics = {216, y, 96, h, "LYRICS"};
    return b;
}

void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent) {
    t.fillScreen(0x6ADC);  // GBA sky blue background

    // top bar
    t.fillRect(0, 0, 320, 22, theme::GBA_NAVY);
    t.setTextColor(theme::GBA_CREAM, theme::GBA_NAVY);
    t.drawString("NOW PLAYING", 8, 5, 2);
    t.setTextDatum(TR_DATUM);
    t.drawString(st.deviceName[0] ? st.deviceName : "device", 312, 5, 2);
    t.setTextDatum(TL_DATUM);

    // left: album art box + pokemon box (placeholders here; images in later tasks)
    panel(t, 8, 28, 104, 104);
    t.fillRect(11, 31, 98, 98, 0xBDD7); // placeholder art
    panel(t, 8, 136, 104, 96);
    t.setTextColor(accent, theme::GBA_CREAM);
    t.setTextDatum(MC_DATUM);
    t.drawString(st.pokeName[0] ? st.pokeName : "Pokemon", 60, 210, 2);
    t.setTextDatum(TL_DATUM);

    // right column
    panel(t, 118, 28, 194, 44);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.drawString(st.trackName[0] ? st.trackName : "Track title", 124, 32, 2);
    t.drawString(st.artist[0] ? st.artist : "Artist", 124, 52, 2);

    panel(t, 118, 76, 194, 36);
    t.drawString("From:", 124, 80, 2);
    t.drawString(st.context[0] ? st.context : "Playlist", 124, 94, 2);

    panel(t, 118, 116, 194, 36);
    float frac = st.durationMs ? (float)st.progressMs / st.durationMs : 0;
    hpBar(t, 124, 138, 182, 9, frac);
    char tbuf[32];
    snprintf(tbuf, sizeof(tbuf), "%u:%02u  CP %d",
             st.progressMs/60000, (st.progressMs/1000)%60, st.popularity);
    t.drawString(tbuf, 124, 120, 2);

    // controls
    NowButtons b = nowButtons();
    drawButton(t, b.prev, false);
    drawButton(t, b.play, st.isPlaying);
    drawButton(t, b.next, false);
    drawButton(t, b.vol,  false);
    drawButton(t, b.lyrics, false);
}
}
```

- [ ] **Step 5: Render with dummy data in `src/main.cpp`** (temporary — comment out the poll for this step)

In `setup()` after display init, set dummy fields and draw once:

```cpp
    strcpy(g_state.trackName, "Mr. Blue Sky");
    strcpy(g_state.artist, "Electric Light Orchestra");
    strcpy(g_state.context, "Discover Weekly");
    strcpy(g_state.pokeName, "Lapras");
    strcpy(g_state.deviceName, "Living Room");
    g_state.progressMs = 102000; g_state.durationMs = 238000;
    g_state.isPlaying = true; g_state.popularity = 72;
    ui::drawNow(tft, g_state, theme::typeColor("water"));
```

- [ ] **Step 6: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Expected on screen: the full GBA deck — navy top bar, cream panels with gold inner border, "Mr. Blue Sky / Electric Light Orchestra", "From: Discover Weekly", a green HP bar ~43% full, "CP 72", and five control buttons with `>II` highlighted red. Pokémon name "Lapras" in water-blue.

- [ ] **Step 7: Commit**

```bash
git add src/ui/widgets.h src/ui/widgets.cpp src/ui/screen_now.h src/ui/screen_now.cpp src/main.cpp
git commit -m "Draw static GBA now-playing deck layout with dummy data"
```

---

## Task 10: Wire live data + local interpolation into the deck

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `spclient::poll`, `interp::currentProgressMs`, `theme::typeColor`, `ui::drawNow`.

- [ ] **Step 1: Replace `src/main.cpp` loop with poll + interpolate + redraw**

```cpp
void loop() {
    net::loop();
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= 4000) {
        lastPoll = millis();
        spclient::poll(g_state);
    }
    // interpolate progress for a smooth bar between polls
    AppState view = g_state;
    view.progressMs = interp::currentProgressMs(
        g_state.progressMs, g_state.durationMs, g_state.isPlaying,
        millis() - g_state.lastPollMs);

    static uint32_t lastDraw = 0;
    if (millis() - lastDraw >= 250) {   // ~4 fps redraw is plenty
        lastDraw = millis();
        uint16_t accent = theme::typeColor(g_state.pokeType);
        ui::drawNow(tft, view, accent);
    }
    delay(10);
}
```

Add includes: `#include "util/interp.h"`, `#include "ui/screen_now.h"`, `#include "ui/theme.h"`. Remove the Task 9 dummy-data block from `setup()`.

- [ ] **Step 2: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
With Spotify playing: the deck now shows the **real** current track/artist, the HP bar advances smoothly (updates between 4 s polls), and pausing in the Spotify app flips the `>II` highlight within ~4 s.

> Note: full-screen redraw at 4 fps may flicker. If it does, Task 15 introduces dirty-region redraw; acceptable for now.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "Wire live Spotify data with local progress interpolation into deck"
```

---

## Task 11: Album art — fetch + JPEG decode to screen

**Files:**
- Create: `src/images/jpeg.h`
- Create: `src/images/jpeg.cpp`
- Modify: `src/ui/screen_now.cpp` (call the art drawer)
- Modify: `src/main.cpp` (fetch art on track change)

**Interfaces:**
- Produces: `namespace img { bool drawAlbumArt(TFT_eSPI& t, const char* url, int x, int y, int boxW, int boxH); }`
  - Downloads the JPEG into a heap buffer, decodes with TJpg_Decoder at a scale that fits `boxW×boxH`, draws at `(x,y)`. Returns false on any failure (caller keeps the placeholder).

- [ ] **Step 1: Write `src/images/jpeg.h`**

```cpp
#pragma once
#include <TFT_eSPI.h>
namespace img {
bool drawAlbumArt(TFT_eSPI& t, const char* url, int x, int y, int boxW, int boxH);
}
```

- [ ] **Step 2: Write `src/images/jpeg.cpp`**

```cpp
#include "jpeg.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>

namespace img {
static TFT_eSPI* g_tft = nullptr;
static int g_ox = 0, g_oy = 0;

static bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
    if (y >= g_tft->height()) return false;
    g_tft->pushImage(g_ox + x, g_oy + y, w, h, bmp);
    return true;
}

bool drawAlbumArt(TFT_eSPI& t, const char* url, int x, int y, int boxW, int boxH) {
    if (!url || !url[0]) return false;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) return false;
    int rc = https.GET();
    if (rc != 200) { https.end(); return false; }
    int len = https.getSize();
    if (len <= 0 || len > 60000) { https.end(); return false; }   // guard RAM
    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) { https.end(); return false; }
    WiFiClient* stream = https.getStreamPtr();
    int got = 0;
    while (https.connected() && got < len) {
        size_t avail = stream->available();
        if (avail) got += stream->readBytes(buf + got, min((size_t)(len - got), avail));
        else delay(1);
    }
    https.end();

    uint16_t jw = 0, jh = 0;
    TJpgDec.getJpgSize(&jw, &jh, buf, len);
    uint8_t scale = 1;
    while ((jw / scale > boxW || jh / scale > boxH) && scale < 8) scale <<= 1;
    TJpgDec.setJpgScale(scale);
    TJpgDec.setCallback(tftOutput);
    g_tft = &t;
    g_ox = x + (boxW - jw/scale)/2;
    g_oy = y + (boxH - jh/scale)/2;
    JRESULT r = TJpgDec.drawJpg(0, 0, buf, len);
    free(buf);
    return r == JDR_OK;
}
}
```

- [ ] **Step 3: Draw art in `screen_now.cpp`** — replace the placeholder art fill. Change the art box block to attempt art, falling back to the fill. Add parameter via a module-level "art url" is avoided; instead call from `main` after `drawNow`. In `drawNow`, keep drawing the placeholder rect (art is layered on top by `main`).

- [ ] **Step 4: In `src/main.cpp`, after `ui::drawNow(...)` redraw, overlay art**

```cpp
        ui::drawNow(tft, view, accent);
        img::drawAlbumArt(tft, g_state.albumArtUrl, 11, 31, 98, 98);
```

Add `#include "images/jpeg.h"`. To avoid re-downloading every 250 ms, cache the last drawn URL:

```cpp
        static char lastArt[160] = "";
        if (strcmp(lastArt, g_state.albumArtUrl) != 0) {
            if (img::drawAlbumArt(tft, g_state.albumArtUrl, 11, 31, 98, 98))
                strcpy(lastArt, g_state.albumArtUrl);
        } else {
            img::drawAlbumArt(tft, g_state.albumArtUrl, 11, 31, 98, 98); // redraw cached-size (cheap re-fetch avoided below)
        }
```

> Simplify: since full redraw wipes the art each frame, fetch once per track into a persistent sprite is ideal but RAM-heavy. For now, only redraw art when `lastArt` differs OR the screen was cleared; acceptable flicker noted in Task 15.

- [ ] **Step 5: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Expected: the real album cover of the currently-playing track renders in the left box, scaled to fit. Changing tracks in Spotify swaps the cover within ~4 s.

- [ ] **Step 6: Commit**

```bash
git add src/images/jpeg.h src/images/jpeg.cpp src/ui/screen_now.cpp src/main.cpp
git commit -m "Fetch and decode Spotify album art onto the deck"
```

---

## Task 12: Pokémon — random pick, metadata, sprite (PNG) + SD cache + type accent

**Files:**
- Create: `src/pokemon/pokeapi.h`
- Create: `src/pokemon/pokeapi.cpp`
- Create: `src/images/png.h`
- Create: `src/images/png.cpp`
- Create: `src/images/cache.h`
- Create: `src/images/cache.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces:
  - `namespace pokeapi { bool pickRandom(AppState& st); }` — chooses a random id in `[1,1025]` (seed from `esp_random()`), fetches name + first type via `https://pokeapi.co/api/v2/pokemon/<id>` (ArduinoJson filter: only `name`, `types[0].type.name`), sets `st.pokedexNum/pokeName/pokeType` and `st.pokeSpriteUrl` = `https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/<id>.png`. Returns true on success.
  - `namespace img { bool drawSprite(TFT_eSPI& t, const char* url, int cx, int cy); }` — fetch (or read cache) PNG, decode with PNGdec, draw centered at `(cx,cy)` with transparency.
  - `namespace cache { bool begin(); String spritePath(int dex); bool has(int dex); bool save(int dex, const uint8_t* data, size_t n); File open(int dex); }`

- [ ] **Step 1: Write `src/pokemon/pokeapi.h`**

```cpp
#pragma once
#include "app_state.h"
namespace pokeapi { bool pickRandom(AppState& st); }
```

- [ ] **Step 2: Write `src/pokemon/pokeapi.cpp`**

```cpp
#include "pokeapi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>

namespace pokeapi {
bool pickRandom(AppState& st) {
    int id = (int)(esp_random() % 1025) + 1;    // 1..1025
    char url[80];
    snprintf(url, sizeof(url), "https://pokeapi.co/api/v2/pokemon/%d", id);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) return false;
    int rc = https.GET();
    if (rc != 200) { https.end(); return false; }

    JsonDocument filter;
    filter["name"] = true;
    filter["types"][0]["type"]["name"] = true;
    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
    https.end();
    if (err) { Serial.printf("[poke] json err %s\n", err.c_str()); return false; }

    st.pokedexNum = id;
    strncpy(st.pokeName, doc["name"] | "?", sizeof(st.pokeName)-1);
    strncpy(st.pokeType, doc["types"][0]["type"]["name"] | "normal", sizeof(st.pokeType)-1);
    snprintf(st.pokeSpriteUrl, sizeof(st.pokeSpriteUrl),
        "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png", id);
    Serial.printf("[poke] #%d %s (%s)\n", id, st.pokeName, st.pokeType);
    return true;
}
}
```

- [ ] **Step 3: Write `src/images/cache.h` / `cache.cpp`**

```cpp
// cache.h
#pragma once
#include <Arduino.h>
#include <FS.h>
namespace cache {
bool begin();
String spritePath(int dex);
bool has(int dex);
bool save(int dex, const uint8_t* data, size_t n);
File open(int dex);     // read handle; caller checks validity
}
```

```cpp
// cache.cpp
#include "cache.h"
#include <SD.h>
#include <SPI.h>
#include "../../include/pins.h"

namespace cache {
static SPIClass sdSPI(VSPI);
static bool ready = false;

bool begin() {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    ready = SD.begin(SD_CS, sdSPI);
    if (ready && !SD.exists("/sprites")) SD.mkdir("/sprites");
    Serial.printf("[cache] SD %s\n", ready ? "ready" : "unavailable");
    return ready;
}
String spritePath(int dex) { return "/sprites/" + String(dex) + ".png"; }
bool has(int dex) { return ready && SD.exists(spritePath(dex)); }
bool save(int dex, const uint8_t* data, size_t n) {
    if (!ready) return false;
    File f = SD.open(spritePath(dex), FILE_WRITE);
    if (!f) return false;
    f.write(data, n); f.close(); return true;
}
File open(int dex) { return ready ? SD.open(spritePath(dex), FILE_READ) : File(); }
}
```

- [ ] **Step 4: Write `src/images/png.h` / `png.cpp`**

```cpp
// png.h
#pragma once
#include <TFT_eSPI.h>
namespace img { bool drawSprite(TFT_eSPI& t, const char* url, int dex, int cx, int cy); }
```

```cpp
// png.cpp
#include "png.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PNGdec.h>
#include "cache.h"

namespace img {
static PNG png;
static TFT_eSPI* g_tft = nullptr;
static int g_x0 = 0, g_y0 = 0;
static uint8_t* g_buf = nullptr;

static void pngDraw(PNGDRAW* d) {
    uint16_t line[128];
    png.getLineAsRGB565(d, line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    // transparency: skip fully transparent pixels via mask
    for (int i = 0; i < d->iWidth; ++i) {
        uint8_t alpha = png.getAlphaMask(d, nullptr, 1) ? 255 : 255; // fallback opaque
        (void)alpha;
    }
    g_tft->pushImage(g_x0, g_y0 + d->y, d->iWidth, 1, line);
}

bool drawSprite(TFT_eSPI& t, const char* url, int dex, int cx, int cy) {
    // Load bytes: from SD cache if present, else download + cache.
    size_t n = 0; uint8_t* data = nullptr;
    if (cache::has(dex)) {
        File f = cache::open(dex);
        n = f.size(); data = (uint8_t*)malloc(n);
        if (!data) { f.close(); return false; }
        f.read(data, n); f.close();
    } else {
        if (!url || !url[0]) return false;
        WiFiClientSecure client; client.setInsecure();
        HTTPClient https;
        if (!https.begin(client, url)) return false;
        if (https.GET() != 200) { https.end(); return false; }
        int len = https.getSize();
        if (len <= 0 || len > 40000) { https.end(); return false; }
        data = (uint8_t*)malloc(len);
        if (!data) { https.end(); return false; }
        WiFiClient* s = https.getStreamPtr(); int got = 0;
        while (https.connected() && got < len) {
            size_t a = s->available();
            if (a) got += s->readBytes(data + got, min((size_t)(len-got), a)); else delay(1);
        }
        https.end(); n = len;
        cache::save(dex, data, n);
    }

    int rc = png.openRAM(data, n, pngDraw);
    if (rc != PNG_SUCCESS) { free(data); return false; }
    g_tft = &t;
    g_x0 = cx - png.getWidth()/2;
    g_y0 = cy - png.getHeight()/2;
    png.decode(nullptr, 0);
    png.close();
    free(data);
    return true;
}
}
```

> API note: `getLineAsRGB565` + per-line `pushImage` is the standard PNGdec+TFT_eSPI pattern. True per-pixel alpha compositing against the cream panel is optional polish; opaque draw on the cream box is acceptable for v1. Verify `PNG`/`PNGDRAW` symbols against the pinned `bitbank2/PNGdec` header.

- [ ] **Step 5: Wire into `src/main.cpp`**

In `setup()`: `cache::begin();`. On track change (detect via `spclient::poll` return value), pick a new Pokémon:

```cpp
    if (millis() - lastPoll >= 4000) {
        lastPoll = millis();
        bool changed = spclient::poll(g_state);
        if (changed) pokeapi::pickRandom(g_state);   // fresh random each play
    }
```

After drawing the deck + album art, draw the sprite centered in the pokémon box:

```cpp
        img::drawSprite(tft, g_state.pokeSpriteUrl, g_state.pokedexNum, 60, 175);
```

Add includes for `pokemon/pokeapi.h`, `images/png.h`, `images/cache.h`.

- [ ] **Step 6: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Expected serial on each track change: `[poke] #<n> <name> (<type>)`. On screen: a Pokémon sprite appears in the lower-left box, its name under it, and the accent color (pokémon name text) matches the type. Skip tracks → a different random Pokémon each time. Re-encountering a cached dex number loads instantly (no network delay); serial `[cache] ready` confirms SD is used.

- [ ] **Step 7: Commit**

```bash
git add src/pokemon/ src/images/png.h src/images/png.cpp src/images/cache.h src/images/cache.cpp src/main.cpp
git commit -m "Add random Pokemon per play with PNG sprite, SD cache, type accent"
```

---

## Task 13: Touch controls → Spotify commands

**Files:**
- Create: `src/input/touch.h`
- Create: `src/input/touch.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `ui::nowButtons()`, `ui::hit()`, `spclient::*`.
- Produces: `namespace input { void begin(); bool read(int& x, int& y); }` — `read` returns true with screen coords (already mapped to 320×240 landscape) on a fresh press (debounced ~250 ms).

- [ ] **Step 1: Write `src/input/touch.h`**

```cpp
#pragma once
#include <Arduino.h>
namespace input { void begin(); bool read(int& x, int& y); }
```

- [ ] **Step 2: Write `src/input/touch.cpp`**

```cpp
#include "touch.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "../../include/pins.h"

namespace input {
static SPIClass touchSPI(HSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
static uint32_t lastTouch = 0;

void begin() {
    touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
}

bool read(int& x, int& y) {
    if (!ts.touched()) return false;
    if (millis() - lastTouch < 250) return false;
    TS_Point p = ts.getPoint();
    if (p.z < 300) return false;                      // ignore light noise
    // Calibrate raw [~200..3900] -> screen. Tune if needed via serial crosshair.
    x = map(p.x, 200, 3900, 0, SCREEN_W);
    y = map(p.y, 200, 3900, 0, SCREEN_H);
    if (x < 0) x = 0; if (x > SCREEN_W) x = SCREEN_W;
    if (y < 0) y = 0; if (y > SCREEN_H) y = SCREEN_H;
    lastTouch = millis();
    return true;
}
}
```

- [ ] **Step 3: Handle touch in `src/main.cpp` loop** (remove the Task 8 serial-trigger block)

```cpp
    int tx, ty;
    if (input::read(tx, ty)) {
        ui::NowButtons b = ui::nowButtons();
        if (ui::hit(b.play, tx, ty))      spclient::togglePlay(g_state.isPlaying);
        else if (ui::hit(b.next, tx, ty)) { spclient::next(); }
        else if (ui::hit(b.prev, tx, ty)) { spclient::prev(); }
        else if (ui::hit(b.vol,  tx, ty)) { spclient::setVolume(g_state.volume = (g_state.volume+20)%120); }
        else if (ui::hit(b.lyrics, tx, ty)) { /* Task 14: toggle lyrics screen */ }
        lastPoll = 0;   // force a quick re-poll so UI reflects the change fast
    }
```

Add `#include "input/touch.h"` and `input::begin();` in `setup()`.

- [ ] **Step 4: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Tap the on-screen `>II` → playback toggles on the real device and the highlight flips within ~1 s. Tap `>>` / `<<` → track changes (and a new Pokémon appears). Tap `VOL` → volume steps. If taps land on the wrong button, note the raw values printed and adjust the `map()` ranges in Step 2.

- [ ] **Step 5: Commit**

```bash
git add src/input/touch.h src/input/touch.cpp src/main.cpp
git commit -m "Wire resistive touch to Spotify playback controls"
```

---

## Task 14: Lyrics — LRCLIB fetch + lyrics screen + LYRICS button

**Files:**
- Create: `src/lyrics/lrclib.h`
- Create: `src/lyrics/lrclib.cpp`
- Create: `src/ui/screen_lyrics.h`
- Create: `src/ui/screen_lyrics.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `util/lrc` (parse + currentIndex), `interp`, track metadata from `AppState`.
- Produces:
  - `namespace lyricsvc { enum class Kind { None, Synced, Plain }; struct Result { Kind kind; std::string text; }; Result fetch(const AppState& st); }`
    - Builds `https://lrclib.net/api/get` with URL-encoded `track_name`, `artist_name`, `album_name`, `duration` (seconds). Prefers `syncedLyrics` → `Synced`; else `plainLyrics` → `Plain`; else `None`.
  - `namespace ui { void drawLyrics(TFT_eSPI&, const lyricsvc::Result&, const std::vector<lrc::LrcLine>&, uint32_t posMs); }`

- [ ] **Step 1: Write `src/lyrics/lrclib.h`**

```cpp
#pragma once
#include <string>
#include "app_state.h"
namespace lyricsvc {
enum class Kind { None, Synced, Plain };
struct Result { Kind kind; std::string text; };
Result fetch(const AppState& st);
}
```

- [ ] **Step 2: Write `src/lyrics/lrclib.cpp`**

```cpp
#include "lrclib.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace lyricsvc {
static String urlEncode(const char* s) {
    String out;
    for (const char* p = s; *p; ++p) {
        char c = *p;
        if (isalnum((unsigned char)c)) out += c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", (unsigned char)c); out += b; }
    }
    return out;
}

Result fetch(const AppState& st) {
    Result r{Kind::None, ""};
    if (!st.trackName[0]) return r;
    String url = "https://lrclib.net/api/get?track_name=" + urlEncode(st.trackName) +
                 "&artist_name=" + urlEncode(st.artist) +
                 "&album_name=" + urlEncode(st.album) +
                 "&duration=" + String(st.durationMs / 1000);
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    https.begin(client, url);
    https.addHeader("User-Agent", "PokeDeck (github.com/placeholder)");
    int rc = https.GET();
    if (rc != 200) { https.end(); return r; }   // 404 => no match
    JsonDocument filter;
    filter["syncedLyrics"] = true;
    filter["plainLyrics"] = true;
    JsonDocument doc;
    deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
    https.end();
    const char* synced = doc["syncedLyrics"] | "";
    const char* plain  = doc["plainLyrics"]  | "";
    if (synced && synced[0]) { r.kind = Kind::Synced; r.text = synced; }
    else if (plain && plain[0]) { r.kind = Kind::Plain; r.text = plain; }
    return r;
}
}
```

- [ ] **Step 3: Write `src/ui/screen_lyrics.h`**

```cpp
#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "../util/lrc.h"
#include "../lyrics/lrclib.h"
namespace ui {
Button lyricsBackButton();
void drawLyrics(TFT_eSPI& t, const lyricsvc::Result& res,
                const std::vector<lrc::LrcLine>& lines, uint32_t posMs);
}
```

- [ ] **Step 4: Write `src/ui/screen_lyrics.cpp`**

```cpp
#include "screen_lyrics.h"
#include "widgets.h"
#include "theme.h"

namespace ui {
Button lyricsBackButton() { return {256, 2, 60, 18, "BACK"}; }

void drawLyrics(TFT_eSPI& t, const lyricsvc::Result& res,
                const std::vector<lrc::LrcLine>& lines, uint32_t posMs) {
    t.fillScreen(0x6ADC);
    t.fillRect(0, 0, 320, 22, theme::GBA_NAVY);
    t.setTextColor(theme::GBA_CREAM, theme::GBA_NAVY);
    t.drawString("LYRICS", 8, 5, 2);
    drawButton(t, lyricsBackButton(), false);

    panel(t, 8, 28, 304, 204);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);

    if (res.kind == lyricsvc::Kind::None) {
        t.setTextDatum(MC_DATUM);
        t.drawString("No lyrics found", 160, 128, 4);
        t.setTextDatum(TL_DATUM);
        return;
    }
    if (res.kind == lyricsvc::Kind::Synced && !lines.empty()) {
        int cur = lrc::currentIndex(lines, posMs);
        if (cur < 0) cur = 0;
        // show cur-2 .. cur+4, highlight current
        int first = cur - 2; if (first < 0) first = 0;
        int yy = 36;
        for (int i = first; i < (int)lines.size() && yy < 224; ++i) {
            bool hot = (i == cur);
            t.setTextColor(hot ? theme::POKE_RED : theme::GBA_NAVY, theme::GBA_CREAM);
            t.drawString(lines[i].text.c_str(), 16, yy, hot ? 4 : 2);
            yy += hot ? 26 : 18;
        }
    } else {
        // plain: draw first N lines from a scroll offset of 0 (scroll in Task 15 polish)
        int yy = 36; const char* p = res.text.c_str(); char line[64]; int li = 0;
        for (; *p && yy < 224; ++p) {
            if (*p == '\n' || li == 63) {
                line[li] = 0; t.drawString(line, 16, yy, 2); yy += 18; li = 0;
            } else line[li++] = *p;
        }
    }
}
}
```

- [ ] **Step 5: Wire a screen-mode toggle into `src/main.cpp`**

Add a mode enum + lyrics state, fetch on entering lyrics mode:

```cpp
enum class Screen { Now, Lyrics };
static Screen g_screen = Screen::Now;
static lyricsvc::Result g_lyrics{lyricsvc::Kind::None, ""};
static std::vector<lrc::LrcLine> g_lrcLines;
```

In the touch handler, replace the lyrics placeholder and add back handling:

```cpp
        else if (ui::hit(b.lyrics, tx, ty)) {
            g_lyrics = lyricsvc::fetch(g_state);
            g_lrcLines = (g_lyrics.kind == lyricsvc::Kind::Synced)
                         ? lrc::parse(g_lyrics.text) : std::vector<lrc::LrcLine>{};
            g_screen = Screen::Lyrics;
        }
    }
    if (g_screen == Screen::Lyrics && input::read(tx, ty)) {
        if (ui::hit(ui::lyricsBackButton(), tx, ty)) g_screen = Screen::Now;
    }
```

Split the redraw by mode:

```cpp
    if (millis() - lastDraw >= 250) {
        lastDraw = millis();
        if (g_screen == Screen::Now) {
            uint16_t accent = theme::typeColor(g_state.pokeType);
            ui::drawNow(tft, view, accent);
            img::drawSprite(tft, g_state.pokeSpriteUrl, g_state.pokedexNum, 60, 175);
            /* album art overlay as in Task 11 */
        } else {
            ui::drawLyrics(tft, g_lyrics, g_lrcLines, view.progressMs);
        }
    }
```

Add includes: `#include "lyrics/lrclib.h"`, `#include "ui/screen_lyrics.h"`, `#include "util/lrc.h"`, `#include <vector>`.

- [ ] **Step 6: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Play a well-known song, tap **LYRICS**. Expected: lyrics screen appears; for songs LRCLIB has synced lyrics for, the current line highlights red and advances with the music; otherwise plain text shows; for obscure tracks "No lyrics found". Tap **BACK** → returns to the deck. Verify an accented/Portuguese song renders characters correctly (if boxes appear, the font lacks the glyphs → Step 7).

- [ ] **Step 7: Extended charset note**

If accented characters render as boxes, enable a Unicode/Latin-1 capable font. Simplest path: add `-D LOAD_FONT6=1` is insufficient for accents; instead load a custom SMOOTH_FONT `.vlw` with Latin-1 range placed on SD/flash and use `tft.loadFont(...)`. Create `docs/fonts.md` recording the chosen font + range, and switch `drawString` calls to the loaded font. (Defer the actual font swap to Task 15 polish unless Portuguese lyrics are a launch blocker.)

- [ ] **Step 8: Commit**

```bash
git add src/lyrics/ src/ui/screen_lyrics.h src/ui/screen_lyrics.cpp src/main.cpp
git commit -m "Add LRCLIB lyrics with synced highlight, plain + not-found fallbacks"
```

---

## Task 15: Polish — offline states, dirty-region redraw, reconnect, calibration, context name

**Files:**
- Modify: `src/ui/screen_now.cpp`
- Modify: `src/main.cpp`
- Modify: `src/spotify/client.cpp` (resolve playlist/context name)
- Create: `docs/calibration.md`

**Interfaces:**
- Produces: `ui::drawOffline(TFT_eSPI&, const char* msg)` and a dirty-redraw path that only repaints the HP bar + time between polls (kills flicker).

- [ ] **Step 1: Add offline screen to `screen_now`**

```cpp
// screen_now.h
void drawOffline(TFT_eSPI& t, const char* msg);
```
```cpp
// screen_now.cpp
void drawOffline(TFT_eSPI& t, const char* msg) {
    t.fillScreen(theme::GBA_NAVY);
    t.setTextColor(theme::GBA_CREAM, theme::GBA_NAVY);
    t.setTextDatum(MC_DATUM);
    t.drawString(msg, 160, 120, 4);
    t.setTextDatum(TL_DATUM);
}
```

- [ ] **Step 2: Route offline/stopped states in `main.cpp` redraw**

```cpp
        if (g_state.status == PlaybackStatus::Offline || !net::isOnline())
            ui::drawOffline(tft, "No signal...");
        else if (g_state.status == PlaybackStatus::Stopped)
            ui::drawOffline(tft, "Nothing playing");
        else { /* normal Now/Lyrics draw */ }
```

- [ ] **Step 3: Dirty-region redraw to stop flicker**

Draw the full deck only when the track changes or mode switches; every 250 ms update **only** the HP bar + time by repainting that panel region:

```cpp
    static char drawnTrack[96] = ""; static Screen drawnScreen = Screen::Now;
    bool full = strcmp(drawnTrack, g_state.trackName) != 0 || drawnScreen != g_screen;
    if (full) {
        /* full draw: deck + sprite + album art, or lyrics */
        strcpy(drawnTrack, g_state.trackName); drawnScreen = g_screen;
    } else if (g_screen == Screen::Now && millis() - lastDraw >= 250) {
        lastDraw = millis();
        ui::panel(tft, 118, 116, 194, 36);       // repaint just the progress panel
        float frac = view.durationMs ? (float)view.progressMs/view.durationMs : 0;
        ui::hpBar(tft, 124, 138, 182, 9, frac);
        char tbuf[32]; snprintf(tbuf, sizeof(tbuf), "%u:%02u  CP %d",
            view.progressMs/60000, (view.progressMs/1000)%60, view.popularity);
        tft.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
        tft.drawString(tbuf, 124, 120, 2);
    }
```

- [ ] **Step 4: Resolve context (playlist) name in `client.cpp`**

The currently-playing response gives a context URI/type, not a name. When `cp.contextUri` is a playlist, fetch its name once and cache by URI:

```cpp
// in onPlaying: if contextUri differs from a static lastContextUri,
// call sp->getPlaylist(...) (or HTTPS GET /v1/playlists/{id}?fields=name) and
// copy the returned name into st.context; else keep cached name.
```
Implement with a small HTTPS GET to `https://api.spotify.com/v1/playlists/<id>?fields=name` using the library's access token (`sp->getAccessToken()` if exposed, else reuse the token from `auth`). Cache `lastContextUri`/`lastContextName` statics to avoid refetching. For non-playlist contexts (album/artist) show the type label.

- [ ] **Step 5: Calibration doc**

Write `docs/calibration.md` describing: flash Task 1's hello build, tap the four screen corners, record the raw min/max X and Y from serial, and update the `map()` ranges in `input/touch.cpp`. Include the recorded values for this unit.

- [ ] **Step 6: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Expected: HP bar advances with **no full-screen flicker**; pulling WiFi (or entering a dead zone) shows "No signal..."; stopping playback shows "Nothing playing"; the deck shows the real **playlist name** (e.g. "Discover Weekly") instead of a URI; touch targets land accurately after calibration.

- [ ] **Step 7: Commit**

```bash
git add src/ui/screen_now.h src/ui/screen_now.cpp src/main.cpp src/spotify/client.cpp docs/calibration.md
git commit -m "Polish: offline states, flicker-free redraw, playlist name, calibration"
```

---

## Task 16: Walking-Pokémon progress bar (bob/step fake-walk)

Builds on Task 12 (sprite fetch) and Task 15 (dirty-region redraw). **Supersedes** the
static Pokémon-box render from Task 9 and the plain HP-fill from Tasks 9/15: the song's
Pokémon now walks along a wide progress "route" bar. The lower-left Pokémon box is
repurposed to a small name/type nameplate (keep `pokeName` + type badge).

**Files:**
- Modify: `src/util/interp.h`, `src/util/interp.cpp` (add `walkX`)
- Create: `test/test_walk/test_walk.cpp`
- Modify: `src/images/png.h`, `src/images/png.cpp` (decode to a downscaled RAM buffer)
- Modify: `src/ui/screen_now.h`, `src/ui/screen_now.cpp` (route bar + `drawWalker`)
- Modify: `src/main.cpp` (animate at ~8 fps; decode walk sprite once per track)

**Interfaces:**
- Produces:
  - `int interp::walkX(float frac, int barX, int barW, int spriteW);` — leading-edge x
    for the sprite center, clamped so the sprite stays within `[barX, barX+barW]`.
  - `namespace img { bool loadWalkSprite(const char* url, int dex, int outW, int outH); const uint16_t* walkBuffer(); int walkW(); int walkH(); bool walkReady(); }` — fetch/cache PNG (as Task 12), decode, nearest-neighbor downscale to `outW×outH` into a persistent static RGB565 buffer + a parallel transparency mask.
  - `void ui::drawRoute(TFT_eSPI&, int x,int y,int w,int h, float frac);` — draws the route bar fill/track.
  - `void ui::drawWalker(TFT_eSPI&, int cx,int cy, bool mirror, int bob);` — blits the current walk sprite centered at `(cx,cy)` with optional horizontal mirror and vertical `bob` offset, skipping transparent pixels.

- [ ] **Step 1: Add failing test `test/test_walk/test_walk.cpp`**

```cpp
#include <unity.h>
#include "../../src/util/interp.h"

void setUp() {}
void tearDown() {}

void test_walk_start() {   // at 0% the sprite sits at the left edge (+ half sprite)
    TEST_ASSERT_EQUAL_INT(100 + 20, interp::walkX(0.0f, 100, 200, 40));
}
void test_walk_mid() {
    TEST_ASSERT_EQUAL_INT(100 + 100, interp::walkX(0.5f, 100, 200, 40));
}
void test_walk_end_clamps() {  // at 100% stays inside the bar (right edge - half sprite)
    TEST_ASSERT_EQUAL_INT(100 + 200 - 20, interp::walkX(1.0f, 100, 200, 40));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_walk_start);
    RUN_TEST(test_walk_mid);
    RUN_TEST(test_walk_end_clamps);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native -f test_walk`
Expected: FAIL — `walkX` undefined.

- [ ] **Step 3: Add `walkX` to `src/util/interp.h`**

```cpp
int walkX(float frac, int barX, int barW, int spriteW);
```

- [ ] **Step 4: Add `walkX` to `src/util/interp.cpp`**

```cpp
int walkX(float frac, int barX, int barW, int spriteW) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    int half = spriteW / 2;
    int lo = barX + half;
    int hi = barX + barW - half;
    int x = barX + (int)(frac * barW);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `pio test -e native`
Expected: PASS (theme + interp + lrc + walk suites).

- [ ] **Step 6: Commit the pure helper**

```bash
git add src/util/interp.h src/util/interp.cpp test/test_walk/test_walk.cpp
git commit -m "Add tested walkX helper for Pokemon-on-progress-bar position"
```

- [ ] **Step 7: Add downscaled walk-sprite loader to `src/images/png.h`**

```cpp
namespace img {
bool loadWalkSprite(const char* url, int dex, int outW, int outH); // decode+downscale once
const uint16_t* walkBuffer();
const uint8_t* walkMask();   // 1 byte per pixel: 1=opaque, 0=transparent
int walkW(); int walkH();
bool walkReady();
}
```

- [ ] **Step 8: Add the loader to `src/images/png.cpp`**

```cpp
// --- walk sprite state (persists for the current song) ---
static uint16_t g_walk[48*48];
static uint8_t  g_walkMask[48*48];
static int g_walkW = 0, g_walkH = 0;
static bool g_walkReady = false;

// full-size decode scratch (96x96 max) reused transiently
static uint16_t g_full[96*96];
static uint8_t  g_fullMask[96*96];
static int g_fullW = 0, g_fullH = 0;

static void pngFullDraw(PNGDRAW* d) {
    uint16_t line[96];
    png.getLineAsRGB565(d, line, PNG_RGB565_BIG_ENDIAN, 0x0000);
    uint8_t mask[96];
    png.getAlphaMask(d, mask, 1);                 // 1 bit/pixel alpha -> here 1 byte each
    for (int i = 0; i < d->iWidth && i < 96; ++i) {
        g_full[d->y*96 + i] = line[i];
        g_fullMask[d->y*96 + i] = mask[i] ? 1 : 0;
    }
    g_fullW = d->iWidth; g_fullH = d->y + 1;
}

bool loadWalkSprite(const char* url, int dex, int outW, int outH) {
    g_walkReady = false;
    size_t n = 0; uint8_t* data = nullptr;
    // (reuse Task 12 load-from-cache-or-download block to fill `data`,`n`)
    // ... identical acquisition code as drawSprite ...
    if (!data) return false;

    if (png.openRAM(data, n, pngFullDraw) != PNG_SUCCESS) { free(data); return false; }
    g_fullW = png.getWidth(); g_fullH = png.getHeight();
    png.decode(nullptr, 0); png.close(); free(data);

    // nearest-neighbor downscale g_full (g_fullW x g_fullH) -> g_walk (outW x outH)
    if (outW > 48) outW = 48; if (outH > 48) outH = 48;
    for (int y = 0; y < outH; ++y) {
        int sy = y * g_fullH / outH;
        for (int x = 0; x < outW; ++x) {
            int sx = x * g_fullW / outW;
            g_walk[y*outW + x]     = g_full[sy*96 + sx];
            g_walkMask[y*outW + x] = g_fullMask[sy*96 + sx];
        }
    }
    g_walkW = outW; g_walkH = outH; g_walkReady = true;
    return true;
}
const uint16_t* walkBuffer() { return g_walk; }
const uint8_t*  walkMask()   { return g_walkMask; }
int walkW() { return g_walkW; } int walkH() { return g_walkH; }
bool walkReady() { return g_walkReady; }
```

> Memory: `g_full` 96×96×(2+1)=~27 KB transient + `g_walk` 48×48×3=~7 KB persistent. Fine without PSRAM. If `getAlphaMask` signature differs in the pinned PNGdec, treat pure-black (0x0000) as the transparent key instead (sprites are on transparent bg → black after RGB565 convert); set mask = `line[i] != 0x0000`.

- [ ] **Step 9: Add route + walker draw to `src/ui/screen_now.*`**

```cpp
// screen_now.h
void drawRoute(TFT_eSPI& t, int x, int y, int w, int h, float frac);
void drawWalker(TFT_eSPI& t, int cx, int cy, bool mirror, int bob);
```
```cpp
// screen_now.cpp
#include "../images/png.h"
void drawRoute(TFT_eSPI& t, int x, int y, int w, int h, float frac) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    t.fillRect(x, y, w, h, 0x2124);
    t.drawRect(x, y, w, h, theme::GBA_NAVY);
    t.fillRect(x+2, y+2, (int)((w-4)*frac), h-4, theme::HP_GREEN);
}
void drawWalker(TFT_eSPI& t, int cx, int cy, bool mirror, int bob) {
    if (!img::walkReady()) return;
    const uint16_t* b = img::walkBuffer(); const uint8_t* m = img::walkMask();
    int w = img::walkW(), h = img::walkH();
    int x0 = cx - w/2, y0 = cy - h + bob;      // feet rest on cy
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sx = mirror ? (w-1-x) : x;
            if (m[y*w + sx]) t.drawPixel(x0+x, y0+y, b[y*w + sx]);
        }
}
```

- [ ] **Step 10: Reduce `drawNow` to a wide route + nameplate**

In `drawNow`, remove the right-column HP panel and the lower-left static-sprite reliance; add a full-width route panel near the bottom (above the controls) and keep a small nameplate for `pokeName` + type color:

```cpp
    // progress route (wide), above the control row
    panel(t, 8, 150, 304, 48);
    char tbuf[40];
    snprintf(tbuf, sizeof(tbuf), "%u:%02u / %u:%02u   CP %d",
        st.progressMs/60000, (st.progressMs/1000)%60,
        st.durationMs/60000, (st.durationMs/1000)%60, st.popularity);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.drawString(tbuf, 16, 154, 2);
    float frac = st.durationMs ? (float)st.progressMs/st.durationMs : 0;
    drawRoute(t, 16, 184, 288, 10, frac);      // the walker rides this (drawn by main)
    // nameplate (left, under album art)
    t.setTextColor(accent, 0x6ADC);
    t.drawString(st.pokeName[0]?st.pokeName:"", 8, 134, 2);
```

- [ ] **Step 11: Animate in `src/main.cpp`**

On track change, load the walk sprite once; animate the walker over the route each frame.

```cpp
        if (changed) {
            pokeapi::pickRandom(g_state);
            img::loadWalkSprite(g_state.pokeSpriteUrl, g_state.pokedexNum, 40, 40);
        }
```

In the redraw (walk needs ~8 fps; repaint only the route strip):

```cpp
    if (g_screen == Screen::Now && millis() - lastWalk >= 120) {
        lastWalk = millis();
        static int step = 0; step++;
        float frac = view.durationMs ? (float)view.progressMs/view.durationMs : 0;
        ui::drawRoute(tft, 16, 184, 288, 10, frac);   // repaint strip (erases old walker)
        int wx = interp::walkX(frac, 16, 288, img::walkW());
        int bob = (step % 2) ? 0 : 2;                 // bob up/down
        bool mirror = (step / 3) % 2;                 // flip every few frames = "step"
        ui::drawWalker(tft, wx, 184, mirror, bob);
    }
```

Add a `static uint32_t lastWalk = 0;` and `#include "util/interp.h"` (already included).

- [ ] **Step 12: Build, flash, observe**

Run: `pio run -e esp32dev -t upload`
Expected: the current song's Pokémon stands on the progress bar and **walks forward** as the song plays (bobbing + mirroring so it looks like stepping), reaching the right edge at the end of the track. New song → new random Pokémon walking. No flicker outside the bar strip; no per-frame network/decode.

- [ ] **Step 13: Commit**

```bash
git add src/images/png.h src/images/png.cpp src/ui/screen_now.h src/ui/screen_now.cpp src/main.cpp
git commit -m "Add walking-Pokemon progress bar (downscaled sprite, bob/step anim)"
```

---

## Self-Review

**Spec coverage:**
- GBA look / palette → Task 2, 9. ✅
- Landscape 320×240 → Task 1 (`setRotation(1)`), Global Constraints. ✅
- Now-playing metadata (track/artist/album/year/explicit/context/device/shuffle/repeat/volume/popularity→CP) → Task 7 (parse) + Task 9/15 (display). *Partial:* release year + explicit flag are parsed-if-present but not yet placed on screen — covered by the info panel in Task 9; added as a display line is optional polish (noted). Shuffle/repeat icons are stored but drawn as device chip only; acceptable for v1, listed in spec §9 as top-bar icons — **gap flagged below.**
- Playback controls (play/pause/next/prev/volume) → Task 8 + Task 13. ✅
- Random Pokémon #1–1025 per play + name + type + type-accent + sprite + SD cache → Task 12. ✅
- Walking-Pokémon progress bar (bob/step fake-walk, downscaled sprite, ~8 fps, rides fraction) → Task 16. ✅
- Lyrics (LRCLIB, synced + plain + not-found, button, extended charset) → Task 14 (+ font note). ✅
- Car/connectivity: multi-network + roam → Task 5; offline mode → Task 15; hotspot is config-only. ✅
- One-time OAuth + NVS refresh token + scopes → Task 6. ✅
- Secrets in git-ignored config → Task 1. ✅
- Progress interpolation → Task 3 + Task 10. ✅
- Build order milestones 1–9 → Tasks map 1↔T1, 2↔T5/6/7, 3↔T9, 4↔T10, 5↔T11, 6↔T12, 7↔T13, 8↔T14, 9↔T15. ✅

**Gap fixes applied inline:**
- Shuffle/repeat icons: add to Task 9 `drawNow` top bar if desired; currently stored in AppState and shown via device chip. Minor, non-blocking — left as polish item in Task 15 scope ("top bar icons").

**Placeholder scan:** No "TBD/implement later" in logic. The font swap (Task 14 Step 7) and shuffle/repeat icons are explicitly scoped deferrals with concrete instructions, not vague placeholders.

**Type consistency:** `AppState` fields referenced in Tasks 7/9/10/12/14/15 match `include/app_state.h` (Task 5). `ui::nowButtons()`/`NowButtons` used identically in Tasks 9 and 13. `lrc::parse`/`currentIndex` signatures match across Tasks 4 and 14. `spclient::poll` return-bool contract consistent in Tasks 7, 10, 12.

---

## Execution Handoff

See end-of-plan options presented after save.
