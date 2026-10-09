# Boot scene: design

Date: 2026-10-09. Status: approved design, not implemented.

## Goal

While the boot greeting plays (`audio/greeting`, e.g. the ETC card voice), the screen shows a
2D animation: a side view of the owner's silver **Honda City sedan (2024)** (trunk lip spoiler,
RGB underglow) driving along a road at night. The car drives in, cruises for at least 5 s and
for as long as WiFi is still connecting, then drives off and the deck starts. The car's lights
react to the sound.

## Decisions

- Scene **A, "GBA night road"** (mockup chosen 2026-10-09): banded navy sky with stars and a
  moon, a city skyline in slow parallax, streetlights passing, guardrail, road with moving lane
  dashes. Always the night look (independent of night mode).
- The car is **large and faithful to the City sedan (7th gen, the owner's 2024)**: 200 × 66 px
  (about two thirds of the screen width), proportions from the real car (4549 mm long, 1477 mm tall, 2600 mm
  wheelbase). No badges. Owner's details: **trunk lip spoiler** (body colour) and **RGB
  underglow cycling through the hues**.
- **Stays at least 5 s, and until WiFi is up**: enter, cruise, exit. If WiFi isn't found the car
  keeps cruising while the network task retries, and leaves when WiFi connects.
- **Lights follow the sound** while it plays: headlight beam, headlights, taillights and
  underglow brighten with the clip's loudness (beeps flash, the voice flickers).
- A caption in the sky shows the WiFi state ("Connecting WiFi...", then
  "WiFi not found - retrying...").
- Rendering: **16-bit strips** (approach 1 of 3; a 16-colour full frame looked flat, direct
  drawing flickers).

## Timeline (`util/bootanim`, pure, host-tested)

Three phases. `t` is ms since the scene started; `tExit` is when the exit began.

| Phase | When | Car left edge x (px) |
|---|---|---|
| Enter (ease-out) | `t < 900` | `-202 + 262 * (1 - (1 - q)^2)`, `q = t / 900` (-202 → 60) |
| Cruise (sway) | until the exit starts | `60 + round(6 * sin(2π * t / 4000))` (54 – 66, centred) |
| Exit (ease-in) | 900 ms from `tExit` | `xc + (330 - xc) * q^2`, `q = (t - tExit) / 900`, `xc` = cruise x at `tExit` (→ 330, off screen) |

The exit starts at the first frame where all hold:

- `t >= 5000` (MIN_SHOW_MS),
- the greeting sound has finished (clips are ≤ 4 s, so this only matters if that limit grows),
- the "WiFi is up" check passed to the scene returns true.

The scene ends when the exit phase completes.

Other motion, all functions of `t`:

- Road dashes and streetlights scroll left at 90 px/s; skyline at 14 px/s.
- Wheels: spoke angle `t * 9 rad/s`, 10 spokes.
- Suspension bob: car drawn 1 px lower when `(t * 6 / 1000) % 5 == 0` (integer division).
- Underglow hue: `t * 120 / 1000` degrees (one full cycle every 3 s).
- Stars twinkle: star `i` hidden when `(t * 3 / 1000 + i) % 7 == 0`.

Light mapping from loudness `e` (0–255; 0 once the sound has ended, which is the resting look):

- Headlight lamp: grey level `160 + 95 * e / 255` (warm tint: blue at 80 %).
- Taillight: red `140 + 115 * e / 255`; taillight glow alpha `128 * e / 255`, drawn only when
  `e > 12`.
- Headlight beam alpha at the lamp: `40 + 56 * e / 255` (of 255), fading linearly to 0 over 90 px.
- Underglow alpha: `115 + 102 * e / 255`.

## Loudness (`util/loudness`, pure, host-tested)

`uint8_t loudness(const uint8_t* pcm16le, size_t samples, size_t at, size_t window)`: the peak
absolute sample in `[at, at + window)` (clamped to the clip), scaled to 0–255 (32767 → 255).
Reads bytes (no alignment assumption), like the greeting player.

`audio/greeting` gains:

```cpp
uint32_t durationMs();          // clip length at 16 kHz
uint8_t level(uint32_t ms);     // loudness() over a 20 ms window at ms; 0 past the end
```

The level ignores `VOLUME` (the scene reacts the same at any volume). The scene and the sound
start together in `setup()`; task start jitter is a few ms, below what the eye notices.

## Car sprite (`tools/gen_car_sprite.py` → `src/ui/car_sprite.inc`, `ui/car_sprite`)

Reference: the 7th-generation City sedan (Brazil 2022–2024, same body as the 2023 Asian
facelift), from freely licensed photos on Wikimedia Commons and Honda Brazil's published
data: 4549 mm long, 1477 mm tall, 2600 mm wheelbase, 185/55 R16 tyres, LED headlights with DRL,
LED taillights, shark-fin antenna, body-coloured mirrors with LED indicators, 16" two-tone
alloys. Scale: 200 px = 4549 mm, so **1 px ≈ 22.7 mm**.

200 × 66 px is too large to hand-type as a character grid, so the generator **builds it from
geometry** (polygons and lines, the same shapes as the chosen mockup), then adds pixel details;
the output is committed, like `status_sprite.inc`. Coordinates: x from the rear bumper (0) to
the nose (200), y from the roof (0) down to the ground line (65). Facing right.

Proportions (from the real car):

| Feature | Value |
|---|---|
| Wheel centres | x = 46 (rear, 1044 mm overhang), x = 160 (front, 905 mm overhang), y = 51.5 |
| Tyre / rim | r = 13.5 (610 mm tyre) / r = 9.5 (16" rim) |
| Roof peak | y ≈ 1 at x ≈ 86–112 |
| Window bottom (beltline) | y ≈ 21 (≈ 1000 mm above the ground) |
| Shoulder line | y ≈ 27 (≈ 870 mm), nearly level, very slightly rising toward the rear |
| Trunk deck | y ≈ 19–21, short (x ≈ 3–26), as high as the beltline |
| Cowl / windshield base | x ≈ 144, y ≈ 20; windshield top x ≈ 120 |
| Nose | y ≈ 30–52, rounded, front bumper bottom y ≈ 54 |
| Wheel arches | radius 17.5 around (x, 54) |

Body outline (polygon, clockwise from the rear bumper bottom):
(4,51) (1,46) (0,38) (0,29) (1,24) (3,21) (22,19.2) (26,19) (40,12) (56,5) (70,2.2) (86,1.2)
(100,1) (112,2) (120,4) (134,13) (144,20) (150,21.5) (172,25) (188,28) (196,30.5) (199,34)
(200,40) (199,47) (196,52) (186,54) (178,54), front arch, (64,55), rear arch, (12,52).

Side glass (polygon): (48,20.6) (60,9.5) (72,4.6) (86,3.4) (100,3.2) (111,4) (119,6.2)
(133,15.6) (139,20.3): the rear door glass **ends in a point** under a thick body-coloured
C-pillar.

Details, all from the reference photos:

- **Paint shading**, silver (Prata Lunar): above the beltline #A7AFB8; band above the shoulder
  line #D5DBE1 with a #EEF2F5 top edge; the crease below it #7C858F (2 px); body #BCC3CB down
  to the lower crease; #959EA8 below it; rocker #5D656E. Roof highlight #EEF2F5.
- **Shoulder line**: the sharp crease from the back of the headlight to the front of the
  taillight; the **door handles sit on it** (chrome #E8ECEF over a #2C3038 shadow, 7 px, at
  x ≈ 82 and 122).
- **Lower door crease** rising from the front door bottom (y ≈ 48) toward the rear wheel.
- **Black B-pillar** x ≈ 96–99, quarter-glass divider at x ≈ 60–63, chrome beltline strip,
  reflections on the windshield and rear door glass (#4F6D92).
- **Shark-fin antenna** at the rear of the roof (x ≈ 75–82, 2 px tall, #2C3038).
- **Door-mounted mirror** (body colour) with an **amber LED indicator** line.
- **Fuel door** outline on the rear quarter (x ≈ 36–42, y ≈ 28–33).
- **Headlight**: slim smoked wedge (#28323F) sweeping back along the top of the fender from the
  nose to x ≈ 164; a row of LED projector pixels and a **white DRL strip** along its lower edge;
  a chrome edge on top; chrome bar at the nose.
- **Black triangular corner intake** in the front bumper, dark lower lip.
- **Wrap-around taillight** where the shoulder line ends (x ≈ 0–21, y ≈ 22–29; #FF5050 top,
  #B81414 bottom, a #FF8080 Z-shaped LED strip); small red **reflector** low on the rear bumper.
- **Trunk lip spoiler** (owner's): body-coloured ducktail 1–2 px above the trunk edge, x ≈ 2–15.
- Door shut lines (front door along the A-pillar base, B-pillar, rear door curving round the
  rear arch), hood/fender shut line, dark side skirt between the wheels.
- Wheel arches: very dark liner #0E1016 inside, #5D656E lip.
- **Pixel kinds**: upper paint (above the shoulder line), lower paint, headlight, taillight,
  other, transparent. The drawing code recolours lamps from the loudness and adds the
  streetlight reflection to paint pixels (see Scene).
- API like `status_sprite`: `width()`, `height()`, `pixel(x, y, &rgb565, &kind)`.

Wheels are drawn in code so they can spin: tyre #141414 r = 13.5, rim lip #C9CED4, dark rim
face #2B2F36, **5 twin spokes** (machined face #DFE3E8 with a #AAB0B8 edge, the two-tone 16"
alloy), hub #C8CCD2 with a dark centre.

The brainstorm mockup (`city-2024-v3.html` under the git-ignored `.superpowers/brainstorm/`) is
the visual reference for the generator; the geometry above is what the generator encodes.

## Scene (`ui/bootscene`)

```cpp
namespace bootscene {
// Starts the scene task. `online` is polled each frame for the exit condition (net::isOnline).
void start(TFT_eSPI& tft, bool (*online)());
void setCaption(const char* text);   // static string; redrawn by the scene task
bool active();                       // true until the car has left and the task has ended
void waitDone();                     // blocks until !active()
}
```

Layout (320 × 240):

| y | Content |
|---|---|
| 0–95 | Sky, drawn **once** directly on the display: bands #0B1030 #0F1640 #141D52 #1A2564 (30 px each), 40 stars, moon (20 px disc #F4F1D0 at x=266, y=14), caption at (6, 6) font 2 #CFD8FF. Twinkling stars are single `drawPixel` calls; a caption change repaints only its own rectangle. |
| 96–239 | Animated area (144 rows), redrawn every frame in **six 320 × 24 strips** through one `TFT_eSprite` (16-bit, 15 360 bytes). |

Animated area, back to front: last sky band #1A2564 (to y 119) and #212E76 (y 120–139);
skyline (buildings 18–28 px wide, tops at y ≥ 96, #0A0D24, lit windows #F0C860); ground
#10152E (y 140–147); streetlights every 160 px (pole #3A4058, head and lamp #FFE9A8, light
cone alpha-blended); guardrail #6B7089 (y 148–151) and #2A2E44 (y 152–153); road #262A3A
(y 154–217) and #1B1E2B (y 218–239); lane dashes 26 × 3 #D8D8C0 every 48 px at y 226; car
shadow; underglow (sill tube + light pool on the road); headlight beam; car sprite with its
ground line at y 214; wheels.

**Streetlight reflections**: the streetlight nearest the car's middle lights the paint below it.
For paint pixels, with `d` = horizontal distance to that lamp, blend toward #FFF0C8 by
`0.55 * (1 - d / 26)` for upper paint and `0.30 * (1 - d / 18)` for lower paint (0 outside).
As the lamps scroll past, a warm highlight sweeps across the car (done per pixel while the
strip is rendered).

Each strip is fully rendered before it is pushed, so nothing flickers. Target 25 fps (40 ms
frame budget); the average frame time is logged when the scene ends.

## Boot sequence

`src/main.cpp`:

```
backlight::begin();
greeting::play();
bootscene::start(s_tft, net::isOnline);
bootscene::setCaption("Connecting WiFi...");
if (net::connectAny()) {                 // no display access from setup while the scene runs
    bootscene::waitDone();               // car leaves after >= 5 s
    ...today's code: "Spotify auth...", setup portal, spclient::begin()
} else {
    bootscene::setCaption("WiFi not found - retrying...");   // scene keeps running
}
nettask::start(spotifyReady);
```

- `loop()` draws nothing while `bootscene::active()` (short delay and return). Its first draw
  after the scene is a full one (nothing was on screen yet), as today.
- `nettask`: in its "WiFi wasn't up at boot" path, it waits until `!bootscene::active()` before
  `spclient::begin()`, so the scene's buffer is gone before TLS needs the heap.
- Later WiFi drops during normal use keep today's "No signal..." screen; the scene only plays at
  boot.

## Memory and tasks

- Scene task: core 1, priority 1, 4 KB stack. The 15 KB strip sprite is allocated at start and
  freed when the scene ends. `mem::log("bootscene")` at start and end.
- The car sprite lives in flash (200 × 66 RGB565 + kind bits, about 30 KB).
- TFT_eSPI is not thread-safe: while the scene is active only the scene task draws
  (`setup()` waits or draws nothing, `loop()` skips).

## Error handling

- Strip allocation fails: no scene; the task draws the current caption on a black screen,
  waits for the same exit condition, then ends. The sound still plays. One log line.
- WiFi never comes: the car keeps cruising and the caption says it's retrying (today the
  "No signal..." screen would show instead).

## Testing

- Host suites:
  - `test_bootanim`: car x at enter start/end, cruise sway range, exit start/end from a given
    `tExit`; exit gating (not before 5000 ms, not while the sound plays, not while offline);
    bob; light mappings at e = 0 and 255; hue wrap.
  - `test_loudness`: silence → 0, full scale → 255, window clamped at the clip end, odd byte
    alignment.
  - `test_car_sprite`: size 200 × 66; transparent corners above the trunk and in front of the
    windshield; a body pixel's colour; headlight and taillight pixel kinds; spoiler pixels
    present above the trunk line.
- On the deck: watch the scene with WiFi available (car leaves at about 5.9 s) and with the
  hotspot off (car keeps cruising, caption changes, car leaves once the hotspot is turned on);
  serial shows the frame time, `[mem] bootscene` before and after, and the greeting line.

## Out of scope

- A day version of the scene, skipping it by touch, playing it anywhere but boot.
- Changing the greeting clip format or volume.
