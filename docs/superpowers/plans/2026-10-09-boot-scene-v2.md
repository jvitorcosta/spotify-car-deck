# Boot Scene v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the boot scene's pixel City match the owner's car, switch to a purple underglow, and replace the drive-off with a VW-style ending: brake and fade to black, underglow, light sweep over a photo of the owner's car, headlight double-flash, "HONDA CITY".

**Architecture:** Pure timing/brightness maths for the ending goes into `util/bootanim` (host-tested). A new generator `tools/gen_hero_sprite.py` turns the owner's photo 7 into a committed 255-colour cut-out (`ui/hero_sprite`, host-tested; plate blanked). `ui/bootscene` gains two render modes after `tHero`: a whole-screen brake fade (10 strips) and a hero mode (8 strips, y 48–239) using the same 15 KB strip sprite.

**Tech Stack:** ESP32 Arduino 2.0.17 / IDF 4.4, TFT_eSPI 2.5.43 (`TFT_eSprite` incl. `drawString`), Unity host tests via `tools/run_tests.ps1`, Python 3 stdlib + ffmpeg for generators.

**Spec:** `docs/superpowers/specs/2026-10-09-boot-scene-v2-design.md` (on top of v1: `docs/superpowers/specs/2026-10-09-boot-scene-design.md`)

## Global Constraints

- Branch `feat/boot-scene`; v1 is implemented and committed there (`f83aa4c`). The executing-plans ledger for v1 is `.superpowers/sdd/2026-10-09-boot-scene/progress.md`; v2 gets its own workspace.
- No PSRAM; the scene keeps one 320 × 24 16-bit `TFT_eSprite` strip (15 360 B), freed at the end. `TFT_eSprite` buffers are byte-swapped: write through `theme::be()`.
- Ending beats (ms from `tHero`): Brake 0–600, Glow 600–900, Sweep 900–1400, Reveal 1400–1700, Flash 1700–2100, Hold 2100–3300.
- Underglow colour #8C46FF (fixed); alpha `glowAlpha(e)` unchanged.
- Hero: photo 7 = `4fd8e581-c70d-4e9d-8ae3-78add1cc188c.jpg`, extracted (not in the repo) at `C:\Users\joaov\AppData\Local\Temp\claude\D--dev-projects-spotify-pokemon-deck\e3c3514b-369e-4fa7-a655-d0f48395f21c\scratchpad\car_photos\`. The plate is blanked before quantisation; the photo and `Photos-*.zip` are never committed (`.gitignore` already covers the zip).
- Host tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` (28 suites before this plan). From bash, set `MINGW_BIN` to the WinLibs bin dir first (`C:\Users\joaov\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin`), otherwise test_cjkfont fails with 0xC0000139 (wrong libstdc++ on bash's PATH).
- Firmware: `pio run -e esp32dev`; flash `pio run -e esp32dev -t upload --upload-port COM11`; `pio run -e hwcheck` must keep building.
- Commits: `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- One deliberate refinement of the spec: during Brake the **world scroll slows with the wheels** (both use a braking clock `brakeTime`), not only the spokes; a road that keeps rushing past a braking car looks wrong. The spec's spoke rule (9 rad/s × (1 − q)) is exactly what `spokeAngle(brakeTime(...))` produces.

## Review Focus

1. **Brake from any cruise position** (x 54–66 at `tHero`): no jump on the first brake frame, stops exactly 40 px further. Pinned by `test_brake_starts_where_cruise_left_off` (Task 1).
2. **Ending beat boundaries**: each beat starts on its exact ms and the scene ends at `tHero + 3300`. Pinned by `test_hero_beats` / `test_hero_done_at` (Task 1).
3. **Plate never shown**: the blanked plate box is one flat colour in the committed sprite. Pinned by `test_plate_is_blanked` (Task 3).
4. **Name text clipped by strip boundaries**: "HONDA CITY" (y 222–237) must fall inside a single hero strip. Pinned by a `static_assert` in `bootscene.cpp` (Task 4, Step 1) — the build fails if strip layout and text position drift apart.
5. **Flares / lamp blends near the right screen edge** (headlight R flare reaches x ≈ 310): must not write outside the strip. Pinned by the existing `clip` tests plus `blendRect` clipping, and the on-deck check in Task 4, Step 5.

---

## File Structure

| File | Change |
|---|---|
| `src/util/bootanim.h/.cpp` | + ending beats and levels (`HeroBeat`, `heroBeat`, `heroProgress`, `brakeX`, `brakeTime`, `brakeLevel`, `heroLevel`, `lampsOn`, `nameLevel`, `heroDoneAt`) |
| `test/test_bootanim/test_bootanim.cpp` | + tests for the above |
| `tools/gen_car_sprite.py`, `src/ui/car_sprite.inc` | photo-informed refinements (tint, DRL on top, fog lamp, silver handles) |
| `test/test_car_sprite/test_car_sprite.cpp` | updated lamp test + new tint/fog/handle tests |
| `tools/gen_hero_sprite.py` (new), `src/ui/hero_sprite.h/.cpp/.inc` (new) | photo 7 cut-out, 262 × 148 at (32, 57), 255-colour palette, 4 lamp rects |
| `test/test_hero_sprite/test_hero_sprite.cpp` (new) | host suite |
| `src/ui/bootscene.cpp` | purple underglow, two-tone wheel edges, brake + hero modes (replaces drive-off) |
| `README.md`, v2 spec status | docs |

---

### Task 1: Ending maths (`util/bootanim`)

**Files:**
- Modify: `src/util/bootanim.h`, `src/util/bootanim.cpp`
- Test: `test/test_bootanim/test_bootanim.cpp`

**Interfaces:**
- Produces (namespace `bootanim`): `constexpr uint32_t BRAKE_MS = 600, GLOW_MS = 300, SWEEP_MS = 500, REVEAL_MS = 300, FLASH_MS = 400, HOLD_MS = 1200, HERO_MS = 3300;` `enum class HeroBeat : uint8_t { Brake, Glow, Sweep, Reveal, Flash, Hold, Done };` `HeroBeat heroBeat(uint32_t u)`; `float heroProgress(uint32_t u)`; `int brakeX(int xc, uint32_t u)`; `uint32_t brakeTime(uint32_t tHero, uint32_t u)`; `uint8_t brakeLevel(uint32_t u)`; `uint8_t heroLevel(uint32_t u, int x, int y)`; `bool lampsOn(uint32_t u)`; `uint8_t nameLevel(uint32_t u)`; `uint32_t heroDoneAt(uint32_t tHero)`. (`u` = ms since `tHero`.)

- [ ] **Step 1: Write the failing tests**

In `test/test_bootanim/test_bootanim.cpp`, add before `int main(`:
```cpp
// ---- v2 ending: brake, then the photo hero ----
void test_hero_beats() {
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Brake, (int)heroBeat(0));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Brake, (int)heroBeat(599));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Glow, (int)heroBeat(600));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Sweep, (int)heroBeat(900));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Reveal, (int)heroBeat(1400));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Flash, (int)heroBeat(1700));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Hold, (int)heroBeat(2100));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Hold, (int)heroBeat(3299));
    TEST_ASSERT_EQUAL_INT((int)HeroBeat::Done, (int)heroBeat(3300));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, heroProgress(300));     // half way through Brake
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, heroProgress(1150));    // half way through Sweep
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, heroProgress(9999));    // Done
}
void test_hero_done_at() {
    TEST_ASSERT_EQUAL_UINT32(3300u, HERO_MS);
    TEST_ASSERT_EQUAL_UINT32(10300u, heroDoneAt(7000));
}
// Review Focus 1: no jump at u = 0 from any cruise x, stops 40 px further.
void test_brake_starts_where_cruise_left_off() {
    for (int xc : {54, 60, 66}) {
        TEST_ASSERT_EQUAL_INT(xc, brakeX(xc, 0));
        TEST_ASSERT_EQUAL_INT(xc + 30, brakeX(xc, 300));
        TEST_ASSERT_EQUAL_INT(xc + 40, brakeX(xc, 600));
        TEST_ASSERT_EQUAL_INT(xc + 40, brakeX(xc, 5000));
    }
}
// The world and the wheels slow with the car: the braking clock advances 300 ms over the 600 ms brake.
void test_brake_time_slows_down() {
    TEST_ASSERT_EQUAL_UINT32(7000u, brakeTime(7000, 0));
    TEST_ASSERT_EQUAL_UINT32(7225u, brakeTime(7000, 300));
    TEST_ASSERT_EQUAL_UINT32(7300u, brakeTime(7000, 600));
    TEST_ASSERT_EQUAL_UINT32(7300u, brakeTime(7000, 3000));
}
void test_brake_fades_to_black() {
    TEST_ASSERT_EQUAL_UINT8(255, brakeLevel(0));
    TEST_ASSERT_EQUAL_UINT8(128, brakeLevel(300));
    TEST_ASSERT_EQUAL_UINT8(0, brakeLevel(600));
    TEST_ASSERT_EQUAL_UINT8(0, brakeLevel(2000));
}
void test_hero_brightness() {
    TEST_ASSERT_EQUAL_UINT8(0, heroLevel(700, 160, 120));          // Glow: hero still dark
    TEST_ASSERT_EQUAL_UINT8(255, heroLevel(1150, 160, 0));         // Sweep band centre (d = 160)
    TEST_ASSERT_EQUAL_UINT8(26, heroLevel(1150, 0, 0));            // outside the band: 10 %
    TEST_ASSERT_EQUAL_UINT8(26, heroLevel(1400, 0, 0));            // Reveal start
    TEST_ASSERT_EQUAL_UINT8(140, heroLevel(1550, 0, 0));           // Reveal half way: 55 %
    TEST_ASSERT_EQUAL_UINT8(255, heroLevel(1700, 0, 0));           // Flash
    TEST_ASSERT_EQUAL_UINT8(255, heroLevel(3000, 0, 0));           // Hold
}
void test_headlight_double_flash() {
    TEST_ASSERT_FALSE(lampsOn(1000));
    TEST_ASSERT_TRUE(lampsOn(1750));
    TEST_ASSERT_FALSE(lampsOn(1850));
    TEST_ASSERT_TRUE(lampsOn(1950));
    TEST_ASSERT_TRUE(lampsOn(2500));
}
void test_name_fades_in() {
    TEST_ASSERT_EQUAL_UINT8(0, nameLevel(1699));
    TEST_ASSERT_EQUAL_UINT8(128, nameLevel(1900));
    TEST_ASSERT_EQUAL_UINT8(255, nameLevel(2100));
    TEST_ASSERT_EQUAL_UINT8(255, nameLevel(3200));
}
```
and add to `main` before `return UNITY_END();`:
```cpp
    RUN_TEST(test_hero_beats);
    RUN_TEST(test_hero_done_at);
    RUN_TEST(test_brake_starts_where_cruise_left_off);
    RUN_TEST(test_brake_time_slows_down);
    RUN_TEST(test_brake_fades_to_black);
    RUN_TEST(test_hero_brightness);
    RUN_TEST(test_headlight_double_flash);
    RUN_TEST(test_name_fades_in);
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_bootanim\test_bootanim.cpp src\util\bootanim.cpp`
Expected: `COMPILE FAILED` (`HeroBeat`, `heroBeat` ... not declared).

- [ ] **Step 3: Implement**

In `src/util/bootanim.h`, before the final `}`:
```cpp

// ---- v2 ending (spec v2 §3): u = ms since tHero ----
constexpr uint32_t BRAKE_MS = 600, GLOW_MS = 300, SWEEP_MS = 500, REVEAL_MS = 300, FLASH_MS = 400,
                   HOLD_MS = 1200;
constexpr uint32_t HERO_MS = BRAKE_MS + GLOW_MS + SWEEP_MS + REVEAL_MS + FLASH_MS + HOLD_MS;   // 3300
enum class HeroBeat : uint8_t { Brake, Glow, Sweep, Reveal, Flash, Hold, Done };
HeroBeat heroBeat(uint32_t u);
float heroProgress(uint32_t u);                  // 0..1 within the current beat (1 when Done)
int brakeX(int xc, uint32_t u);                  // rolls 40 px from xc and stops (ease-out)
uint32_t brakeTime(uint32_t tHero, uint32_t u);  // braking clock for scroll and spokes
uint8_t brakeLevel(uint32_t u);                  // whole-frame brightness 255 -> 0
uint8_t heroLevel(uint32_t u, int x, int y);     // hero photo brightness at screen (x, y)
bool lampsOn(uint32_t u);                        // headlight/fog flash pattern; on during Hold
uint8_t nameLevel(uint32_t u);                   // "HONDA CITY" fade-in
uint32_t heroDoneAt(uint32_t tHero);
```

In `src/util/bootanim.cpp`, before the final `}`:
```cpp

// ---- v2 ending ----
static constexpr uint32_t BEAT_START[] = {0, BRAKE_MS, BRAKE_MS + GLOW_MS, BRAKE_MS + GLOW_MS + SWEEP_MS,
                                          BRAKE_MS + GLOW_MS + SWEEP_MS + REVEAL_MS,
                                          BRAKE_MS + GLOW_MS + SWEEP_MS + REVEAL_MS + FLASH_MS, HERO_MS};

HeroBeat heroBeat(uint32_t u) {
    int b = 0;
    while (b < 6 && u >= BEAT_START[b + 1]) ++b;
    return (HeroBeat)b;
}

float heroProgress(uint32_t u) {
    const int b = (int)heroBeat(u);
    if (b >= 6) return 1.0f;
    return (float)(u - BEAT_START[b]) / (float)(BEAT_START[b + 1] - BEAT_START[b]);
}

int brakeX(int xc, uint32_t u) {
    const float q = u >= BRAKE_MS ? 1.0f : (float)u / BRAKE_MS;
    return xc + (int)lroundf(40.0f * (1.0f - (1.0f - q) * (1.0f - q)));
}

uint32_t brakeTime(uint32_t tHero, uint32_t u) {
    const uint32_t uc = u < BRAKE_MS ? u : BRAKE_MS;
    const float q = (float)uc / BRAKE_MS;
    return tHero + (uint32_t)lroundf((float)uc * (1.0f - q / 2.0f));   // integral of (1 - q)
}

uint8_t brakeLevel(uint32_t u) {
    if (u >= BRAKE_MS) return 0;
    return (uint8_t)lroundf(255.0f * (1.0f - (float)u / BRAKE_MS));
}

uint8_t heroLevel(uint32_t u, int x, int y) {
    const HeroBeat b = heroBeat(u);
    const float q = heroProgress(u);
    switch (b) {
        case HeroBeat::Brake:
        case HeroBeat::Glow: return 0;
        case HeroBeat::Sweep: {
            const float d = (float)x + (float)y / 2.0f, s = -80.0f + 480.0f * q, off = fabsf(d - s);
            const float lvl = off < 30.0f ? 0.1f + 0.9f * (1.0f - off / 30.0f) : 0.1f;
            return (uint8_t)lroundf(255.0f * lvl);
        }
        case HeroBeat::Reveal: return (uint8_t)lroundf(255.0f * (0.1f + 0.9f * q));
        default: return 255;
    }
}

bool lampsOn(uint32_t u) {
    const HeroBeat b = heroBeat(u);
    if (b == HeroBeat::Flash) {
        const uint32_t f = u - BEAT_START[(int)HeroBeat::Flash];
        return f < 100 || f >= 200;          // on, off, on
    }
    return b == HeroBeat::Hold || b == HeroBeat::Done;
}

uint8_t nameLevel(uint32_t u) {
    const HeroBeat b = heroBeat(u);
    if ((int)b < (int)HeroBeat::Flash) return 0;
    if (b == HeroBeat::Flash) return (uint8_t)lroundf(255.0f * heroProgress(u));
    return 255;
}

uint32_t heroDoneAt(uint32_t tHero) { return tHero + HERO_MS; }
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_bootanim\test_bootanim.cpp src\util\bootanim.cpp`
Expected: `20 Tests 0 Failures 0 Ignored`

- [ ] **Step 5: Commit**

```bash
git add src/util/bootanim.h src/util/bootanim.cpp test/test_bootanim/test_bootanim.cpp
git commit -m "feat(util): boot scene ending maths (brake, hero beats, flash)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Photo-informed pixel car

**Files:**
- Modify: `tools/gen_car_sprite.py`, `src/ui/car_sprite.inc` (regenerated)
- Test: `test/test_car_sprite/test_car_sprite.cpp`

**Interfaces:**
- Consumes/Produces: `car_sprite::pixel/width/height/Kind` unchanged (Task 4 relies on them).

- [ ] **Step 1: Update the tests first**

In `test/test_car_sprite/test_car_sprite.cpp`, replace the whole `test_lamp_kinds` function with:
```cpp
// v2: the white DRL runs along the TOP edge of the headlight (owner's photos), LEDs under it.
void test_lamp_kinds() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(180, 28, &c));    // DRL strip, top edge
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(180, 29, &c));    // LED projector
    TEST_ASSERT_EQUAL_HEX16(0xEF9F, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(190, 32));       // smoked housing, no bottom DRL
    TEST_ASSERT_EQUAL_UINT8(car_sprite::TAIL, kindAt(8, 26));          // taillight LED strip
}
void test_fog_lamp_is_lit() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(192, 46, &c));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, c);
}
void test_near_black_tint() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(110, 10, &c));
    TEST_ASSERT_EQUAL_HEX16(0x0882, c);                                 // #0C1016
}
void test_silver_door_handles() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(84, 27, &c));
    TEST_ASSERT_EQUAL_HEX16(0xEF9E, c);                                 // #EEF2F5
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(84, 28, &c));
    TEST_ASSERT_EQUAL_HEX16(0x7C31, c);                                 // #7C858F under it
}
```
and add to `main` before `return UNITY_END();`:
```cpp
    RUN_TEST(test_fog_lamp_is_lit);
    RUN_TEST(test_near_black_tint);
    RUN_TEST(test_silver_door_handles);
```

- [ ] **Step 2: Run to verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_car_sprite\test_car_sprite.cpp src\ui\car_sprite.cpp`
Expected: failures in `test_lamp_kinds`, `test_fog_lamp_is_lit`, `test_near_black_tint`, `test_silver_door_handles` (old sprite).

- [ ] **Step 3: Change the generator**

In `tools/gen_car_sprite.py`:
1. In the colour table replace `glass="#16233a", glassHi="#4f6d92"` with `glass="#0c1016", glassHi="#2c3a4a"`, and replace `amber="#ffb030").items()}` with `amber="#ffb030", fog="#ffffff", fogRing="#cfe8ff").items()}`.
2. Replace
```python
    for x in range(172, 197):
        P(x, round(26.6 + (x - 172) * 0.13), "housingTop")
    for x in range(180, 197, 3):
        y = round(28 + (x - 180) * 0.14)
        P(x, y, "led", HEAD)
        P(x + 1, y, "led", HEAD)
    for x in range(172, 199):
        P(x, round(29.4 + (x - 172) * 0.17), "drl", HEAD)
```
with
```python
    for x in range(166, 198):                     # DRL strip along the top edge (owner's photos)
        P(x, round(26.0 + (x - 166) * 0.135), "drl", HEAD)
    for x in range(180, 197, 3):                  # LED projectors under it
        y = round(28.6 + (x - 180) * 0.16)
        P(x, y, "led", HEAD)
        P(x + 1, y, "led", HEAD)
```
3. Replace
```python
    fill(INTAKE, lambda x, y: P(x, y, "trim" if (x + y) % 3 else "dark"))
```
with
```python
    fill(INTAKE, lambda x, y: P(x, y, "trim"))     # bumper corner recess
    for y in range(41, 51):                       # lit LED fog lamp (owner's photos)
        for x in range(186, 199):
            d = math.hypot(x + 0.5 - 192.5, y + 0.5 - 46)
            if d < 2.4:
                P(x, y, "fog", HEAD)
            elif d < 3.4 and px[y][x]:
                P(x, y, "fogRing", HEAD)
```
4. In the door-handle loop replace
```python
            P(x, round(shoulder_y(x)), "chrome")
            P(x, round(shoulder_y(x)) + 1, "trim")
```
with
```python
            P(x, round(shoulder_y(x)), "hi", UPPER)     # silver, body-coloured handles
            P(x, round(shoulder_y(x)) + 1, "shade")
```
5. Remove the now-unused `housingTop="#7d8fa8", ` entry from the colour table.

- [ ] **Step 4: Regenerate and look at it**

Run: `python tools/gen_car_sprite.py` → `gen_car_sprite: src\ui\car_sprite.inc  200x66, 8373 opaque px`.
Run: `python tools/gen_car_sprite.py --png car_preview.png`; view it: near-black windows, white strip along the top of the headlight, a bright white dot in the front bumper corner, silver handles. Delete `car_preview.png`.

- [ ] **Step 5: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_car_sprite\test_car_sprite.cpp src\ui\car_sprite.cpp`
Expected: `9 Tests 0 Failures 0 Ignored`

- [ ] **Step 6: Commit**

```bash
git add tools/gen_car_sprite.py src/ui/car_sprite.inc test/test_car_sprite/test_car_sprite.cpp
git commit -m "feat(ui): car sprite details from the owner's photos (tint, DRL, fog lamp, handles)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Hero sprite from photo 7

**Files:**
- Create: `tools/gen_hero_sprite.py`, `src/ui/hero_sprite.h`, `src/ui/hero_sprite.cpp`, `src/ui/hero_sprite.inc` (generated), `test/test_hero_sprite/test_hero_sprite.cpp`

**Interfaces:**
- Produces (namespace `hero_sprite`): `constexpr int X = 32, Y = 57;` `int width()` (262); `int height()` (148); `bool pixel(int x, int y, uint16_t* rgb565)` (sprite coords; false = transparent/outside); `struct Rect { int16_t x0, y0, x1, y1; }` (screen coords, inclusive); `int lampCount()` (4); `Rect lamp(int i)`; `bool isHeadlight(int i)` (true for 0, 1).

- [ ] **Step 1: Write the failing test**

`test/test_hero_sprite/test_hero_sprite.cpp`:
```cpp
#include <unity.h>
#include "../../src/ui/hero_sprite.h"

void setUp() {}
void tearDown() {}

void test_size_and_position() {
    TEST_ASSERT_EQUAL_INT(262, hero_sprite::width());
    TEST_ASSERT_EQUAL_INT(148, hero_sprite::height());
    TEST_ASSERT_EQUAL_INT(32, hero_sprite::X);
    TEST_ASSERT_EQUAL_INT(57, hero_sprite::Y);
}
void test_transparent_outside_the_car() {
    TEST_ASSERT_FALSE(hero_sprite::pixel(0, 0, nullptr));               // top-left corner of the box
    TEST_ASSERT_FALSE(hero_sprite::pixel(-1, 10, nullptr));
    TEST_ASSERT_FALSE(hero_sprite::pixel(262, 10, nullptr));
    TEST_ASSERT_FALSE(hero_sprite::pixel(10, 148, nullptr));
}
void test_car_body_is_opaque() {
    uint16_t c = 0;
    TEST_ASSERT_TRUE(hero_sprite::pixel(200 - hero_sprite::X, 120 - hero_sprite::Y, &c));
}
void test_lamps() {
    TEST_ASSERT_EQUAL_INT(4, hero_sprite::lampCount());
    TEST_ASSERT_TRUE(hero_sprite::isHeadlight(0));
    TEST_ASSERT_TRUE(hero_sprite::isHeadlight(1));
    TEST_ASSERT_FALSE(hero_sprite::isHeadlight(2));
    TEST_ASSERT_FALSE(hero_sprite::isHeadlight(3));
    const hero_sprite::Rect r = hero_sprite::lamp(0);
    TEST_ASSERT_EQUAL_INT(131, r.x0);
    TEST_ASSERT_EQUAL_INT(120, r.y0);
    TEST_ASSERT_EQUAL_INT(192, r.x1);
    TEST_ASSERT_EQUAL_INT(145, r.y1);
}
// Review Focus 3: the number plate box is one flat colour (blanked before quantisation).
void test_plate_is_blanked() {
    uint16_t first = 0, c = 0;
    TEST_ASSERT_TRUE(hero_sprite::pixel(221 - hero_sprite::X, 158 - hero_sprite::Y, &first));
    for (int y = 158; y < 177; ++y)
        for (int x = 221; x < 263; ++x) {
            TEST_ASSERT_TRUE(hero_sprite::pixel(x - hero_sprite::X, y - hero_sprite::Y, &c));
            TEST_ASSERT_EQUAL_HEX16(first, c);
        }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_size_and_position);
    RUN_TEST(test_transparent_outside_the_car);
    RUN_TEST(test_car_body_is_opaque);
    RUN_TEST(test_lamps);
    RUN_TEST(test_plate_is_blanked);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_hero_sprite\test_hero_sprite.cpp`
Expected: `COMPILE FAILED`.

- [ ] **Step 3: Write the generator**

`tools/gen_hero_sprite.py` (prototyped on photo 7; reproduces the approved hero frame):
```python
"""Generate src/ui/hero_sprite.inc: the boot scene's final "hero" shot, the owner's photo 7 (front
three-quarter, nose right) cut out on black, plate blanked, 255 colours
(docs/superpowers/specs/2026-10-09-boot-scene-v2-design.md, section 4).
Run: python tools/gen_hero_sprite.py <photo 7 .jpg> [--png out.png]   (output is committed)
The photo itself never goes into the repo; the .inc holds the blanked, quantised cut-out only.
Needs ffmpeg on PATH. Otherwise standard library only.
"""
import math
import os
import struct
import subprocess
import sys
import tempfile
import zlib

W, H = 320, 240
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "ui", "hero_sprite.inc")

# Place the car as in the approved mockup and blank the number plate (never stored).
PLACE = ("scale=270:-1:flags=area,pad=320:ih+200:25:100:black,crop=320:240:0:224,"
         "drawbox=x=221:y=158:w=42:h=19:color=0xB8BCC0:t=fill")
OUTLINE = [(35, 160), (32, 140), (33, 117), (37, 97), (45, 80), (57, 66), (70, 60), (100, 57),
           (186, 57), (205, 72), (220, 83), (233, 92), (267, 110), (282, 121), (288, 127),
           (290, 143), (293, 173), (283, 182), (260, 190), (200, 193), (140, 197), (120, 204),
           (100, 197), (97, 182), (67, 168), (50, 166)]
FEATHER = 3.0
BOX = (32, 57, 294, 205)               # x0, y0, x1, y1 (exclusive): the outline's bounding box
# Lamp rectangles, screen coordinates, inclusive: headlight L, headlight R, fog L, fog R.
LAMPS = [(131, 120, 192, 145), (270, 124, 290, 138), (146, 163, 165, 177), (284, 152, 292, 162)]
HEADLIGHTS = 2


def ffmpeg(args):
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y"] + args, check=True)


def read_raw(path):
    d = open(path, "rb").read()
    assert len(d) == W * H * 3, len(d)
    return [tuple(d[i:i + 3]) for i in range(0, len(d), 3)]


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


def edge_dist(px, py, poly):
    best = 1e9
    for i in range(len(poly)):
        (ax, ay), (bx, by) = poly[i - 1], poly[i]
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / float(dx * dx + dy * dy)))
        best = min(best, math.hypot(px - ax - t * dx, py - ay - t * dy))
    return best


def mask():
    """Alpha 0..1 per pixel: 0 outside the outline, ramping to 1 over FEATHER px inside it."""
    a = [0.0] * (W * H)
    for y in range(BOX[1], BOX[3]):
        for x in range(BOX[0], BOX[2]):
            cx, cy = x + 0.5, y + 0.5
            if in_poly(cx, cy, OUTLINE):
                a[y * W + x] = min(1.0, edge_dist(cx, cy, OUTLINE) / FEATHER)
    return a


def rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def build(photo):
    tmp = tempfile.mkdtemp(prefix="hero_")
    canvas, masked, quant = (os.path.join(tmp, n) for n in ("canvas.rgb", "masked.ppm", "quant.rgb"))
    ffmpeg(["-i", photo, "-vf", PLACE, "-f", "rawvideo", "-pix_fmt", "rgb24", canvas])
    px, alpha = read_raw(canvas), mask()
    with open(masked, "wb") as f:
        f.write(b"P6 %d %d 255\n" % (W, H))
        f.write(bytes(int(round(c * a)) for p, a in zip(px, alpha) for c in p))
    ffmpeg(["-i", masked, "-filter_complex",
            "split[a][b];[a]palettegen=max_colors=255:reserve_transparent=0[p];"
            "[b][p]paletteuse=dither=bayer:bayer_scale=4",
            "-f", "rawvideo", "-pix_fmt", "rgb24", quant])
    q = read_raw(quant)
    pal, idx = [], []
    lookup = {}
    for y in range(BOX[1], BOX[3]):
        for x in range(BOX[0], BOX[2]):
            i = y * W + x
            if alpha[i] <= 0.0:
                idx.append(0)
                continue
            c = q[i]
            if c not in lookup:
                pal.append(c)
                lookup[c] = len(pal)            # 1-based; 0 = transparent
            idx.append(lookup[c])
    assert len(pal) <= 255, len(pal)
    return pal, idx


def write_inc(pal, idx):
    w, h = BOX[2] - BOX[0], BOX[3] - BOX[1]
    pal565 = [0] + [rgb565(c) for c in pal] + [0] * (255 - len(pal))
    lines = ["// Generated by tools/gen_hero_sprite.py from the owner's photo 7 (plate blanked). Do not edit.",
             "static const int HERO_X = %d, HERO_Y = %d, HERO_W = %d, HERO_H = %d;" % (BOX[0], BOX[1], w, h),
             "static const uint16_t HERO_PAL[256] = {"]
    for i in range(0, 256, 12):
        lines.append("    " + ", ".join("0x%04X" % v for v in pal565[i:i + 12]) + ",")
    lines += ["};", "static const uint8_t HERO_IDX[%d] = {" % len(idx)]
    for i in range(0, len(idx), 24):
        lines.append("    " + ",".join("%d" % v for v in idx[i:i + 24]) + ",")
    lines += ["};", "// Lamps (screen coordinates, inclusive): headlights first.",
              "static const int16_t HERO_LAMPS[%d][4] = {" % len(LAMPS)]
    lines += ["    {%d, %d, %d, %d}," % r for r in LAMPS]
    lines += ["};", "static const int HERO_HEADLIGHTS = %d;" % HEADLIGHTS]
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("gen_hero_sprite: %s  %dx%d at (%d,%d), %d colours, %d opaque px"
          % (os.path.relpath(OUT), w, h, BOX[0], BOX[1], len(pal), sum(1 for v in idx if v)))


def write_png(pal, idx, path, lamps=True):
    w = BOX[2] - BOX[0]
    img = [[(0, 0, 0)] * W for _ in range(H)]
    for j, v in enumerate(idx):
        if v:
            img[BOX[1] + j // w][BOX[0] + j % w] = pal[v - 1]
    if lamps:
        for x0, y0, x1, y1 in LAMPS:
            for x in range(x0, x1 + 1):
                img[y0][x] = img[y1][x] = (255, 0, 255)
            for y in range(y0, y1 + 1):
                img[y][x0] = img[y][x1] = (255, 0, 255)
    raw = b"".join(b"\x00" + bytes(c for p in row for c in p) for row in img)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    palette, indices = build(sys.argv[1])
    if "--png" in sys.argv:
        write_png(palette, indices, sys.argv[sys.argv.index("--png") + 1])
    else:
        write_inc(palette, indices)
```

- [ ] **Step 4: Generate the sprite and check it**

Run (photo path from Global Constraints): `python tools/gen_hero_sprite.py "<scratchpad>\car_photos\4fd8e581-c70d-4e9d-8ae3-78add1cc188c.jpg"`
Expected: `gen_hero_sprite: src\ui\hero_sprite.inc  262x148 at (32,57), 255 colours, 29006 opaque px`.
Run the same with `--png hero_preview.png` and view it: the car cut out on black, plate a flat grey box, magenta lamp boxes around both headlights and both fog lamps. Delete `hero_preview.png`; make sure no photo or preview is staged (`git status`).

- [ ] **Step 5: Write the module**

`src/ui/hero_sprite.h`:
```cpp
#pragma once
#include <cstdint>
// The boot scene's final shot (tools/gen_hero_sprite.py): the owner's photo 7, cut out on black,
// plate blanked, 255 colours, drawn at screen (X, Y). PURE, host-tested.
namespace hero_sprite {
constexpr int X = 32, Y = 57;                       // screen position of the bounding box
int width();
int height();
// Sprite coordinates. False when transparent (outside the car) or outside the box.
bool pixel(int x, int y, uint16_t* rgb565);
struct Rect { int16_t x0, y0, x1, y1; };            // screen coordinates, inclusive
int lampCount();                                    // 4: headlight L, headlight R, fog L, fog R
Rect lamp(int i);
bool isHeadlight(int i);
}
```

`src/ui/hero_sprite.cpp`:
```cpp
#include "hero_sprite.h"

namespace hero_sprite {

#include "hero_sprite.inc"

static_assert(HERO_X == X && HERO_Y == Y, "hero_sprite.inc moved: update hero_sprite::X/Y");

int width() { return HERO_W; }
int height() { return HERO_H; }

bool pixel(int x, int y, uint16_t* rgb565) {
    if (x < 0 || y < 0 || x >= HERO_W || y >= HERO_H) return false;
    const uint8_t i = HERO_IDX[y * HERO_W + x];
    if (!i) return false;
    if (rgb565) *rgb565 = HERO_PAL[i];
    return true;
}

int lampCount() { return (int)(sizeof HERO_LAMPS / sizeof HERO_LAMPS[0]); }

Rect lamp(int i) { return {HERO_LAMPS[i][0], HERO_LAMPS[i][1], HERO_LAMPS[i][2], HERO_LAMPS[i][3]}; }

bool isHeadlight(int i) { return i < HERO_HEADLIGHTS; }

}
```

- [ ] **Step 6: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_hero_sprite\test_hero_sprite.cpp src\ui\hero_sprite.cpp`
Expected: `5 Tests 0 Failures 0 Ignored`

- [ ] **Step 7: Commit**

```bash
git add tools/gen_hero_sprite.py src/ui/hero_sprite.h src/ui/hero_sprite.cpp src/ui/hero_sprite.inc test/test_hero_sprite
git commit -m "feat(ui): hero sprite from the owner's photo (cut-out, plate blanked)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Scene v2 (`ui/bootscene`)

Firmware-only; its maths is tested in Tasks 1–3. Verified by building and by the owner watching on the deck.

**Files:**
- Modify: `src/ui/bootscene.cpp` (full replacement below; `bootscene.h` unchanged)

**Interfaces:**
- Consumes: Task 1's `bootanim` ending functions; `car_sprite::*`; `hero_sprite::{X, Y, width, height, pixel, lampCount, lamp, isHeadlight, Rect}`; `greeting::durationMs/level`; `theme::be`; `mem::log`.
- Produces: unchanged `bootscene::start/setCaption/active/waitDone`.

- [ ] **Step 1: Replace `src/ui/bootscene.cpp`**

```cpp
#include "bootscene.h"
#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include "car_sprite.h"
#include "hero_sprite.h"
#include "theme.h"
#include "../audio/greeting.h"
#include "../core/mem.h"
#include "../util/bootanim.h"

namespace bootscene {
namespace {

using bootanim::HeroBeat;
using bootanim::hex565;

constexpr int W = 320, H = 240;
constexpr int SKY_H = 96;                        // static sky while cruising, drawn once
constexpr int STRIP_H = 24;
constexpr int STRIPS = (H - SKY_H) / STRIP_H;    // cruising: y 96-239
constexpr int ALL_STRIPS = H / STRIP_H;          // brake fade: whole screen
constexpr int HERO_TOP = 48;                     // hero beats: y 48-239
constexpr int HERO_STRIPS = (H - HERO_TOP) / STRIP_H;
constexpr int GROUND_Y = 214;                    // the car's ground line
constexpr int CAR_TOP = GROUND_Y - 65;           // sprite row 0 (without the bob)
constexpr uint32_t FRAME_MS = 40;                // 25 fps target
constexpr int ROAD_SPEED = 90, SKYLINE_SPEED = 14;
constexpr int LAMP_PERIOD = 160, DASH_PERIOD = 48, SKYLINE_PERIOD = 720;
constexpr float TAU = 6.28318530718f;            // Arduino.h #defines TWO_PI
constexpr uint16_t UNDERGLOW = hex565(0x8C46FF); // the owner's purple underglow
constexpr uint16_t WHITE = 0xFFFF;
constexpr int NAME_Y = 222, NAME_H = 16;         // "HONDA CITY", font 2
// Review Focus 4: the name must fall inside one hero strip (it is drawn into a single strip).
static_assert((NAME_Y - HERO_TOP) / STRIP_H == (NAME_Y + NAME_H - 1 - HERO_TOP) / STRIP_H,
              "HONDA CITY straddles two hero strips");

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

void dimStrip(uint8_t level) {
    if (level == 255) return;
    for (int i = 0; i < W * STRIP_H; ++i) s_buf[i] = theme::be(bootanim::scale565(theme::be(s_buf[i]), level));
}

// ---- sky ----
struct Star { int x, y; uint16_t c; };
Star star(int i) {
    return {(i * 73) % W, (i * 37) % SKY_H, i % 3 ? hex565(0xCFD8FF) : hex565(0xFFFFFF)};
}
bool starSkipped(const Star& s) {
    return (s.y < 24 && s.x < 220) || (s.x >= 264 && s.x <= 288 && s.y >= 12 && s.y <= 36);   // caption, moon
}
uint16_t bandAt(int y) { return SKY_BANDS[y / 30 < 3 ? y / 30 : 3]; }

// Moon pixel at offset (dx, dy) from its centre (276, 24); false outside the disc.
bool moonPixel(int dx, int dy, uint16_t* c) {
    const int r2 = dx * dx + dy * dy;
    if (r2 > 100) return false;
    const bool shadow = (dx + 4) * (dx + 4) + (dy + 3) * (dy + 3) > 100;
    *c = shadow ? hex565(0xC9C49E) : (r2 < 30 && dx > -2) ? hex565(0xFFFBE8) : hex565(0xF4F1D0);
    if ((dx >= 3 && dx <= 4 && dy >= -3 && dy <= -2) || (dx >= -3 && dx <= -1 && dy >= 4 && dy <= 5))
        *c = hex565(0xDCD7B0);                                                        // craters
    return true;
}

// Cruising: sky straight on the display, once (and again when the caption changes).
void drawSky(const char* caption) {
    for (int i = 0; i < 4; ++i) s_tft->fillRect(0, i * 30, W, i < 3 ? 30 : SKY_H - 90, SKY_BANDS[i]);
    for (int i = 0; i < 40; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, s.c);
    }
    uint16_t c;
    for (int dy = -10; dy <= 10; ++dy)
        for (int dx = -10; dx <= 10; ++dx)
            if (moonPixel(dx, dy, &c)) s_tft->drawPixel(276 + dx, 24 + dy, c);
    s_tft->setTextColor(hex565(0xCFD8FF), SKY_BANDS[0]);
    s_tft->drawString(caption, 6, 6, 2);
}

void twinkle(uint32_t t) {
    for (int i = 0; i < 40; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, bootanim::starHidden(t, i) ? bandAt(s.y) : s.c);
    }
}

// Brake fade: the sky rendered into the strip (no caption) so it can be dimmed with the rest.
void drawSkyIntoStrip(uint32_t t) {
    for (int i = 0; i < 4; ++i) rect(0, i * 30, W, i < 3 ? 30 : SKY_H - 90, SKY_BANDS[i]);
    for (int i = 0; i < 40; ++i) {
        const Star s = star(i);
        if (!starSkipped(s) && !bootanim::starHidden(t, i)) rect(s.x, s.y, 1, 1, s.c);
    }
    uint16_t c;
    for (int dy = -10; dy <= 10; ++dy)
        for (int dx = -10; dx <= 10; ++dx)
            if (moonPixel(dx, dy, &c)) rect(276 + dx, 24 + dy, 1, 1, c);
}

// ---- road world ----
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

void wheel(int cx, int cy, float base) {
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
                if (d >= 2.5f) {                             // 5 twin spokes, two-tone
                    const float a = atan2f((float)dy, (float)dx) - base;
                    float best = 9.0f;
                    for (int k = 0; k < 5; ++k)
                        for (float off : {-0.17f, 0.17f})
                            best = fminf(best, fabsf(remainderf(a - (k * TAU / 5 + off), TAU)));
                    if (best < 0.10f) c = hex565(0xDFE3E8);          // machined face
                    else if (best < 0.18f) c = hex565(0x6A7078);     // dark edge (owner's alloys)
                }
            }
            put(x, y, c);
        }
    }
}

struct Car { uint32_t t; int x; int top; uint8_t e; bool lamp; int lampX; float spoke; };

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

// `world` = the clock the road scrolls and the wheels turn with (slows while braking).
Car makeCar(uint32_t world, int x, int bob, uint8_t e) {
    Car c{};
    c.t = world;
    c.x = x;
    c.top = CAR_TOP + bob;
    c.e = e;
    c.spoke = bootanim::spokeAngle(world);
    c.lamp = nearestLamp(world, x, &c.lampX);
    return c;
}

void drawCar(const Car& c) {
    rect(c.x + 24, GROUND_Y, 154, 2, hex565(0x121420));              // shadow
    const uint8_t ga = bootanim::glowAlpha(c.e);                       // purple underglow
    blendRect(c.x + 66, GROUND_Y - 10, 76, 2, UNDERGLOW, ga);
    for (int i = 0; i < 8; ++i)
        blendRect(c.x + 30 - 3 * i, GROUND_Y - 7 + i, 140 + 6 * i, 1, UNDERGLOW, (uint8_t)(ga * (60 - 7 * i) / 100));
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
    wheel(c.x + car_sprite::REAR_WHEEL_X, wy, c.spoke);
    wheel(c.x + car_sprite::FRONT_WHEEL_X, wy, c.spoke);
}

// ---- hero (photo) ----
bool inRect(const hero_sprite::Rect& r, int x, int y) { return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1; }

void drawHero(uint32_t u, uint8_t e) {
    rect(0, s_y0, W, STRIP_H, 0x0000);
    const uint8_t ga = bootanim::glowAlpha(e);                          // underglow pool under the hero
    for (int y = s_y0; y < s_y0 + STRIP_H; ++y) {
        const float dy = (float)(y - 206) / 10.0f;
        if (dy * dy >= 1.0f) continue;
        for (int x = 42; x < 282; ++x) {
            const float dx = (float)(x - 162) / 120.0f, d = dx * dx + dy * dy;
            if (d < 1.0f) put(x, y, bootanim::blend565(UNDERGLOW, get(x, y), (uint8_t)(ga * 0.6f * (1.0f - d))));
        }
    }
    if (bootanim::heroBeat(u) == HeroBeat::Glow) return;               // the car is still dark
    const bool lamps = bootanim::lampsOn(u);
    int x0 = hero_sprite::X, x1 = hero_sprite::X + hero_sprite::width();
    int y0 = hero_sprite::Y, y1 = hero_sprite::Y + hero_sprite::height();
    if (bootanim::clip(x0, x1, 0, W) && bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                uint16_t c;
                if (!hero_sprite::pixel(x - hero_sprite::X, y - hero_sprite::Y, &c)) continue;
                c = bootanim::scale565(c, bootanim::heroLevel(u, x, y));
                if (lamps)
                    for (int i = 0; i < hero_sprite::lampCount(); ++i)
                        if (inRect(hero_sprite::lamp(i), x, y)) {
                            c = bootanim::blend565(WHITE, c, 200);
                            break;
                        }
                put(x, y, c);
            }
    }
    if (!lamps) return;
    for (int i = 0; i < hero_sprite::lampCount(); ++i) {               // lens flares on the headlights
        if (!hero_sprite::isHeadlight(i)) continue;
        const hero_sprite::Rect r = hero_sprite::lamp(i);
        const int cx = (r.x0 + r.x1) / 2, cy = (r.y0 + r.y1) / 2;
        for (int dx = -30; dx <= 30; ++dx) blendRect(cx + dx, cy, 1, 1, WHITE, (uint8_t)(220 * (30 - abs(dx)) / 30));
        for (int dy = -6; dy <= 6; ++dy)
            for (int dx = -6; dx <= 6; ++dx) {
                const float rr = sqrtf((float)(dx * dx + dy * dy));
                if (rr < 6.0f) blendRect(cx + dx, cy + dy, 1, 1, WHITE, (uint8_t)(160.0f * (1.0f - rr / 6.0f)));
            }
    }
}

void drawName(TFT_eSprite& spr, uint32_t u) {
    const uint8_t lvl = bootanim::nameLevel(u);
    if (!lvl || NAME_Y < s_y0 || NAME_Y + NAME_H > s_y0 + STRIP_H) return;
    spr.setTextColor(bootanim::scale565(hex565(0xDCE0E6), lvl));       // one argument: transparent background
    spr.setTextDatum(TC_DATUM);
    spr.drawString("H O N D A   C I T Y", W / 2, NAME_Y - s_y0, 2);
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
        uint32_t tHero = bootanim::NO_EXIT;
        int xc = 0;
        bool black = false;
        uint32_t frames[3] = {0, 0, 0}, busyMs[3] = {0, 0, 0};   // drive, brake, hero
        TickType_t wake = xTaskGetTickCount();
        for (;;) {
            const uint32_t t = millis() - t0;
            if (tHero == bootanim::NO_EXIT && bootanim::mayExit(t, t >= greeting::durationMs(), s_online())) {
                tHero = t;
                xc = bootanim::cruiseX(t);
            }
            const uint32_t u = tHero == bootanim::NO_EXIT ? 0 : t - tHero;
            const HeroBeat beat = tHero == bootanim::NO_EXIT ? HeroBeat::Brake : bootanim::heroBeat(u);
            if (tHero != bootanim::NO_EXIT && beat == HeroBeat::Done) break;
            const uint32_t f0 = millis();
            const uint8_t e = greeting::level(t);
            int mode;
            if (tHero == bootanim::NO_EXIT) {                    // enter + cruise (v1)
                mode = 0;
                if (gen != s_captionGen) {
                    gen = s_captionGen;
                    drawSky(s_caption);
                }
                twinkle(t);
                const Car car = makeCar(t, bootanim::carX(t, bootanim::NO_EXIT), bootanim::bob(t), e);
                for (int s = 0; s < STRIPS; ++s) {
                    s_y0 = SKY_H + s * STRIP_H;
                    drawBackground(t);
                    drawCar(car);
                    spr.pushSprite(0, s_y0);
                }
            } else if (beat == HeroBeat::Brake) {                // brake + fade the whole screen
                mode = 1;
                const uint32_t world = bootanim::brakeTime(tHero, u);
                const Car car = makeCar(world, bootanim::brakeX(xc, u), 0, e);
                const uint8_t lvl = bootanim::brakeLevel(u);
                for (int s = 0; s < ALL_STRIPS; ++s) {
                    s_y0 = s * STRIP_H;
                    if (s_y0 < SKY_H) drawSkyIntoStrip(world);
                    drawBackground(world);
                    drawCar(car);
                    dimStrip(lvl);
                    spr.pushSprite(0, s_y0);
                }
            } else {                                             // underglow, sweep, reveal, flash, hold
                mode = 2;
                if (!black) {
                    s_tft->fillScreen(TFT_BLACK);
                    black = true;
                }
                for (int s = 0; s < HERO_STRIPS; ++s) {
                    s_y0 = HERO_TOP + s * STRIP_H;
                    drawHero(u, e);
                    drawName(spr, u);
                    spr.pushSprite(0, s_y0);
                }
            }
            busyMs[mode] += millis() - f0;
            ++frames[mode];
            vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS));
        }
        spr.deleteSprite();
        s_buf = nullptr;
        Serial.printf("[bootscene] %u ms; render avg drive %u ms (%u frames), brake %u ms (%u), hero %u ms (%u); stack free %u\n",
                      (unsigned)(millis() - t0),
                      (unsigned)(frames[0] ? busyMs[0] / frames[0] : 0), (unsigned)frames[0],
                      (unsigned)(frames[1] ? busyMs[1] / frames[1] : 0), (unsigned)frames[1],
                      (unsigned)(frames[2] ? busyMs[2] / frames[2] : 0), (unsigned)frames[2],
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

- [ ] **Step 2: Build**

Run: `pio run -e esp32dev` → `SUCCESS`, no warnings from `src/ui/bootscene.cpp`, `src/ui/hero_sprite.cpp`, `src/util/bootanim.cpp`. Run `pio run -e hwcheck` → `SUCCESS`.

- [ ] **Step 3: Host tests**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` → `suites pass=29 fail=0`.

- [ ] **Step 4: Commit**

```bash
git add src/ui/bootscene.cpp
git commit -m "feat(ui): boot scene v2 - purple underglow, brake to black, photo hero ending

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5: On the deck, with the owner watching (Review Focus 5)**

Flash: `pio run -e esp32dev -t upload --upload-port COM11`; reset and capture serial for 25 s.
Expected serial: `[bootscene] T ms; render avg drive ~54 ms (...), brake R1 ms (...), hero R2 ms (...); stack free S` with T ≈ WiFi-ready time + 3300, **R2 ≤ 40**, **S ≥ 512**; then `[mem] bootscene end` and Spotify starting.
Ask the owner to confirm, in order: refined car (dark windows, lit fog lamp), purple underglow; brake with road and sky fading to black; purple glow alone; light sweep across the dim photo; reveal; headlight double-flash with flares (nothing smeared at the right edge); "HONDA CITY"; ~1.2 s hold; then the deck. If R2 > 40 or S < 512: stop and report.

---

### Task 5: Docs

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-10-09-boot-scene-v2-design.md`, `docs/superpowers/specs/2026-10-09-boot-scene-design.md`

- [ ] **Step 1: README**

In `README.md`, replace the whole `### Boot scene` section body (the paragraph under the heading) with:
```markdown
While the greeting plays and WiFi connects, the screen shows a night drive: a silver Honda City
(details from the owner's photos: tinted windows, LED fog lamps, purple underglow) drives in and
cruises for at least 5 s and until WiFi is up. Then it brakes while the world fades to black,
only the underglow is left, a light sweep reveals a photo of the owner's car, the headlights
double-flash and "HONDA CITY" fades in. If no WiFi is found the car keeps cruising ("WiFi not
found - retrying...") until the hotspot appears. The pixel car comes from
`python tools/gen_car_sprite.py`, the photo from `python tools/gen_hero_sprite.py <photo.jpg>`
(plate blanked; the photo itself is never committed). Both take `--png out.png` for a preview.
```
In the Architecture list, replace the `- **Boot scene**` entry with:
```markdown
- **Boot scene** (`ui/bootscene`, `ui/car_sprite`, `ui/hero_sprite`, `util/bootanim`,
  `util/loudness`) — own task at boot; cruising: sky drawn once, y 96–239 re-rendered (~18 fps)
  through one 320×24 strip (15 KB, freed at the end); ending: whole-screen brake fade, then the
  photo hero (y 48–239). `setup()` waits for it; `loop()` and the late-WiFi path wait while active.
```
and add `hero_sprite` after `car_sprite` in the "Pure, host-tested modules" `ui/{...}` list.

- [ ] **Step 2: Spec status lines**

In `docs/superpowers/specs/2026-10-09-boot-scene-v2-design.md` set the status line to `Status: implemented (feat/boot-scene, ` + the short hashes of this plan's Task 1–4 commits from `git log --oneline -6`, comma-separated + `).`
In `docs/superpowers/specs/2026-10-09-boot-scene-design.md` set the status line to `Status: implemented (feat/boot-scene); the Exit (drive-off) phase is superseded by 2026-10-09-boot-scene-v2-design.md.`

- [ ] **Step 3: Commit**

```bash
git add README.md docs/superpowers/specs/2026-10-09-boot-scene-design.md docs/superpowers/specs/2026-10-09-boot-scene-v2-design.md
git commit -m "docs: boot scene v2 in README, specs implemented

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
