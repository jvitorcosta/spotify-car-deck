# Gen-3 Battle UI + PMD Walk-Cycle Walker — Design

Date: 2026-10-06. Extends `2026-10-04-pokedeck-esp32-spotify-design.md` (§9 walking
animation is superseded by §3 below). Builds on the as-built deck (Tasks 1–16).

## 1. Goal

Make the deck look like a Gen-3 (FireRed/LeafGreen/Emerald) battle screen: battle
info boxes, shadowed pixel text, the HP bar with its "HP" tag, and the battle dialogue
box. The song's Pokémon lives on the progress bar instead of in a separate static box,
and it really walks (a multi-frame walk cycle) instead of the bob/flip approximation.

Success: on device, every song shows its Pokémon walking (real frames when a PMD sheet
exists, the current bob/flip walker otherwise) along a full-width HP bar inside a
battle-style status box, with the lyric in a battle dialogue box; no flicker, no
per-frame network/decode, bounded RAM on the no-PSRAM ESP32.

## 2. Layout (320×240 landscape)

| Region | Rect (approx.) | Content |
|---|---|---|
| Top strip | 0,0 → 320×20 | "NOW PLAYING" + spinning CD · shuffle/repeat · device icon + name |
| Album art | box 8,24 98×98 | cover, framed like a battle box |
| **Info box** | 114,30 198×86 | TRACK TITLE · artist · "From:" context |
| **Status box** | 8,126 304×70 | Pokémon name + `No.NNNN` · `HP m:ss/m:ss` · walker · HP bar · EXP bar |
| **Dialogue box** | 4,200 312×38 | ♪ current lyric ♪ (up to 2 lines) |

- **Info box:** track title (caps-folded, truncated), artist, then `From: <context>`.
  (No `Lv.`: track popularity isn't in the now-playing data and would cost an extra
  Spotify request per track.)
- **Status box:** row 1 = Pokémon name (type accent colour) + `No.NNNN` left,
  `HP m:ss/m:ss` (remaining/total) right; walk band (~32 px) where the walker stands
  on the bar; HP bar (x≈16, w≈288, h≈10) draining from the left; a 3 px **EXP bar**
  under it showing Spotify volume %.
- **Dialogue box:** current synced lyric line, centred, up to 2 lines in font 2,
  ♪ icons either side; empty box when there is no line.
- `AppState.repeat` uses our own convention 0 = off, 1 = context, 2 = track (the
  library enum has 0 = track, which would show repeat-one before the first poll).
- No gender symbol (would need an extra PokéAPI species request per song).

### 2.0 Gen-3 battle style

Applies to every box (palette values approximate the GBA games; final values tuned on
the real panel, which renders colours differently from a PC screen):

- **Background:** sky `#A8D8F8` upper half, grass `#88C878` lower half with a slightly
  darker horizon stripe; mostly hidden behind boxes, visible in gaps.
- **Info/status boxes:** cream fill `#F8F8D8`, 2 px dark border `#404848`, a 3 px
  offset drop shadow `#587060`, and a **slanted tab** on the outer edge (right end of
  the info box, left end of the status box) like the opponent/player boxes.
- **Text:** dark grey `#404040` with a 1 px down-right shadow `#D8D0B0` (drawn twice).
  Dynamic text (HP time) clears its own rect before redrawing.
- **HP bar:** dark tag `#484848` with "HP" in amber `#F8B800` on the left; fill green
  `#58D080`, yellow `#F8C800` (≤50%), red `#F85838` (≤20%), each with a 2 px lighter
  shine line on top; empty part dark `#506058`.
- **EXP bar:** 3 px, blue `#40C8F8` on dark.
- **Dialogue box:** 3 px dark-teal frame `#284860` with an inner light-teal line
  `#68A0B8`, white `#F8F8F8` rounded interior, text `#404040` with shadow `#D0D0D0`.
- **Top strip:** dark `#283038`, cream text with shadow.
- **Status screens** ("No signal...", "Nothing playing") use the dialogue-box style
  centred on the battle background.

### 2.1 Pixel icons

The built-in fonts are ASCII-only, so "emoji" are small pixel-art icons (1-bit bitmaps,
12×12, drawn with `drawBitmap` in a theme colour), except the CD which is drawn
procedurally:

- **Spinning CD** right after "NOW PLAYING" in the top bar: disc + hole + a highlight
  wedge rotated through 4 frames (~6 fps) while playing; frozen when paused, so it
  doubles as the play/pause indicator. Repaints only its own ~14×14 rect.
- **Music notes** `♪` either side of the lyric in the dialogue box (hidden when there
  is no line).
- **Device icon** before the device name, chosen from Spotify's device `type`:
  Smartphone/Tablet → phone, Computer → laptop, Speaker/AVR/CastAudio → speaker,
  TV/CastVideo/STB/GameConsole → TV, Automobile/CarThing → car, anything else →
  speaker. Requires storing `device.type` in `AppState` (`deviceType[16]`) from the
  existing PlayerDetails poll.
- **Shuffle / repeat** icons in the top bar left of the device icon: drawn in cream
  when on, dim navy-grey when off; repeat-one (track) adds a tiny "1". Data already
  polled every ~12 s; the top bar redraws when any of these change.

Device-type → icon mapping is a pure, host-tested function (`ui/icons` pure part).

The old left Pokémon box, the separate track/context panels and the static 48 px sprite
are removed. Exact coordinates may shift a few px during implementation to fit font
metrics; region order is fixed.

## 3. Walker

### 3.1 Source (primary): PMDCollab SpriteCollab

- Base: `https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/<DDDD>/`
  where `DDDD` is the zero-padded dex number (same random #1–1025 as today).
- `AnimData.xml` → the `<Anim>` whose `<Name>` is `Walk`: `FrameWidth`,
  `FrameHeight`, `Durations/Duration*` (game ticks, 1/60 s). If that Anim has
  `<CopyOf>X</CopyOf>`, resolve to Anim `X`.
- `Walk-Anim.png` → 8 rows (directions, PMD order Down, DownRight, **Right**, UpRight,
  Up, UpLeft, Left, DownLeft) × N frames of `FrameWidth×FrameHeight`. Only row 2
  (Right) is used; the walker moves left→right.
- Host is already allowed (`raw.githubusercontent.com`). License CC BY-NC — personal,
  non-commercial use with credit (see §6).

### 3.2 Loading (once per song, on track change)

1. Get the XML and PNG from the SD cache (`/pmd/<dex>.xml`, `/pmd/<dex>.png`) or
   download them (size guards: XML ≤ 32 KB read whole — real files are 3–12 KB — and
   PNG ≤ 40 KB) and save them to the cache.
2. Decode the PNG with PNGdec, copying only rows `[2·FH, 3·FH)` into a transient
   buffer (sheet width ≤ 512 px; wider → fail to fallback).
3. Compute the **shared opaque bounding box** across all frames of the row, so every
   frame is cropped to the same rect (stable baseline, no jitter).
4. Scale: native 1:1 if the cropped height ≤ band height (36 px); otherwise
   nearest-neighbour downscale so height = 36 (aspect kept).
5. Store frames in a persistent buffer capped at **16 KB** (RGB565 + 1-byte mask = 3 B/px).
   If `frames × w × h × 3` exceeds the cap, keep every k-th frame (k minimal) and sum the
   skipped durations into the kept ones.
6. Log `[walk] pmd <n> frames <w>x<h>` or `[walk] fallback (<reason>)`, plus
   `[heap] free=<n> max=<n>` on every track change.

### 3.3 Fallback

If the XML/PNG is missing (non-200), malformed, too large, or out of memory: use the
existing cropped PokeAPI sprite with bob + periodic mirror (current behaviour), so every
song still has a walker.

### 3.4 Animation

- Position: `walkX(frac)` along the HP bar's inner width, standing on the
  drained/remaining boundary (unchanged).
- Frame: chosen from elapsed play time using the PMD durations (cyclic). Advances only
  while playing; paused → frame 0, standing still.
- Drawn at ~8 fps by composing a cream-backed rect in RAM and `pushImage` (existing
  `walkrect` dirty-rect logic; push rect sized for the largest frame).

## 4. Components

| Unit | Kind | Responsibility |
|---|---|---|
| `util/animdata` | pure, host-tested | Parse AnimData XML text → `{frameW, frameH, durations[]}` for `Walk` (incl. `CopyOf`). |
| `util/walkanim` | pure, host-tested | Frame index for an elapsed time + durations; shared-bbox crop + scale + frame-skip arithmetic. |
| `images/png` | device | `loadWalkSheet(dex)` (PMD path) alongside existing `loadWalkSprite` (fallback); exposes current frame buffer/mask/size/count. |
| `images/cache` | device | Generalised to a path prefix so `/pmd/` entries sit beside sprite cache. |
| `ui/icons` | pure map + device draw | Device-type → icon mapping (host-tested); 12×12 bitmaps; procedural spinning CD. |
| `ui/battle` | device | Gen-3 style primitives: battle box (tab + shadow), shadowed text, HP bar with tag + shine, EXP bar, dialogue box, background. |
| `ui/theme` | pure, host-tested | Battle palette constants (+ shine variants); `hpColor` thresholds already Gen-3 (≤50% yellow, ≤20% red) — now covered by a test. |
| `ui/screen_now` | device | Battle layout; top-strip icons; walker draws the current frame. |
| `main.cpp` | device | On track change: PMD load → fallback; heap log; drive frame time. |

## 5. Error handling

- Every network/decode failure degrades to the fallback walker; never blocks the deck.
- Corrupt cache entries (decode fails) are deleted so the next play re-downloads.
- All PNG decode paths reject images wider than their line buffer.

## 6. Credits

Add `CREDITS.md`: sprites from PMDCollab SpriteCollab (CC BY-NC 4.0, per-sprite
artist credits in each `credits.txt`; many are official Spike Chunsoft rips), Pokémon
data/sprites from PokeAPI, Pokémon © Nintendo / Creatures / GAME FREAK. The project is
personal and non-commercial.

## 7. Testing

- Host (Unity via `.devtools/ntest.ps1`): `test_animdata` (simple Walk, CopyOf,
  missing Walk, multiple durations), `test_walkanim` (frame-at-time wraparound, paused,
  bbox crop union, downscale to band, frame-skip under the cap), `test_icons`
  (device-type mapping incl. unknown/empty), `test_theme` (HP colour thresholds at 51/50/21/20%).
- Device: side-by-side visual check against a Gen-3 battle screenshot (boxes, tab,
  shadow, HP tag, dialogue box); colours tuned on the panel.
- Device: CD spins while playing and freezes on pause; shuffle/repeat icons follow the
  phone within ~12 s; device icon matches the active device.
- Device: serial shows `[walk] pmd …` for most songs and `[walk] fallback` when a sheet
  is absent; visual check that the Pokémon walks right on a full-width bar with no
  flicker; heap log stable across many track changes.

## 8. Out of scope

Touch controls, full lyrics screen, shiny/gender/form variants, directions other than Right.
