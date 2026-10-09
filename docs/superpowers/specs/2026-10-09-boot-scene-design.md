# Boot scene: design

Date: 2026-10-09. Status: approved design, not implemented.

## Goal

While the boot greeting plays (`audio/greeting`, e.g. the ETC card voice), the screen shows a
short 2D animation: a side view of the owner's silver Honda City (trunk lip spoiler, RGB
underglow) driving along a road at night. The animation lasts as long as the sound and its
lights react to the sound. WiFi connects in the background at the same time, so boot is not
slower than today.

## Decisions

- Scene **A, "GBA night road"** (mockup chosen 2026-10-09): banded navy sky with stars and a
  moon, a city skyline in slow parallax, streetlights passing, guardrail, road with moving lane
  dashes. Always the night look (independent of night mode).
- The car: silver sedan, side view facing right, City-like silhouette, no badges, **trunk lip
  spoiler** (body colour), **RGB underglow cycling through the hues**.
- **Drive-through**: the car enters from the left, cruises mid-screen, leaves on the right as
  the sound ends.
- **Lights follow the sound**: headlight beam, headlights, taillights and underglow brighten with
  the clip's loudness at each moment (beeps flash, the voice flickers).
- **Runs while WiFi connects**: the scene draws from its own task; `setup()` connects without
  touching the display and waits for the scene before it draws anything.
- Rendering: **16-bit strips** (approach 1 of 3; a 16-colour full frame looked flat, direct
  drawing flickers).

## Timeline (`util/bootanim`, pure, host-tested)

Scene length `S = max(clipMs, 1500)` (a very short clip still gets a readable drive-through).
With `p = t / S`:

| Phase | p | Car left edge x (px) |
|---|---|---|
| Enter (ease-out) | 0 – 0.25 | `-110 + 210 * (1 - (1 - q)^2)`, `q = p / 0.25` (-110 → 100) |
| Cruise (slow drift) | 0.25 – 0.75 | `100 + 20 * (p - 0.25) / 0.5` (100 → 120) |
| Leave (ease-in) | 0.75 – 1 | `120 + 210 * q^2`, `q = (p - 0.75) / 0.25` (120 → 330, off screen) |

Other motion, all functions of `t` (ms since the scene started):

- Road dashes and streetlights scroll left at 90 px/s; skyline at 14 px/s.
- Wheels: spoke angle `t * 18 rad/s`, 5 spokes.
- Suspension bob: car drawn 1 px lower when `(t * 6 / 1000) % 5 == 0` (integer division).
- Underglow hue: `t * 120 / 1000` degrees (one full cycle every 3 s).
- Stars twinkle: star `i` hidden when `(t * 3 / 1000 + i) % 7 == 0`.

Light mapping from loudness `e` (0–255):

- Headlight lamp: grey level `160 + 95 * e / 255` (warm tint: blue at 80 %).
- Taillight: red `140 + 115 * e / 255`; taillight glow alpha `128 * e / 255`, drawn only when
  `e > 12`.
- Headlight beam alpha at the lamp: `40 + 56 * e / 255` (of 255), fading linearly to 0 over 70 px.
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

- Hand-drawn pixel art, about 100 × 30 px, written in the generator as a text grid (one
  character per pixel, a legend maps characters to RGB565; `.` is transparent). Output is
  committed, like `status_sprite.inc`.
- Colours: body #B9C0C8, highlight #E9EEF2, shade #7C858F, trim/arches #2A2E38, glass #1E2F48,
  glass reflection #5C7AA0. Lamp pixels use marker characters (`H` headlight, `T` taillight)
  so the drawing code recolours them from the loudness.
- Includes the trunk lip spoiler, door lines, handles, mirror, the dark wheel arches.
- Wheels are not in the sprite: drawn in code (tyre r=7 #141414, rim r=4 #C8CCD2, 5 spokes
  #2B2B2B, hub #E0E0E0) so they can spin. Wheel centres at sprite x=20 and x=80.
- API like `status_sprite`: `width()`, `height()`, `pixel(x, y, &rgb565, &kind)` where kind is
  body / headlight / taillight / transparent.

## Scene (`ui/bootscene`)

```cpp
namespace bootscene {
// Starts the scene task: draws the scene for max(greeting length, 1.5 s), then clears the screen
// and draws `afterText` at (10, 10) in font 2 (the boot status line). Returns immediately.
void start(TFT_eSPI& tft, const char* afterText);
void waitDone();   // blocks until the scene task has finished with the display
}
```

Layers, back to front:

1. Sky y 0–95, drawn **once** directly on the display: bands #0B1030 #0F1640 #141D52
   #1A2564 #212E76 (30 px each, the last continuing into the strips), 40 stars, the moon
   (22 px disc #F4F1D0 at x=262, y=22). Twinkling stars are single `drawPixel` calls.
2. Animated area y 96–239 (144 rows), redrawn every frame in **three 320 × 48 strips** through
   one `TFT_eSprite` (16-bit, 30 720 bytes): band colour, skyline (buildings 18–28 px wide,
   top at y ≥ 96, #0A0D24, lit windows #F0C860), ground #10152E, streetlights (pole #3A4058,
   lamp #FFE9A8, light cone alpha-blended), guardrail #6B7089/#2A2E44, road #262A3A/#1B1E2B,
   lane dashes #D8D8C0, underglow, headlight beam, car sprite, wheels, car shadow.
3. Each strip is fully rendered before it is pushed, so nothing flickers.

Target 25 fps (40 ms frame budget); the frame time is logged once at the end.

## Boot sequence (`src/main.cpp`)

```
backlight::begin();
greeting::play();
bootscene::start(s_tft, "Connecting WiFi...");
bool online = net::connectAny();      // no display access while the scene runs
bootscene::waitDone();
...then today's code: "Spotify auth..." / setup portal / "WiFi not found - retrying..."
```

The "Connecting WiFi..." line that `setup()` drew before `connectAny()` moves into the scene's
`afterText`, so it still appears if WiFi is still searching after the scene ends.

## Memory and tasks

- Scene task: core 1, priority 1, 4 KB stack. The strip sprite is allocated at start and freed
  at the end, before Spotify auth needs heap for TLS. Logged with `mem::log("bootscene")`.
- TFT_eSPI is not thread-safe: only the scene task draws until `waitDone()` returns.

## Error handling

- Strip allocation fails: no scene. The task draws `afterText` and ends; the sound still plays.
  One log line.
- Greeting clip shorter than 1.5 s: the scene runs 1.5 s (`S` above); lights go dark when the
  sound ends.

## Testing

- Host suites: `test_bootanim` (car x at p = 0, 0.25, 0.5, 0.75, 1; scene length minimum; bob;
  light mappings at e = 0 and 255; hue wrap), `test_loudness` (silence → 0, full-scale → 255,
  window clamped at the clip end, odd byte alignment), `test_car_sprite` (size, transparent
  corners, body pixel colour, lamp pixel kinds, spoiler pixels present).
- On the deck: watch the scene with the ETC clip and with a short clip; serial shows frame time,
  `[mem] bootscene` before/after, and the greeting line.

## Out of scope

- A day version of the scene, skipping it by touch, playing it anywhere but boot.
- Changing the greeting clip format or volume.
