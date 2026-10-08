# Night mode: design

Date: 2026-10-08. Status: written-spec review.

## Goal

The day palette (cream boxes, light sky, white dialogue box) and the full backlight glare when
driving at night. From 18:00 to 06:00 Manaus time (UTC-4, no daylight saving) the deck switches
by itself to a dark palette and a dimmer backlight, then back to today's look.

## Decisions

- Night mode changes **both** the palette and the backlight (album art and sprites can only be
  dimmed by the backlight). Day mode keeps full brightness (the auto-dim from the light sensor
  stays declined).
- Night = **18:00 to 06:00**, fixed times: Manaus sunset/sunrise move only a few minutes over
  the year.
- Palette **A, "Moonlit battle"** (mockup chosen 2026-10-08): navy sky, dark green field, slate
  boxes, cream text.
- Built as **two named palettes** (not mutable colour globals, not an automatic darkening).

## Palette (`src/ui/theme`)

Mode-dependent colours move into a struct; drawing code reads the active one.

```cpp
struct Palette {
    uint16_t top, topText, iconOff, sky, horizon, grass;
    uint16_t boxFill, boxBorder, boxShadow, text, textShadow;
    uint16_t dlgFrame, dlgLine, dlgFill, dlgShadow;
};
extern const Palette DAY, NIGHT;
const Palette& pal();          // the active palette (DAY at boot)
void setNight(bool night);
bool isNightActive();
```

| Field | DAY (today, unchanged) | NIGHT (Moonlit) |
|---|---|---|
| top | #283038 | #0E1216 |
| topText (strip text, "on" icons) | #F8F8D8 | #C8C4B0 |
| iconOff | #808890 | #5A6470 |
| sky | #A8D8F8 | #18284A |
| horizon | #60A858 | #1E3A2A |
| grass | #88C878 | #1F3B2B |
| boxFill | #F8F8D8 | #2A3240 |
| boxBorder | #404848 | #0E1218 |
| boxShadow | #587060 | #0A0E14 |
| text | #404040 | #E0DCC8 |
| textShadow | #D8D0B0 | #101418 |
| dlgFrame | #284860 | #101C28 |
| dlgLine | #68A0B8 | #3A5A70 |
| dlgFill | #F8F8F8 | #1C2430 |
| dlgShadow | #D0D0D0 | #0A0E14 |

- The top-strip text and "on" icons use `BOX_FILL` today; they move to `topText`, so at night
  they stay light on the dark strip instead of turning slate.
- Unchanged in both modes (they carry meaning): HP colours and `HP_EMPTY`, `HP_TAG*`,
  `EXP_BLUE`, `CD_SILVER`, type colours, genre badge colours, `GBA_NAVY`, `POKE_RED`.
- Everything drawn over a box (walker sprite, Poke Ball, note and device icons, cleared
  rectangles) takes its background from `pal()` at draw time, so nothing stays light. No colour
  may be cached in a `static` across a mode change.
- Only the UI loop (core 1) reads or sets the active palette: no lock needed.
- Dark badges (RAP #2A2A2E, R&B #6B4226) have little edge contrast on the night `boxFill`;
  check on the panel. If needed, at night only, the badge border becomes `iconOff` instead of
  `darken(colour)`.

## Clock (`src/net/clock`, new)

- `clock::begin()` after WiFi is up: `configTime(-4 * 3600, 0, "pool.ntp.org",
  "time.google.com")`. SNTP runs in the background (UDP, re-syncs about hourly); no TLS, no
  measurable heap.
- `bool clock::minuteOfDay(int* m)`: false until the time is synced (year < 2024), else the
  local minute 0..1439.
- The offset is a constant in `clock.cpp` (`UTC_OFFSET_S = -4 * 3600`, Manaus).

## Day or night (`src/util/daynight`, new, pure)

```cpp
namespace daynight {
constexpr int NIGHT_START = 18 * 60;   // 18:00
constexpr int NIGHT_END   = 6 * 60;    // 06:00
bool isNight(int minuteOfDay);         // [NIGHT_START, 1440) or [0, NIGHT_END)
}
```

## Switching (`src/main.cpp`)

- The UI loop checks every 10 s (`MODE_CHECK_MS`). No synced time -> day.
- On a change: `theme::setNight`, set the backlight, full redraw of the current screen
  (the same path as returning from a status screen: `drawNow`, art push, dialogue, status
  screens redraw with the new palette too). Log `[ui] night mode on/off`.
- Backlight: LEDC PWM on GPIO 21 (TFT_BL), 5 kHz, 8-bit, attached after `tft.init()`.
  `BACKLIGHT_DAY = 255`, `BACKLIGHT_NIGHT = 90` (~35 %), constants tuned on the panel.
- WiFi never up: the clock never syncs, the deck stays in day mode.

## Testing

Host (Unity):
- `test_daynight`: 17:59 day, 18:00 night, 23:59 night, 00:00 night, 05:59 night, 06:00 day,
  12:00 day.
- `test_theme`: `DAY` equals today's constants field by field; `NIGHT` text vs `boxFill`
  and vs `dlgFill` contrast >= 4.5:1; `setNight`/`pal()` switch and switch back.

On the board:
- A temporary build flag forces night mode to review it by day (not committed).
- After 18:00: switch happens within 10 s, backlight dims, no light patches around the walker,
  Poke Ball or icons; same `[mem]` largest block as before.

## Non-goals

- Sunrise/sunset computation, a configurable time zone, a manual day/night toggle (no touch
  controls), light-sensor dimming.
