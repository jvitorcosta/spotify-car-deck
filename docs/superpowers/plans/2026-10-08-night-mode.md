# Night Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** From 18:00 to 06:00 Manaus time the deck switches by itself to the "Moonlit battle"
palette and a dimmer backlight, and back to today's look in the morning.

**Architecture:** The mode-dependent colours move into a `theme::Palette` struct with two
fixed instances (`DAY` = today's values, `NIGHT`); drawing code reads `theme::pal()`. A pure
`util/daynight` module turns UTC seconds into Manaus minute-of-day and decides night/day;
`net/clock` (namespace `netclock`) starts SNTP and reads the time; `ui/backlight` drives
GPIO 21 with LEDC PWM. The UI loop checks every 10 s and, on a change, swaps the palette,
sets the backlight and redraws the current screen once.

**Tech Stack:** C++17, Arduino-ESP32 2.0.17 (PlatformIO `espressif32@6.9.0`), TFT_eSPI,
Unity host tests built with g++ (`tools/ntest.ps1`, `tools/run_tests.ps1`).

**Spec:** `docs/superpowers/specs/2026-10-08-night-mode-design.md`

## Global Constraints

- Night = 18:00 (inclusive) to 06:00 (exclusive), Manaus, UTC-4, no daylight saving.
- Night changes both the palette and the backlight; day keeps full brightness (255).
- `NIGHT` palette = "Moonlit battle" values from the spec table; `DAY` = today's values,
  unchanged.
- Unchanged in both modes: HP colours, `HP_EMPTY`, `HP_TAG`, `HP_TAG_TEXT`, `EXP_BLUE`,
  `CD_SILVER`, type colours, genre badge colours, `GBA_NAVY`, `POKE_RED`.
- No synced time (or no WiFi) -> day mode.
- Only the UI loop (core 1) reads or sets the active palette; no lock.
- No colour cached in a `static` across a mode change.
- No new TLS session; SNTP only.
- Commits: Conventional Commits, condensed, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Work on `main`.
- Never print secrets; `src/config.h` is git-ignored and never committed.

## Rulings made while planning (spec deviations)

- **Namespace `netclock`, not `clock`:** a namespace `clock` collides with the C library's
  `clock()` function from `<time.h>`. The file is still `src/net/clock.{h,cpp}`.
- **UTC offset lives in `util/daynight`** (`UTC_OFFSET_S`), not in `clock.cpp`, so the
  offset maths is host-tested. `netclock` calls `configTime(0, 0, ...)` (UTC) and does no
  time-zone work of its own.
- **New palette field `note`:** the dancing note icons are drawn in `DLG_FRAME` on
  `DLG_FILL`; with the night values that is 1.1:1 (invisible). `note` = `#284860` by day
  (today's colour) and `#6A90B0` at night (4.7:1).
- **New fixed constant `BALL_DARK` = `#404040`:** the Poke Ball's outline used `TEXT`, which
  turns cream at night. It keeps today's dark grey in both modes.

## Review Focus

- Mode switch while a status screen ("No signal..." / "Nothing playing") is shown: that
  screen must redraw in the new palette, and the deck must too when it comes back.
- A UTC time just after midnight (e.g. 02:00 UTC = 22:00 Manaus): the negative offset must
  wrap to the previous day, not give a negative minute. Pinned in Task 2.
- The note icons and the "off" top-strip icons must stay visible at night. Pinned in Task 1
  (contrast tests run over both palettes).
- Boot at night: the deck starts in day mode and switches within ~10 s of the SNTP sync;
  no crash or stuck day mode if SNTP never answers. Pinned in Task 2 (`synced`).
- Walker / Poke Ball / icon backgrounds: no light rectangles around them at night (they
  must read `pal()` at draw time). Checked on the board in Task 3.

---

### Task 1: Day and night palettes

**Files:**
- Modify: `src/ui/theme.h`, `src/ui/theme.cpp`
- Modify: `src/ui/battle.cpp`, `src/ui/icons.cpp`, `src/ui/screen_now.cpp`
- Test: `test/test_theme/test_theme.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace theme {
  struct Palette {
      uint16_t top, topText, iconOff, sky, horizon, grass;
      uint16_t boxFill, boxBorder, boxShadow, text, textShadow;
      uint16_t dlgFrame, dlgLine, dlgFill, dlgShadow, note;
  };
  extern const Palette DAY;
  extern const Palette NIGHT;
  const Palette& pal();          // active palette, DAY at boot
  void setNight(bool night);
  bool isNightActive();
  constexpr uint16_t BALL_DARK = rgb(0x40, 0x40, 0x40);
  }
  ```
- Removed from `theme.h`: `SKY, GRASS, HORIZON, BOX_FILL, BOX_BORDER, BOX_SHADOW, TEXT,
  TEXT_SHADOW, DLG_FRAME, DLG_LINE, DLG_FILL, DLG_SHADOW, TOP_DARK, ICON_OFF`.

- [ ] **Step 1: Write the failing tests**

In `test/test_theme/test_theme.cpp`, replace `test_off_icons_visible_but_distinct_from_on`
and `test_text_is_high_contrast` with the tests below (they use the existing `contrast()`
helper), and add the new ones to `main()`:

```cpp
// DAY must be today's look exactly: night mode may not change the day screen.
void test_day_palette_is_todays_colours() {
    const theme::Palette& d = theme::DAY;
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x28, 0x30, 0x38), d.top);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xD8), d.topText);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x80, 0x88, 0x90), d.iconOff);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xA8, 0xD8, 0xF8), d.sky);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x60, 0xA8, 0x58), d.horizon);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x88, 0xC8, 0x78), d.grass);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xD8), d.boxFill);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x40, 0x48, 0x48), d.boxBorder);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x58, 0x70, 0x60), d.boxShadow);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x40, 0x40, 0x40), d.text);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xD8, 0xD0, 0xB0), d.textShadow);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x28, 0x48, 0x60), d.dlgFrame);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x68, 0xA0, 0xB8), d.dlgLine);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xF8), d.dlgFill);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xD0, 0xD0, 0xD0), d.dlgShadow);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x28, 0x48, 0x60), d.note);
}
void test_night_palette_is_moonlit() {
    const theme::Palette& n = theme::NIGHT;
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x18, 0x28, 0x4A), n.sky);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x2A, 0x32, 0x40), n.boxFill);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xE0, 0xDC, 0xC8), n.text);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x1C, 0x24, 0x30), n.dlgFill);
}
// Read at a glance in a car, day and night.
void test_text_is_high_contrast_in_both_palettes() {
    for (const theme::Palette* p : {&theme::DAY, &theme::NIGHT}) {
        TEST_ASSERT_TRUE(contrast(p->text, p->boxFill) >= 7.0);
        TEST_ASSERT_TRUE(contrast(p->text, p->dlgFill) >= 7.0);
        TEST_ASSERT_TRUE(contrast(p->topText, p->top) >= 7.0);
    }
}
void test_off_icons_visible_but_distinct_from_on_in_both_palettes() {
    for (const theme::Palette* p : {&theme::DAY, &theme::NIGHT}) {
        TEST_ASSERT_TRUE(contrast(p->iconOff, p->top) >= 3.0);       // still visible
        TEST_ASSERT_TRUE(contrast(p->topText, p->iconOff) >= 3.0);   // on vs off
    }
}
void test_note_icons_visible_in_both_palettes() {
    for (const theme::Palette* p : {&theme::DAY, &theme::NIGHT})
        TEST_ASSERT_TRUE(contrast(p->note, p->dlgFill) >= 3.0);
}
void test_set_night_switches_the_active_palette() {
    TEST_ASSERT_EQUAL_PTR(&theme::DAY, &theme::pal());           // boot: day
    TEST_ASSERT_FALSE(theme::isNightActive());
    theme::setNight(true);
    TEST_ASSERT_EQUAL_PTR(&theme::NIGHT, &theme::pal());
    TEST_ASSERT_TRUE(theme::isNightActive());
    theme::setNight(false);
    TEST_ASSERT_EQUAL_PTR(&theme::DAY, &theme::pal());
}
```

Add `#include <initializer_list>` after `#include <math.h>`. In `main()`, replace the two old
`RUN_TEST` lines and add (keep `test_set_night_switches_the_active_palette` FIRST, since it
asserts the boot state):

```cpp
    RUN_TEST(test_set_night_switches_the_active_palette);
    RUN_TEST(test_day_palette_is_todays_colours);
    RUN_TEST(test_night_palette_is_moonlit);
    RUN_TEST(test_text_is_high_contrast_in_both_palettes);
    RUN_TEST(test_off_icons_visible_but_distinct_from_on_in_both_palettes);
    RUN_TEST(test_note_icons_visible_in_both_palettes);
```

- [ ] **Step 2: Run the tests to verify they fail**

Run (PowerShell, repo root):
```
$env:MINGW_BIN = "C:\Users\joaov\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_theme\test_theme.cpp src\ui\theme.cpp src\util\text.cpp
```
Expected: COMPILE FAILED, `'Palette' in namespace 'theme' does not name a type` / `'DAY' is
not a member of 'theme'`.

- [ ] **Step 3: Implement the palettes**

`src/ui/theme.h`: delete the 14 constants listed under "Removed" (lines `SKY` ... `ICON_OFF`,
with their comments), keep `rgb`, `GBA_NAVY`, `POKE_RED`, `HP_TAG`, `HP_TAG_TEXT`,
`HP_EMPTY` (with its comment), `EXP_BLUE`, `CD_SILVER`, the HP colours and helpers, and add
after `CD_SILVER`:

```cpp
// Poke Ball outline: today's dark grey in both modes (TEXT turns cream at night).
constexpr uint16_t BALL_DARK   = rgb(0x40, 0x40, 0x40);

// Colours that change between day and night mode (18:00-06:00 Manaus, see util/daynight).
// DAY is the original Gen-3 battle look; NIGHT the "Moonlit battle" one (spec 2026-10-08).
struct Palette {
    uint16_t top, topText, iconOff, sky, horizon, grass;   // top strip, scene
    uint16_t boxFill, boxBorder, boxShadow, text, textShadow;   // battle boxes
    uint16_t dlgFrame, dlgLine, dlgFill, dlgShadow, note;  // dialogue box, note icons
};
extern const Palette DAY;
extern const Palette NIGHT;
// Active palette (DAY at boot). UI loop only: read at draw time, never cached.
const Palette& pal();
void setNight(bool night);
bool isNightActive();
```

`src/ui/theme.cpp`, inside `namespace theme` before `struct TypeColor`:

```cpp
// Field order: top, topText, iconOff, sky, horizon, grass, boxFill, boxBorder, boxShadow,
// text, textShadow, dlgFrame, dlgLine, dlgFill, dlgShadow, note.
// Day: approximations of the GBA games, tuned on the panel. ICON_OFF was 0x606870 (2.4:1 on
// the top strip: "off" icons vanished in daylight).
const Palette DAY = {
    rgb(0x28, 0x30, 0x38), rgb(0xF8, 0xF8, 0xD8), rgb(0x80, 0x88, 0x90),
    rgb(0xA8, 0xD8, 0xF8), rgb(0x60, 0xA8, 0x58), rgb(0x88, 0xC8, 0x78),
    rgb(0xF8, 0xF8, 0xD8), rgb(0x40, 0x48, 0x48), rgb(0x58, 0x70, 0x60),
    rgb(0x40, 0x40, 0x40), rgb(0xD8, 0xD0, 0xB0),
    rgb(0x28, 0x48, 0x60), rgb(0x68, 0xA0, 0xB8), rgb(0xF8, 0xF8, 0xF8),
    rgb(0xD0, 0xD0, 0xD0), rgb(0x28, 0x48, 0x60),
};
// Night: navy sky, dark green field, slate boxes, cream text.
const Palette NIGHT = {
    rgb(0x0E, 0x12, 0x16), rgb(0xC8, 0xC4, 0xB0), rgb(0x5A, 0x64, 0x70),
    rgb(0x18, 0x28, 0x4A), rgb(0x1E, 0x3A, 0x2A), rgb(0x1F, 0x3B, 0x2B),
    rgb(0x2A, 0x32, 0x40), rgb(0x0E, 0x12, 0x18), rgb(0x0A, 0x0E, 0x14),
    rgb(0xE0, 0xDC, 0xC8), rgb(0x10, 0x14, 0x18),
    rgb(0x10, 0x1C, 0x28), rgb(0x3A, 0x5A, 0x70), rgb(0x1C, 0x24, 0x30),
    rgb(0x0A, 0x0E, 0x14), rgb(0x6A, 0x90, 0xB0),
};
static const Palette* s_active = &DAY;
const Palette& pal() { return *s_active; }
void setNight(bool night) { s_active = night ? &NIGHT : &DAY; }
bool isNightActive() { return s_active == &NIGHT; }
```

- [ ] **Step 4: Run the theme tests to verify they pass**

Run: the Step 2 command.
Expected: `18 Tests 0 Failures 0 Ignored`.

- [ ] **Step 5: Move the drawing code to `pal()`**

Special cases first (they are NOT the plain mapping):
- `src/ui/screen_now.cpp` `drawTopStrip`: the title, device name and the "on" icons use
  `theme::BOX_FILL` as their foreground: change those 5 uses (title `shadowText`, device-name
  `drawText`, device `drawIcon`, repeat and shuffle "on" branch) to `theme::pal().topText`.
- `drawBall`: `case pokeball::Px::Dark: c = theme::TEXT;` -> `c = theme::BALL_DARK;`.
- `drawNotes`: the two `drawIcon(... theme::DLG_FRAME, theme::DLG_FILL)` foregrounds ->
  `theme::pal().note`.

Then every remaining use in `battle.cpp`, `icons.cpp`, `screen_now.cpp` maps one to one:

| old | new |
|---|---|
| `theme::TOP_DARK` | `theme::pal().top` |
| `theme::ICON_OFF` | `theme::pal().iconOff` |
| `theme::SKY` | `theme::pal().sky` |
| `theme::HORIZON` | `theme::pal().horizon` |
| `theme::GRASS` | `theme::pal().grass` |
| `theme::BOX_FILL` | `theme::pal().boxFill` |
| `theme::BOX_BORDER` | `theme::pal().boxBorder` |
| `theme::BOX_SHADOW` | `theme::pal().boxShadow` |
| `theme::TEXT_SHADOW` | `theme::pal().textShadow` |
| `theme::TEXT` | `theme::pal().text` |
| `theme::DLG_FRAME` | `theme::pal().dlgFrame` |
| `theme::DLG_LINE` | `theme::pal().dlgLine` |
| `theme::DLG_FILL` | `theme::pal().dlgFill` |
| `theme::DLG_SHADOW` | `theme::pal().dlgShadow` |

(`TEXT_SHADOW` before `TEXT`, whole-word.) `drawWalker`'s `fillBE` is computed per call, so
it follows the palette. Check nothing else is left:

Run: `git grep -nE "theme::(SKY|GRASS|HORIZON|BOX_|TEXT|DLG_|TOP_DARK|ICON_OFF)\b" -- src`
Expected: no output.

- [ ] **Step 6: Full tests and firmware build**

Run:
```
powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1
powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev
powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e hwcheck
```
Expected: `suites pass=24 fail=0`; both builds `[SUCCESS]`, no `src/` warnings.

- [ ] **Step 7: Commit**

```
git add src/ui/theme.h src/ui/theme.cpp src/ui/battle.cpp src/ui/icons.cpp src/ui/screen_now.cpp test/test_theme/test_theme.cpp
git commit -m "feat(ui): day and night palettes" -m "Mode-dependent colours move into theme::Palette (DAY = today's look, NIGHT = Moonlit battle); drawing code reads theme::pal(). Note icons and the Poke Ball outline get their own colours so they stay visible at night." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Day or night from the clock (pure)

**Files:**
- Create: `src/util/daynight.h`, `src/util/daynight.cpp`
- Test: `test/test_daynight/test_daynight.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace daynight {
  constexpr int NIGHT_START = 18 * 60;          // 18:00
  constexpr int NIGHT_END   = 6 * 60;           // 06:00
  constexpr int32_t UTC_OFFSET_S = -4 * 3600;   // Manaus (America/Manaus), no DST
  constexpr int64_t SYNCED_AFTER = 1704067200;  // 2024-01-01 00:00 UTC
  bool isNight(int minuteOfDay);                // [NIGHT_START, 1440) or [0, NIGHT_END)
  bool synced(int64_t utcSeconds);              // false: clock not set yet (boot = 1970)
  int minuteOfDay(int64_t utcSeconds, int32_t offsetS);   // local 0..1439
  }
  ```

- [ ] **Step 1: Write the failing test**

`test/test_daynight/test_daynight.cpp`:

```cpp
#include <unity.h>
#include "../../src/util/daynight.h"

void setUp() {}
void tearDown() {}

static int hm(int h, int m) { return h * 60 + m; }

void test_night_starts_at_six_pm() {
    TEST_ASSERT_FALSE(daynight::isNight(hm(17, 59)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(18, 0)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(23, 59)));
}
void test_night_runs_past_midnight_until_six_am() {
    TEST_ASSERT_TRUE(daynight::isNight(hm(0, 0)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(5, 59)));
    TEST_ASSERT_FALSE(daynight::isNight(hm(6, 0)));
    TEST_ASSERT_FALSE(daynight::isNight(hm(12, 0)));
}
// 2026-10-08 22:30:00 UTC = 18:30 in Manaus.
void test_minute_of_day_applies_the_manaus_offset() {
    const int64_t t = 1791498600;
    TEST_ASSERT_EQUAL_INT(hm(22, 30), daynight::minuteOfDay(t, 0));
    TEST_ASSERT_EQUAL_INT(hm(18, 30), daynight::minuteOfDay(t, daynight::UTC_OFFSET_S));
}
// 02:00 UTC is 22:00 the previous day in Manaus: the negative offset must wrap, not go < 0.
void test_minute_of_day_wraps_to_the_previous_day() {
    const int64_t t = 1791511200;   // 2026-10-09 02:00:00 UTC
    TEST_ASSERT_EQUAL_INT(hm(2, 0), daynight::minuteOfDay(t, 0));
    TEST_ASSERT_EQUAL_INT(hm(22, 0), daynight::minuteOfDay(t, daynight::UTC_OFFSET_S));
}
void test_clock_counts_as_synced_only_after_2024() {
    TEST_ASSERT_FALSE(daynight::synced(0));            // boot: 1970
    TEST_ASSERT_FALSE(daynight::synced(86400 * 3));
    TEST_ASSERT_TRUE(daynight::synced(1791498600));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_night_starts_at_six_pm);
    RUN_TEST(test_night_runs_past_midnight_until_six_am);
    RUN_TEST(test_minute_of_day_applies_the_manaus_offset);
    RUN_TEST(test_minute_of_day_wraps_to_the_previous_day);
    RUN_TEST(test_clock_counts_as_synced_only_after_2024);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_daynight\test_daynight.cpp`
Expected: COMPILE FAILED, `daynight.h: No such file or directory`.

- [ ] **Step 3: Implement**

`src/util/daynight.h`:

```cpp
#pragma once
#include <cstdint>
// Night mode schedule: 18:00-06:00 Manaus time (UTC-4 all year). Pure: the UTC seconds come
// from net/clock (SNTP), the result drives the palette and the backlight in main.cpp.
namespace daynight {
constexpr int NIGHT_START = 18 * 60;          // 18:00
constexpr int NIGHT_END   = 6 * 60;           // 06:00
constexpr int32_t UTC_OFFSET_S = -4 * 3600;   // Manaus (America/Manaus), no DST
constexpr int64_t SYNCED_AFTER = 1704067200;  // 2024-01-01 00:00 UTC
// True from NIGHT_START to midnight and from midnight to NIGHT_END.
bool isNight(int minuteOfDay);
// False while the clock still counts from 1970 (no SNTP answer yet).
bool synced(int64_t utcSeconds);
// Local minute of the day (0..1439) for a UTC time and an offset in seconds.
int minuteOfDay(int64_t utcSeconds, int32_t offsetS);
}
```

`src/util/daynight.cpp`:

```cpp
#include "daynight.h"

namespace daynight {

bool isNight(int m) { return m >= NIGHT_START || m < NIGHT_END; }

bool synced(int64_t utcSeconds) { return utcSeconds >= SYNCED_AFTER; }

int minuteOfDay(int64_t utcSeconds, int32_t offsetS) {
    int64_t s = (utcSeconds + offsetS) % 86400;
    if (s < 0) s += 86400;   // before 1970 + offset: keep it in 0..86399
    return (int)(s / 60);
}

}
```

- [ ] **Step 4: Run it to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_daynight\test_daynight.cpp src\util\daynight.cpp`
Expected: `5 Tests 0 Failures 0 Ignored`. Then `tools\run_tests.ps1`: `suites pass=25 fail=0`.

- [ ] **Step 5: Commit**

```
git add src/util/daynight.h src/util/daynight.cpp test/test_daynight/test_daynight.cpp
git commit -m "feat(util): night schedule for Manaus time" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Clock, backlight and the automatic switch

**Files:**
- Create: `src/net/clock.h`, `src/net/clock.cpp`, `src/ui/backlight.h`, `src/ui/backlight.cpp`
- Modify: `src/main.cpp`, `src/core/nettask.cpp`
- Modify: `README.md`, `docs/superpowers/specs/2026-10-08-night-mode-design.md`,
  `docs/superpowers/README.md` (only if it lists spec statuses)

**Interfaces:**
- Consumes: `theme::setNight(bool)`, `theme::isNightActive()` (Task 1);
  `daynight::isNight`, `daynight::synced`, `daynight::minuteOfDay`,
  `daynight::UTC_OFFSET_S` (Task 2).
- Produces:
  ```cpp
  namespace netclock { void begin(); bool minuteOfDay(int* m); }
  namespace backlight { void begin(); void set(bool night); }
  ```

No host test: these wrap SNTP, LEDC and the display; their logic is in `daynight` and
`theme`. Verified on the board (Steps 5-6).

- [ ] **Step 1: The clock**

`src/net/clock.h`:

```cpp
#pragma once
// Wall clock from SNTP (UTC). begin() once WiFi is up; SNTP then re-syncs in the background
// (UDP, no TLS). Named netclock: `clock` would collide with the C library's clock().
namespace netclock {
void begin();
// Manaus minute of the day (0..1439); false until the first SNTP answer.
bool minuteOfDay(int* m);
}
```

`src/net/clock.cpp`:

```cpp
#include "clock.h"
#include <Arduino.h>
#include <time.h>
#include "../util/daynight.h"

namespace netclock {

void begin() {
    static bool started = false;
    if (started) return;
    started = true;
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // UTC; daynight adds the offset
    Serial.println("[clock] SNTP started");
}

bool minuteOfDay(int* m) {
    time_t now = time(nullptr);
    if (!daynight::synced((int64_t)now)) return false;
    *m = daynight::minuteOfDay((int64_t)now, daynight::UTC_OFFSET_S);
    return true;
}

}
```

- [ ] **Step 2: The backlight**

`src/ui/backlight.h`:

```cpp
#pragma once
// Display backlight on GPIO 21 (PIN_BL) through LEDC PWM: full by day, dimmer at night.
namespace backlight {
void begin();            // after tft.init() (which drives the pin high); starts at day level
void set(bool night);
}
```

`src/ui/backlight.cpp`:

```cpp
#include "backlight.h"
#include <Arduino.h>
#include "../pins.h"

namespace backlight {

constexpr int CHANNEL = 1;          // LEDC channel (nothing else uses LEDC)
constexpr int FREQ_HZ = 5000;
constexpr int BITS = 8;
constexpr int DAY_LEVEL = 255;      // full brightness (auto-dim declined 2026-10-07)
constexpr int NIGHT_LEVEL = 90;     // ~35 %; tune on the panel

void begin() {
    ledcSetup(CHANNEL, FREQ_HZ, BITS);
    ledcAttachPin(PIN_BL, CHANNEL);
    ledcWrite(CHANNEL, DAY_LEVEL);
}

void set(bool night) { ledcWrite(CHANNEL, night ? NIGHT_LEVEL : DAY_LEVEL); }

}
```

Check `PIN_BL`'s header first: `git grep -n "PIN_BL" -- src/pins.h` (expected: `#define`
or `constexpr` with 21). If `pins.h` is not at `src/pins.h`, fix the include path.

- [ ] **Step 3: Start the clock**

`src/main.cpp`: add `#include "net/clock.h"`, `#include "ui/backlight.h"`,
`#include "ui/theme.h"`, `#include "util/daynight.h"`. In `setup()`:
- right after `s_tft.init(); ... s_tft.fillScreen(TFT_BLACK);` add `backlight::begin();`
- inside `if (net::connectAny()) {`, right after `mem::log("boot+wifi");` add
  `netclock::begin();`

`src/core/nettask.cpp`: add `#include "../net/clock.h"`; in the late-WiFi branch, after
`spclient::begin();` add `netclock::begin();`.

- [ ] **Step 4: The switch**

`src/main.cpp`: add with the other constants:

```cpp
constexpr uint32_t MODE_CHECK_MS = 10000; // day/night check (18:00-06:00 Manaus)
```

and add this helper above `loop()`:

```cpp
// Night mode: every MODE_CHECK_MS, picks the palette and backlight for the Manaus time. True
// when the mode changed (the caller redraws the current screen). No clock yet -> day.
static bool updateDayNight() {
    static uint32_t lastCheck = 0;
    static bool first = true, loggedSync = false;
    uint32_t now = millis();
    if (!first && now - lastCheck < MODE_CHECK_MS) return false;
    first = false;
    lastCheck = now;
    int m = 0;
    bool synced = netclock::minuteOfDay(&m);
    if (synced && !loggedSync) {
        loggedSync = true;
        Serial.printf("[clock] synced, Manaus %02d:%02d\n", m / 60, m % 60);
    }
    bool night = synced && daynight::isNight(m);
    if (night == theme::isNightActive()) return false;
    theme::setNight(night);
    backlight::set(night);
    Serial.printf("[ui] night mode %s\n", night ? "on" : "off");
    return true;
}
```

In `loop()`, right after the four `static` declarations (`lastMode`, `shownGen`, `walkerOn`,
`shownGenre`), add:

```cpp
    if (updateDayNight()) lastMode = -1;   // redraw the deck or the status screen in the new palette
```

(`lastMode = -1` makes the next branch redraw: a status screen because `lastMode != mode`,
the deck through the "back from a status screen" path, which repaints everything, resets
the top strip signature and the lyric cache.)

- [ ] **Step 5: Build, then review night mode by day (temporary)**

Temporarily replace `bool night = synced && daynight::isNight(m);` with
`bool night = true;   // TEMP preview`. Then:

Run:
```
powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11
python tools\capture_serial.py COM11 90 <scratchpad>\night.log
```
Expected: `[ui] night mode on` within ~1 s of boot; `[clock] SNTP started` and
`[clock] synced, Manaus HH:MM` (correct local time) a few seconds after WiFi; `[mem]` largest
block still 34804 at "track".

Look at the panel with the user: dark scene, slate boxes, cream text; no light rectangles
around the walker, Poke Ball or icons; note icons visible; "off" icons visible; dark RAP /
R&B badges distinguishable. Tune `NIGHT_LEVEL` with the user if needed. If the dark badges'
edges vanish, in `drawBadge` (screen_now.cpp) use
`uint16_t edge = theme::isNightActive() ? theme::pal().iconOff : theme::darken(c);`
for the border only (keep the text shadow `theme::darken(c)`).

Then restore `bool night = synced && daynight::isNight(m);`. Check it is restored:

Run: `git grep -n "TEMP preview" -- src`
Expected: no output.

- [ ] **Step 6: Full checks**

Run:
```
powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1
powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11
powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e hwcheck
```
Expected: `suites pass=25 fail=0`; both builds `[SUCCESS]`, no `src/` warnings. On the board
by day: `[clock] synced` and no `[ui] night mode on`.

- [ ] **Step 7: Docs**

- `README.md`, after the "Genre badge" module bullet, add:
  ```
  - **Night mode** (`util/daynight`, `net/clock`, `ui/backlight`) — 18:00–06:00 Manaus time
    (SNTP, UTC-4): "Moonlit battle" palette (`ui/theme` `NIGHT`) and a dimmer backlight.
  ```
  and add `daynight` to the `util/{...}` list of pure, host-tested modules.
- Spec: `Status: written-spec review.` -> `Status: implemented (main, <short sha>).`; add the
  `note` row to the palette table (`| note (note icons) | #284860 | #6A90B0 |`) and one line
  under the table: `The Poke Ball outline uses the fixed BALL_DARK (#404040) in both modes.`
- `docs/superpowers/README.md`: if it lists specs with statuses, add/update the night-mode
  line the same way.

- [ ] **Step 8: Commit**

```
git add src/net/clock.h src/net/clock.cpp src/ui/backlight.h src/ui/backlight.cpp src/main.cpp src/core/nettask.cpp README.md docs/superpowers
git commit -m "feat: automatic night mode at 18:00 Manaus time" -m "SNTP clock (UTC, offset in util/daynight), backlight PWM on GPIO 21 (255 by day, dimmer at night), and a 10 s check in the UI loop that swaps the palette and redraws the current screen. No synced clock -> day mode." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
