# Boot scene v3 (photo-matched car park, reshaped City, photo montage ending): design

Date: 2026-10-09. Status: implemented (feat/boot-scene, d08cc00, a19e53e, 0b79cee, 22edf66, 585005e, 08227e2, 1c11484).
(`2026-10-09-boot-scene-design.md`) and replaces parts of v2 (`2026-10-09-boot-scene-v2-design.md`):
v2's car-detail refinements and purple underglow stay; v2's photo-hero ending (cut-out, light
sweep, flash, "HONDA CITY") is **dropped** and replaced by a photo montage.

## Goal

The boot scene should feel like the owner's own photos: the pixel City cruises through a dark
car park lit by white LED street lamps, its purple underglow glowing on near-black asphalt; once
the deck is ready it brakes and fades to black, then a short montage of the owner's photos plays
(the camera coming round the car and ending on the headlight), and the deck starts.

## Decisions (owner, 2026-10-09)

- **Pixel car reshaped** from a true side-profile reference (Wikimedia Commons,
  "2024 Honda City RS - Sideview", CC BY-SA 4.0, same body as the owner's car).
- **Background like the owner's photos**: near-black sky, tree silhouettes, green fence, distant
  parked cars, tall white LED lamps; **near-black road** so the purple underglow shows detail.
- **Montage** (shot on the deck in the trial): owner photos **07 → 02 → 04 → 06** from
  `montage-photos/` (07 opens because its car faces right, like the pixel car), full-size frames
  (never shrunk, no blurred margins), car centred, slow push-in, 0.3 s crossfades, fade in from
  black on 07, 06 held and faded out. Plates visible.
- The repo is public: montage frames are **built locally into a git-ignored file**; without it
  (fresh clone, CI) the scene ends after the brake fade.

## 1. Pixel car (`tools/gen_car_sprite.py`)

Side reference measured at the sprite scale (200 px = 4549 mm), mirrored to face right:

| Feature | v2 | v3 |
|---|---|---|
| Body outline, rear | trunk deck y ≈ 19–21, rear window from (26,19) | higher, shorter deck: (1,22) (3,18) (20,16.4) (24,16), flatter rear window (40,9.6) (56,4.2) (70,1.9) (86,1.0) |
| Beltline (window bottom) | flat, y 21 | rises toward the rear: `belt_y(x) = 20.0 − (139 − x) × 0.053`; paint above it is the upper band |
| Side glass | (48,20.6) … (139,20.3) | (44,15.0) (56,6.6) (72,3.6) (86,3.0) (100,3.0) (111,3.8) (119,6.0) (133,15.2) (139,19.9) |
| B-pillar | x 96–99 | x 86–89 (to the beltline) |
| Quarter-glass divider | x ≈ 60–63 | x = round(56 − (y − 5) × 0.2), y 5–16 |
| Chrome belt strip | y ≈ 21 | follows `belt_y(x) + 0.6`, x 45–138 |
| Door shut lines | 141, 98, rear door edge 64 | 141, **88**, rear door rear edge **x 43** (y 16–33) curving onto the rear arch (x 43 + (y − 34) × 1.2, y 34–39) |
| Door handles | x 82 and 122 | x **97** and **51** (near each door's rear edge), silver on a shade line |
| Fuel door | x 36–42, y 28–33 | x 32–38, y 19–24 (above the shoulder line) |
| Shoulder line | 27 − (x − 16) × 0.006 | 26.6 − (x − 16) × 0.005 |
| Lower door crease | 48 − (140 − x) × 0.04 | 47 − (140 − x) × 0.085 (steeper rise toward the rear wheel) |
| Trunk lip spoiler | y 18–20 | y 15–17 on the higher trunk edge, x 2–14 |
| Rear-glass reflection | 8 px from (72,6) | 7 px from (66,6) |

Unchanged: size 200 × 66, wheel centres, nose, headlight/DRL/fog lamp, taillight, tint, kinds.

## 2. Background (`ui/bootscene`)

Layout changes so tall lamps and tree tops fit the animated area:

| y | Content |
|---|---|
| 0–71 | Static sky, drawn once: vertical gradient from #05070C (top) to #0B1222 (y 71), 18 faint stars #96A0BE (twinkle as v1), no moon; caption as before. |
| 72–239 | Animated, **7 strips** of 24 rows through the same 320 × 24 sprite. |

Animated layers, back to front:
1. Sky gradient continued (#0B1222 at y 72 → #0E1628 at y 129).
2. **Tree line** (static, no scroll): canopy silhouette #0A120E from a per-column top
   `treeTop[x]` computed once at scene start (26 overlapping round canopies, tops y 86–110, seeded,
   deterministic), trunks are not drawn; ground band #09100C y 126–135.
3. **Fence** y 124–139: chain-link diagonal pattern #285A37 at alpha 90, top rail #32643F; scrolls
   30 px/s. Two-three **parked car** silhouettes (#1E2128 body, #1A1D24 cabin, 1 red pixel tail
   light each end) every 150 px, scrolling with the fence.
4. **Asphalt** y 140–239: near black, #0E0F13 at y 140 → #12131A at y 239, ±2 grain from a fixed
   per-pixel hash (no randomness per frame); faint parking-bay lines (#46484E, alpha 140) slanted,
   every 60 px, y 150–167; lane dashes #3C3E42 22 × 2 px every 48 px at y 226; both scroll 90 px/s.
5. **White LED street lamps** every 170 px, scroll 90 px/s: pole #242830 2 px wide y 62→150
   (clipped to the animated area: visible from y 72), arm to the head, head 6 × 2 #F5FAFF at y 72,
   light cone alpha 13 of #E1EBFF down to y 153, lit ellipse on the asphalt (#C8D2E6, alpha ≤ 26).
6. Car shadow, **underglow** (below), headlight beam (cool white #F0F5FF), car, wheels.

**Underglow** (#8C46FF, core #BE8CFF), all alphas scaled by `glowAlpha(e) / 217` (1.0 at rest):
- Halo on the asphalt: ellipse centred (car x + 100, 213), semi-axes (126, 14), alpha
  0.55 × (1 − d)^1.5.
- Core: ellipse (car x + 100, 211), semi-axes (90, 5), core colour, alpha 0.45 × (1 − d).
- LED tube: x car x + 30 … + 171, y 205 (core colour, alpha 230) and y 206 (purple, alpha 205).
- Car body rows 45–65 of the sprite blend toward purple by min(0.45, (row − 44) × 0.03);
  tyre pixels below the hub (dy > 4) by min(0.5, (dy − 4) × 0.05).

## 3. Ending: brake, then montage (replaces v2 §3)

- Exit gating unchanged (`tHero` = first frame with t ≥ 5000, greeting done, online).
- **Brake** 0–600 ms: as v2 (car rolls 40 px, world slows via `brakeTime`, whole screen fades
  via `brakeLevel`), now rendered over the v3 background (all 10 strips of the screen).
- **Montage** from 600 ms: frame `i = (u − 600) × fps / 1000` of `data/montage.bin`, drawn
  full-screen with TJpg_Decoder straight to the display (each new index once); ends after the
  last frame. Without a montage (0 frames) the scene ends at the end of the brake (screen black).
- The scene's last image is the montage's own fade to black; then `setup()`/`loop()` take over.

## 4. Montage asset

**Format** (`data/montage.bin`, little-endian): `"MTG1"`, `u16 count`, `u16 fps`, then `count` ×
(`u32 offset`, `u32 size`) from the file start, then the baseline JPEGs (320 × 240, 4:2:0).

**Builder** `tools/gen_montage.py [photo dir]` (default `montage-photos/`), stdlib + ffmpeg:
shot table (photo file, car centre/width in a 640-wide copy, frames, slide):

| Shot | File | Car centre (x, y) | Car width | Frames | Push-in /frame | Slide |
|---|---|---|---|---|---|---|
| 1 | `07-right-wide-turning.jpg` | (325, 603) | 610 | 18, fade in 0.8 s | 0.0015 | 0 |
| 2 | `02-front-close.jpg` | (282, 655) | 556 | 14 | 0.004 | −1 |
| 3 | `04-left-closer.jpg` | (291, 653) | 570 | 16 | 0.004 | −1 |
| 4 | `06-headlight-detail.jpg` | crop y 290 (plain) | — | 28, fade out 0.6 s | 0.003 | 0 |

Each shot: scale the photo to 640 wide; scale factor `s = max(1, target × 640 / width)` with
targets 0.95 / 0.87 / 0.89; place it so the car centre lands at (320, 250) but always covering the
full width (`fx = clamp(320 − cx·s, 640 − 640·s, 0)`); grade `eq=contrast=1.06:saturation=1.08:gamma=0.97`;
zoompan push-in from 1.0 with the slide `x = (iw − iw/zoom)(0.5 + 0.45·slide·(on/n − 0.5))`;
light vignette (PI/8); output 320 × 240 at 10 fps. Shots joined with 0.3 s crossfades; frames
encoded as JPEG (`-q:v 8`, yuvj420p). Prints frame count and size (≈ 735 KB for this sequence).
Photos are matched by file name; a missing photo is an error naming it.

**Build fallback** `tools/montage_clip.py` (pre-build, like `greeting_clip.py`): if
`data/montage.bin` is missing, write an empty montage (`"MTG1"`, count 0, fps 10). Both
`data/montage.bin` and `montage-photos/` are git-ignored.

**Reader** `ui/montage` (pure, host-tested): `bool parse(const uint8_t* blob, size_t len)` on the
embedded file, `int count()`, `int fps()`, `bool frame(int i, const uint8_t** jpg, size_t* len)`
with bounds checks (bad magic, offsets past the end → count 0).

## 5. Removed

`tools/gen_hero_sprite.py`, `src/ui/hero_sprite.*`, `test/test_hero_sprite`, and the hero-only
parts of `util/bootanim` (`HeroBeat` beyond Brake, `heroLevel`, `lampsOn`, `nameLevel`,
`heroProgress`, `heroDoneAt`, the related constants and tests). Kept: `BRAKE_MS`, `brakeX`,
`brakeTime`, `brakeLevel`. Added: `int montageFrame(uint32_t u, int fps, int count)` (−1 before
600 ms, `count` when finished).

## 6. Tests

- `test_bootanim`: brake tests kept; montage frame index (u 599 → −1, 600 → 0, 600 + 1000 at
  10 fps → 10, past the end → count, count 0 → 0 at once).
- `test_car_sprite`: updated values for the v3 geometry (handles at the new x, spoiler at the new
  height, fuel door, B-pillar), measured from the regenerated sprite.
- `test_montage`: valid blob (2 frames) parses; frame bytes/lengths; out-of-range index; bad
  magic; offset past the end; truncated header.
- On the deck: full boot with the owner watching; frame times per phase in the log; montage at
  ~10 fps; `[mem] bootscene end` before Spotify starts.

## Out of scope

- Animated tree parallax, rain/reflections, other montage sequences (the builder's shot table
  is the one place to change it).
