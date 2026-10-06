# Merged Status Panel + PMD Walk-Cycle Walker — Design

Date: 2026-10-06. Extends `2026-10-04-pokedeck-esp32-spotify-design.md` (§9 walking
animation is superseded by §3 below). Builds on the as-built deck (Tasks 1–16).

## 1. Goal

Make the deck read as one Gen-3 "battle status" box: the song's Pokémon lives on the
progress bar instead of in a separate static box, and it really walks — a multi-frame
walk cycle — instead of the bob/flip approximation.

Success: on device, every song shows its Pokémon walking (real frames when a PMD sheet
exists, the current bob/flip walker otherwise) along a full-width HP bar, with no
flicker, no per-frame network/decode, and bounded RAM on the no-PSRAM ESP32.

## 2. Layout (320×240 landscape)

| Region | Rect | Content |
|---|---|---|
| Top bar | 0,0 → 320×22 | unchanged ("NOW PLAYING", device) |
| Album art | panel 8,26 104×104 | unchanged |
| Track panel | 118,26 194×48 | title, artist (unchanged) |
| Context panel | 118,78 194×52 | "From:" + context (ends y=130, aligned with art) |
| **Status panel** | **8,134 304×104** | merged Pokémon + HP + lyric (below) |

Status panel contents:
- **Row y≈138:** Pokémon name (type accent colour, ASCII-folded, truncated) +
  `No.NNNN` on the left; `HP m:ss/m:ss` (remaining/total) right-aligned.
- **Walk band y≈154–190:** the walker; feet rest on the bar top.
- **HP bar:** x16, y190, w288, h12 — drains from the left, green/yellow/red (unchanged
  semantics, longer bar).
- **Lyric area:** y≈204–236, full panel width, current synced line, up to 2 lines.

The old left Pokémon box and the static 48 px sprite are removed. Exact y values may
shift ±2 px during implementation to fit font metrics; region order is fixed.

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
| `ui/screen_now` | device | Merged status panel layout; walker draws the current frame. |
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
  bbox crop union, downscale to band, frame-skip under the cap).
- Device: serial shows `[walk] pmd …` for most songs and `[walk] fallback` when a sheet
  is absent; visual check that the Pokémon walks right on a full-width bar with no
  flicker; heap log stable across many track changes.

## 8. Out of scope

Touch controls, full lyrics screen, shiny/gender/form variants, directions other than Right.
