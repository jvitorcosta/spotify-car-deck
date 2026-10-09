# Boot scene v2 (photo-informed car, purple underglow, photo hero ending): design

Date: 2026-10-09. Status: partly implemented; the photo-hero ending (sections 3-4) was replaced by v3 (2026-10-09-boot-scene-v3-design.md).
`2026-10-09-boot-scene-design.md` (v1, implemented on `feat/boot-scene`); everything not changed
here stays as v1 describes it.

## Goal

Make the boot scene look like the owner's car and end it the way the Volkswagen start-up does:
the pixel-art City cruises on the night road as today, then, once the deck is ready, it brakes
while the world fades to black, only its purple underglow is left, a light sweep reveals a
**photo of the owner's car** (front three-quarter, owner's photo 7), the headlights double-flash
and "HONDA CITY" fades in.

## Decisions (owner, 2026-10-09)

- The 11 owner photos (night, car park, front / front-3/4 only, no side view) inform the pixel
  car's details; they can't change its side silhouette.
- Body is **all silver** (the dark roof/hood in the photos were reflections); spoiler stays body
  colour.
- Underglow: **fixed purple**, as on the car (replaces RGB cycling); still brightens with the
  sound.
- Ending: **hybrid pixel → photo**, hero = **photo 7**, wordmark **"HONDA CITY"**, **plate
  blanked** (the plate never enters the firmware or the repo).
- Extra time is fine: the hero holds 1.2 s.
- The photos zip stays out of git (`Photos-*.zip` in `.gitignore`).

## 1. Pixel car refinements (`tools/gen_car_sprite.py`)

Size, outline and pixel kinds unchanged (200 × 66). Changes, all seen in the photos:

| Detail | v1 | v2 |
|---|---|---|
| Window tint | glass #16233A, reflections #4F6D92 | near-black glass #0C1016, reflections #2C3A4A |
| Headlight | white DRL along the bottom edge | white DRL (`HEAD`) along the **top** edge: x 166–197, y = round(26.0 + (x − 166) × 0.135); LED projector pairs under it every 3 px from x 180: y = round(28.6 + (x − 180) × 0.16); no bottom DRL, no separate chrome top line |
| Front bumper corner | black/dark-checkered intake | intake area filled #2C3038, with a **lit fog lamp** (`HEAD`): disc r < 2.4 #FFFFFF and ring r < 3.4 #CFE8FF centred (192.5, 46) |
| Door handles | chrome #E8ECEF over #2C3038 | silver: #EEF2F5 (`UPPER`) over #7C858F |

Wheels (drawn in `ui/bootscene`): the spoke edge colour becomes #6A7078 (was #AAB0B8) for the
two-tone machined look of the owner's alloys; faces stay #DFE3E8.

## 2. Purple underglow

`bootanim::hue565()` is no longer used by the scene; the underglow colour is the constant
#8C46FF. The alpha mapping `glowAlpha(e)` is unchanged (brightens with the sound). The hero
ending uses the same colour and alpha.

## 3. Ending: brake and photo hero (replaces the v1 Exit phase)

Exit gating is unchanged: the ending starts at `tHero`, the first frame where
`mayExit(t, soundDone, online)` holds (t ≥ 5000 ms, greeting finished, WiFi up). The scene ends
at `tHero + 3300`. With `u = t − tHero`:

| Beat | u (ms) | Screen |
|---|---|---|
| Brake + dim | 0 – 600 | Car decelerates: x = xc + round(40 × (1 − (1 − q)²)), q = u / 600, xc = cruise x at tHero (it rolls 40 px and stops). Whole frame (sky included) scaled by brightness 1 − q; wheels slow with it (spoke angle advances by 9 rad/s × (1 − q)). |
| Underglow | 600 – 900 | Black screen; only the purple underglow pool at the hero position. |
| Light sweep | 900 – 1400 | Hero photo at 10 % brightness; a diagonal light band crosses it left to right (see below). |
| Reveal | 1400 – 1700 | Hero brightness 10 % → 100 % (linear). |
| Flash + name | 1700 – 2100 | Headlights and fog lamps flash: on 1700–1800, off 1800–1900, on 1900–2100; lens flare on the headlights while on; "HONDA CITY" fades in. |
| Hold | 2100 – 3300 | Hero at 100 %, lamps on, name shown. |

The underglow pool shows in every beat from 600 on; its alpha follows `glowAlpha(e)` with the
current greeting loudness (usually 0 by then: resting glow).

**Light sweep**: band centre `s = −80 + 480 × q` (q = (u − 900) / 500) along the diagonal
coordinate `d = x + y / 2`; pixels with |d − s| < 30 get brightness 10 % + 90 % × (1 − |d − s| / 30)
(max with the base 10 %).

**Flash**: pixels inside the four lamp rectangles are blended toward white with alpha 200 while
on. Each headlight gets a flare while on: additive white streak 1 px tall, 60 px wide, centred
on the lamp centre, alpha falling linearly from 220 at the centre to 0, plus a 6 px radial glow
(alpha 160 at the centre).

**Name**: "HONDA CITY", TFT_eSPI font 2, letters spaced (`"H O N D A   C I T Y"`), centred at
x 160, y 222, colour #DCE0E6 scaled by `nameLevel` (0 → 1 over 1700–2100).

### Pure maths (`util/bootanim`, host-tested)

```cpp
constexpr uint32_t BRAKE_MS = 600, GLOW_MS = 300, SWEEP_MS = 500, REVEAL_MS = 300,
                   FLASH_MS = 400, HOLD_MS = 1200;   // HERO_MS = 3300
enum class HeroBeat : uint8_t { Brake, Glow, Sweep, Reveal, Flash, Hold, Done };
HeroBeat heroBeat(uint32_t u);                  // u = t - tHero
float heroProgress(uint32_t u);                 // 0..1 within the current beat
int brakeX(int xc, uint32_t u);                 // car x during Brake
uint8_t brakeLevel(uint32_t u);                 // frame brightness 255 -> 0 during Brake
uint8_t heroLevel(uint32_t u, int x, int y);    // hero brightness 0..255 (Sweep band, Reveal, Flash/Hold = 255)
bool lampsOn(uint32_t u);                       // Flash pattern, true during Hold
uint8_t nameLevel(uint32_t u);                  // 0..255
uint32_t heroDoneAt(uint32_t tHero);            // tHero + 3300
```

`exitDone` and `carX`'s exit branch stay (tested, small) but the scene no longer uses them.

## 4. Hero image (`tools/gen_hero_sprite.py` → `src/ui/hero_sprite.inc`, `ui/hero_sprite`)

Generator (standard library + ffmpeg), run by hand with the owner's photo 7:
`python tools/gen_hero_sprite.py <path to 4fd8e581-c70d-4e9d-8ae3-78add1cc188c.jpg>`.

1. ffmpeg: scale the photo to 270 px wide (area filter), place it on a 320 × 240 black canvas so
   the car sits as in the mockup (pad to 320 × (h + 200) at x 25 / y 100, crop 320 × 240 at y 224),
   and **blank the plate** with a filled box x 221, y 158, 42 × 19, colour #B8BCC0.
2. Rasterise the car outline polygon (screen coordinates):
   (35,160) (32,140) (33,117) (37,97) (45,80) (57,66) (70,60) (100,57) (186,57) (205,72)
   (220,83) (233,92) (267,110) (282,121) (288,127) (290,143) (293,173) (283,182) (260,190)
   (200,193) (140,197) (120,204) (100,197) (97,182) (67,168) (50,166),
   feathered 3 px into black.
3. ffmpeg: multiply by the mask, `palettegen=max_colors=255`, `paletteuse` (bayer dither, scale 4),
   raw rgb24 out.
4. Crop to the outline's bounding box (x 32–293, y 57–204 → 262 × 148), map pixels to palette
   indices 1–255 (index 0 = transparent: outside the polygon), write `hero_sprite.inc`:
   `HERO_X = 32, HERO_Y = 57, HERO_W = 262, HERO_H = 148`, `HERO_PAL[256]` (RGB565, [0] unused),
   `HERO_IDX[HERO_W * HERO_H]`, and the lamp rectangles (screen coordinates; verify on the
   generated image and correct by a few px if needed):
   headlight L x 132–172, y 122–138; headlight R x 270–290, y 124–138;
   fog L x 146–165, y 163–177; fog R x 284–292, y 152–162.
5. `--png out.png` writes a preview instead.

The committed `.inc` contains the blanked image only (≈ 39 KB indices + 0.5 KB palette). The
photo and the zip stay outside git.

Module API:

```cpp
namespace hero_sprite {
constexpr int X = 32, Y = 57;                       // screen position of the bounding box
int width();                                        // 262
int height();                                       // 148
bool pixel(int x, int y, uint16_t* rgb565);         // sprite coords; false = transparent
struct Rect { int16_t x0, y0, x1, y1; };            // screen coords, inclusive
int lampCount();                                    // 4
Rect lamp(int i);                                   // 0,1 headlights; 2,3 fog lamps
bool isHeadlight(int i);
}
```

## 5. Scene rendering changes (`ui/bootscene`)

- Before `tHero`: as v1 (enter, cruise), with the refined sprite, purple underglow and two-tone
  wheel edges.
- **Brake** (0–600 ms): the whole screen is rendered in strips (10 strips of 24 rows, sky
  included, stars and moon drawn into the strips), every pixel scaled by `brakeLevel`. Slower
  (~54+ ms/frame) is fine for a fade.
- At the end of Brake: one `fillScreen(TFT_BLACK)`.
- **Glow → Hold**: only rows 48–239 are rendered (8 strips): black, underglow pool under the hero
  (ellipse centred x 162, y 206, semi-axes 120 × 10, colour #8C46FF, alpha as v1's pool scaled by
  `glowAlpha(e)`), hero pixels scaled by `heroLevel`, lamp blend + flares when `lampsOn`, the
  name drawn into the strip with the sprite's `drawString` in its strip-relative position.
- Memory and task model unchanged (same 15 KB strip, freed at the end).

## 6. Tests

- `test_bootanim`: beat boundaries (0, 599, 600, 900, 1400, 1700, 2100, 3299, 3300); brakeX at
  q 0 / 0.5 / 1; brakeLevel 255 → 0; heroLevel in the sweep band vs outside, reveal endpoints,
  255 in Hold; lampsOn pattern (1750 on, 1850 off, 1950 on, Hold on, Sweep off); nameLevel
  endpoints; heroDoneAt.
- `test_car_sprite`: updated values — DRL `HEAD` pixel on the top edge (e.g. (180, 28)); fog lamp
  `HEAD` white at (192, 46); glass colour #0C1016 at a window pixel; handle `UPPER` #EEF2F5.
- `test_hero_sprite`: size 262 × 148; transparent top-left corner; an opaque body pixel; 4 lamps,
  2 headlights; all pixels inside the blanked plate box share one colour.
- On the deck: the full sequence with the owner watching (drive, brake fade, underglow, sweep,
  reveal, flash, name, hold), then the deck; frame time logged per phase; `[mem] bootscene end`
  before Spotify starts.

## Out of scope

- Photo-derived side sprite (needs a side photo), photo 11 / multi-photo sequences, a day
  version, skipping by touch.
