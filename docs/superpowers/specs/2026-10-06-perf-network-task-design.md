# Smooth Deck: Network Task, Progressive Track Change, Bundled Pokédex, Prefetch — Design

Date: 2026-10-06. Extends `2026-10-06-merged-status-panel-pmd-walker-design.md`.

## 1. Problem (measured on device, 2026-10-06)

Timing instrumentation around every blocking call in `loop()` (single core, all I/O inline):

| Step | Time | Cadence |
|---|---|---|
| Spotify now-playing poll (`spclient::poll`) | ~1.5 s | every 4 s |
| Spotify player details (`pollPlayerDetails`) | ~1.4 s | every 12 s |
| PokéAPI `/pokemon/N` (name + type) | 4.2 s | per track |
| Album art download | 2.5 s | per track |
| LRCLIB lyrics | 3.8 s | per track |
| PMD walk sheet (XML + PNG) | 3.5 s | per track |
| `drawNow` + album-art decode | 0.24 s | per track |

Effects: the UI freezes ~40 % of the time during a song (walker jumps, CD stops, 3 s
stalls when both polls coincide), and a track change keeps the old song on screen for
**~19 s**, then everything appears at once. Root cause: every network call is a fresh
TLS handshake executed synchronously on the only task that draws. Drawing is cheap.

## 2. Goals

- No visible freeze during a song: walker, CD and HP bar animate continuously.
- On track change, the new title/artist and Pokémon appear within one poll (~1.5 s);
  walker, album art and lyric fill in as they arrive.
- No PokéAPI request per track.
- The next song's walker is usually ready before the song starts.
- The history above (problem, measurements, fixes, results) is written down in the
  README and at the relevant code.

## 3. Design

### 3.1 Two tasks, one owner per resource

- **Network task** (FreeRTOS, pinned to **core 0**, 16 KB stack, priority 1): owns WiFi
  upkeep, the Spotify client, all HTTPS/SD I/O, PNG decoding of walk sprites, LRC
  parsing. It never touches the display.
- **UI loop** (Arduino `loop()`, **core 1**): owns the TFT exclusively (all drawing,
  JPEG decode of album art). It never does network or SD I/O.
- Setup (WiFi connect, Spotify auth) stays synchronous in `setup()`; the network task
  starts after it.

### 3.2 Shared state (`core/shared`)

One FreeRTOS mutex guards:
- `AppState` published by the network task after every poll (UI copies a snapshot
  each frame). New field `uint32_t trackGen` increments on every track change.
- A **media mailbox** for the current `trackGen`: album-art JPEG bytes (ownership moves
  to the UI), parsed lyric lines (ownership moves to the UI), "walker promoted" flag.
- Results are tagged with the `trackGen` they were fetched for; the network task drops
  (frees) a result whose generation is no longer current — rapid skipping never shows
  the previous song's art/lyrics/walker.

### 3.3 Network task schedule (`util/netplan`, pure)

- Poll now-playing every 4 s and player details every 12 s (unchanged cadence).
- Track change detection is pure (`TrackGen`): a different non-empty track name bumps
  the generation and resets the per-track work list; the same name does not.
- Between polls the task does **one work step at a time**, in this order: walker (only
  if the prefetched one didn't match), album art, lyrics, prefetch next walker. One step
  per iteration keeps polls on schedule.
- On track change the task first picks the Pokémon (bundled dex, instant), promotes the
  prefetched walker if present, and **publishes immediately**, before any download.

### 3.4 Progressive UI

- Snapshot `trackGen` differs from the one on screen → full `drawNow` at once (text,
  Pokémon name, boxes, empty art box, empty dialogue box).
- Each frame the UI checks the mailbox: art bytes → `setAlbumArt` + `drawAlbumArt`;
  lyric lines → swap in; walker promoted → walker starts drawing.
- Animations (walker 8 fps, CD 6 fps, HP 4 fps) run every frame regardless of network.

### 3.5 Bundled Pokédex (`pokemon/dex`, pure)

- `tools/gen_dex.py` (dev-time) downloads PokéAPI CSVs
  (`pokemon_species_names.csv` English, `pokemon_types.csv` slot 1, `types.csv`) and
  writes `src/pokemon/dex_data.inc`: 1025 `{name, typeIndex}` entries, names folded
  to ASCII at generation time (`♀`→` F`, `♂`→` M`, accents removed).
- `dex::name(n)`, `dex::type(n)` (lowercase type name, matches `theme::typeColor`),
  `dex::spriteUrl(n, buf, len)`; out-of-range → empty string. ~20 KB of flash.
- Replaces `pokeapi::pickRandom` (no network). The PokeAPI sprite repo is still used
  for fallback sprites.

### 3.6 Prefetch (walk store double slot)

- `walk::` gets two pools: **active** (read by the UI) and **staged** (written by the
  network task). Loaders always write the staged slot; `walk::promote()` swaps the two
  under the shared mutex. The UI holds the mutex while composing a walker frame.
- After a track's art and lyrics are done, the network task picks the next dex and
  loads its walker into the staged slot. On the next track change that dex becomes the
  song's Pokémon and the slot is promoted instantly. If no prefetch is ready, it picks a
  new dex and loads it as the first work step.
- Both pools are allocated once at start-up (2 × 16 KB) before the heap fragments.

### 3.7 Logging and documentation

- Permanent, cheap timing logs in the network task: `[net] poll 1523ms`,
  `[net] art 2488ms`, `[net] walk pmd 3519ms`, `[net] prefetch #N 3400ms`, plus
  `[heap]` on each track change, so the history can be re-measured.
- `README.md` (new): what the deck is, hardware, build/flash, architecture (two tasks,
  shared state, modules), and a **Design & performance history** section: original
  single-loop design, the measurements in §1, each fix and why, and the after
  measurements from device verification.
- Short "why" comments at the network task, shared state, prefetch and dex modules
  that point to the README history.

## 4. Error handling

- Network/decode failures leave that piece empty (no art / no lyric / fallback walker);
  the deck keeps animating. Stale-generation results are freed, never shown.
- If the staged pool or a download fails, the next track simply loads synchronously in
  the network task (still off the UI core).

## 5. Testing

- Host: `test_dex` (known names incl. Nidoran F / Mr. Mime / Flabebe, types, bounds,
  1025 entries, every name ASCII), `test_netplan` (TrackGen bump/no-bump/empty, step
  order, reset on new generation, prefetch only after art+lyrics).
- Device: `[net]` logs show polls no longer delay drawing (walker moves smoothly while
  `[net] poll` lines print); track change shows new title within one poll; after one
  full song, the next track logs a promoted prefetch and the walker appears with the
  title; rapid skipping 3× never shows a previous song's art/lyric.

## 6. Out of scope

TLS session reuse / keep-alive inside SpotifyArduino; SD card hardware.
