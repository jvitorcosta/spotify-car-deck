# PokeDeck — Spotify now-playing deck in Gen-3 Pokémon battle style

An ESP32 "Cheap Yellow Display" (ESP32-2432S028R, 320×240) that shows what's playing on
Spotify as a FireRed/Emerald battle screen: the song's random Pokémon walks along the HP
bar (remaining time), with album art, synced lyrics in the battle dialogue box, and pixel
icons for the playing device, shuffle and repeat. Built for the car, running off a phone
hotspot.

## Hardware

- ESP32-2432S028R (ESP32-WROOM-32, **no PSRAM**), ILI9341 320×240, XPT2046 touch, microSD slot.
- Optional microSD card: caches walk sheets and sprites so repeats load instantly
  (without one the deck works, it just re-downloads).

## Setup

Prerequisites: [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
(or the VS Code extension), Python 3, a Spotify account and a
[Spotify developer app](https://developer.spotify.com/dashboard) with the redirect URI
`http://127.0.0.1:8888/callback`.

1. Copy `include/config.example.h` to `src/config.h` (git-ignored) and fill in your WiFi
   networks (first match wins, e.g. phone hotspot first) and the app's client id and secret.
2. Get a refresh token on the PC: `python tools/spotify_auth.py` (opens the browser, writes the
   token into `src/config.h`; Spotify only allows loopback `http` redirects, so this can't run
   on the device).
3. Build and flash: `pio run -e esp32dev -t upload` (add `--upload-port <port>` if needed;
   `pio device list` shows ports). Serial log: `pio device monitor` (115200 baud).

Hardware bring-up without WiFi/Spotify: `pio run -e hwcheck -t upload` (display, colours,
orientation, touch; `src/hwcheck.cpp`).

## Development

- Host unit tests (Unity, compiled with g++ because `pio test -e native` doesn't work for this
  layout): `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`. Needs g++ on PATH
  (or `$env:MINGW_BIN`) and Unity once: `pio pkg install -e native -l throwtheswitch/Unity`.
  One suite: `tools\ntest.ps1 test\<suite>\<suite>.cpp <module.cpp ...>`.
- Generated, committed data (re-run only to update): Pokédex `python tools/gen_dex.py`,
  status sprite `python tools/gen_status_sprite.py [dex]`, CJK font
  `python tools/gen_cjk_font.py [unifont_all-*.hex.gz]` → `data/cjk16.bin`, genre badges
  `python tools/gen_genres.py`.
- Measure a session (`pip install -r tools/requirements.txt`):
  `python tools/capture_serial.py <port> 600 session.log`, then
  `python tools/analyze_session.py session.log` (step timings, per-track arrival times,
  byte RAM, failed allocations, TLS errors, restarts).
- `tools/patch_spotify_lib.py` runs before every build: it turns off the Spotify library's
  debug mode, which would print the refresh token and client secret over serial.

## Architecture

```
core 0: network task (src/core/nettask.cpp)            core 1: UI loop (src/main.cpp)
  Spotify poll 4 s / player 12 s                          snapshot AppState every frame
  per track, one step at a time (util/netplan):           redraw deck on new track generation
    art -> lyrics -> genre -> walker -> prefetch          take art / lyrics / genre / walker
  results tagged with trackGen                            animate: walker 8 fps, CD 6 fps, HP 4 fps
                 \                                        /
                  +---- src/core/shared.cpp (one mutex) -+
                        AppState + per-track mailbox
```

- **UI loop** draws only: battle layout (`ui/screen_now`, `ui/battle`, `ui/icons`), album
  art JPEG decode, walker, CD, lyric.
- **Network task** does every HTTPS / SD / PNG decode. It never touches the display.
- **Shared state** — one FreeRTOS mutex; `AppState` snapshots plus a mailbox whose results
  carry the track generation they were fetched for, so a quickly skipped song's art or
  lyrics are dropped instead of shown on the next one.
- **Walker** (`images/walksprite`) — PMD SpriteCollab walk cycle (Right-facing row) cropped
  to a shared bounding box and fitted to a 32 px band. Two slots: the UI reads the active
  one while the network task loads the next song's walker into the staged one. Fallback:
  the PokeAPI sprite with a bob/flip fake walk.
- **Pokémon** — random #1–1025 from a bundled Pokédex (`pokemon/dex`, generated).
- **Lyrics** (`lyrics/lrclib`, `util/lrcstream`, `util/lyricbuf`) — LRCLIB `/api/get`, then
  `/api/search`; synced beats plain; retries on server errors; the dialogue box shows
  Pokémon-style status messages (`ui/lyricmsg`) when there is no line to show.
- **Genre badge** (`genre/apple`, `util/genre`, `ui/genrebadge`) — Apple artist genre, cached
  per artist, mapped to Gen-3 style badges.
- **Pure, host-tested modules** (one suite each under `test/`) — `ui/{theme,icon_map,accents,
  cjkfont,status_sprite,typebadge,genrebadge,lyricmsg,pokeball,labels}`, `pokemon/dex`,
  `util/{animdata,walkanim,walkrect,netplan,lyricstatus,interp,text,glyphrun,lrcstream,
  lyricbuf,artmap,ctxcache,dexset,genre}`.

## Design & performance history

1. **v1 — single loop.** Every HTTPS call ran inline in `loop()`. On track change: PokéAPI
   (name/type), album art, LRCLIB lyrics, sprite. Between polls, the deck interpolated
   progress locally.
2. **Walker v1.** The PokeAPI sprite bobbed and flipped along the HP bar (fake walk).
   A dirty-rect blit (`util/walkrect`) removed a blink every ~9 px of travel.
3. **PokéAPI heap failures.** Buffering the multi-hundred-KB `/pokemon/N` JSON with
   `getString()` failed (`IncompleteInput`) on a fragmented heap → switched to HTTP/1.0 and a
   filtered stream parse.
4. **Gen-3 battle UI + PMD walk cycle.** Real multi-frame walk, battle boxes, HP tag,
   dialogue box, pixel icons. The 16 KB frame pool had to move from static DRAM to the heap
   (the static segment overflowed by 4 KB).
5. **Measured stutter (2026-10-06).** The user saw multi-second freezes and slow track
   changes. Timing around every blocking call:

   | Step | Before (inline in `loop()`) | After |
   |---|---|---|
   | Spotify now-playing poll (every 4 s) | ~1.5 s **UI freeze** | 0 s UI freeze (core 0) |
   | Spotify player poll (every 12 s) | ~1.4 s **UI freeze** | 0 s UI freeze (core 0) |
   | PokéAPI name + type | 4.2 s | 0 s (bundled dex) |
   | Album art | 2.5 s, blocks UI | ~2.8–3.5 s in background |
   | Lyrics | 3.8 s, blocks UI | ~2.4–3.9 s in background |
   | PMD walk sheet | 3.5 s, blocks UI | 0 s when prefetched (~4 s during the previous song) |
   | New song visible after a skip | **~19 s** | title + Pokémon + walker at the next poll (≤ ~5.5 s); art ~3 s later, lyric ~6 s later |

   Drawing was never the problem (`drawNow` 124 ms, album-art decode 118 ms).
   **Root cause:** one task did both drawing and synchronous TLS requests.
6. **Fixes:** network task on core 0 with a shared mailbox; progressive track change
   (title first, media as it arrives); bundled Pokédex; prefetch of the next walker.
7. **Memory fallout of the split (found on device).** Two 16 KB walker slots plus a 16 KB
   task stack left the largest free block at ~41 KB, and album-art `malloc` failed while a
   TLS session was open (`[fetch] no heap for 24072 bytes`). Fixes: slots 16 → 10 KB, task
   stack 16 → 10 KB (measured high-water mark 5.3 KB); boot heap went from free 86 KB /
   largest 41 KB to free 143 KB / largest 65 KB. A single failed poll (`HTTP -1`) also flashed
   "No signal", now shown only after 3 consecutive failures (`netplan::LinkGate`).
8. **The freeze (resolved in 9).** A 10-minute logged session showed the deck stuck for
   **500 of 600 s**: right after a song's album art arrived, every TLS handshake failed with
   `-32512` (mbedTLS SSL_ALLOC_FAILED) and never recovered. A failed-allocation hook showed
   why: the 16 717-byte mbedTLS buffer found at most 9.7–16.4 KB contiguous, and the WiFi
   driver couldn't even get 1 512-byte packet buffers (230 times). The "~100 KB free" we had
   been logging (`ESP.getFreeHeap`) included 32-bit-only IRAM that byte buffers can't use —
   **byte-addressable RAM was exhausted**, and nothing freed memory except a track change,
   which itself needed a successful poll.
9. **Deterministic memory + resilience.**
   - Measure the right thing: `core/mem` reports `MALLOC_CAP_8BIT | INTERNAL`; failed
     allocations are counted (printing them from the WiFi driver slowed it down).
   - Album art streamed over **plain HTTP** (the CDN serves it) straight into the JPEG decoder
     and into one fixed **92×92 bitmap** allocated at boot — no held JPEG, no decode buffer,
     no TLS. (A since-fixed bug left the decoder's swap flag uninitialised: scrambled colours.)
   - Walker downloads use one fixed **12 KB scratch** buffer; the fallback sprite decodes in
     two passes without a full-size copy; PNGs PNGdec can't buffer are skipped.
   - Lyrics: synced field only, streamed, parsed into a fixed **~5.6 KB arena**.
   - Network steps wait until the largest free block fits TLS; **self-healing**: 2 failed
     polls pause optional downloads, 180 s without a good poll (WiFi up) restarts the board.
   - Late hotspot retried in the background, 8 s TLS timeouts, "No signal" once the last good
     poll is 20 s old, track changes keyed on the Spotify track URI.

   | | Before (10-min session) | After (270 s, 3 tracks) |
   |---|---|---|
   | Time frozen | 500 of 600 s | 0 |
   | Failed allocations | 340 TLS + 230 WiFi | 0 TLS; 132 small (34 WiFi RX buffers, 98 ≤ 2.3 KB) |
   | Album art | 2.5–3.3 s (TLS) | 0.9–1.2 s (plain HTTP) |
   | Lyrics | 3.7 s | 2.4–2.7 s |
   | Lowest byte RAM free / largest block | 2 KB / 0.7 KB | 32 KB / 20.5 KB (during TLS) |
   | After a skip | old song ~19 s, then stuck | title at next poll; art 1.2 s, lyrics 3.9 s, walker instant if prefetched |

   Measured with `tools/capture_serial.py` + `tools/analyze_session.py`.
10. **UI additions:** bundled Pikachu on the status screens (`tools/gen_status_sprite.py`),
    "PAUSED" in the top strip, Portuguese/Latin-1 accents drawn as pixel marks over the ASCII
    font (same widths, so fitting and wrapping are unchanged).
11. **Japanese / Chinese text.** Kana and hanzi/kanji lyric lines rendered empty: the fonts are
    ASCII-only and the accent folding dropped every 3-byte UTF-8 character. GNU Unifont glyphs
    (16 px tall, like font 2) for kana, CJK punctuation, half/full-width forms and all CJK
    Unified Ideographs — 21 504 glyphs, **686 KB** — are embedded in flash
    (`tools/gen_cjk_font.py` → `data/cjk16.bin`, `board_build.embed_files`) and read on demand:
    **0 bytes of RAM for the font** (flash 42 % → 57.6 %). Text is decoded into glyph runs
    (font-2 ASCII + accent marks, Unifont CJK), measured exactly and wrapped between CJK
    characters (no spaces). The lyric buffers cost ~1.7 KB of byte RAM.
    10-minute session afterwards: 0 TLS errors, 0 failed polls, 0 restarts; byte RAM flat at
    66–68 KB free / 34.8 KB largest block. 2 811 WiFi receive buffers (1 512 B) could not be
    allocated during traffic bursts — dropped packets that TCP resends. (This line first said
    "0 failed allocations": the analyzer only counted the old per-failure `[allocfail]` lines,
    not the counts in `[mem]` lines. Fixed in `tools/analyze_session.py`.)
12. **Watchdog reboots and leaked secrets.** The one "unexplained" `rst:0xc` was the task
    watchdog: on a reset TLS connection, SpotifyArduino's response loops spun ~11 s on core 0.
    Their `yield()` never lets the lower-priority idle task run, so it missed the 5 s watchdog.
    The watchdog is now 30 s (a real hang still reboots), and every boot logs
    `[boot] reset reason: …`. A 12 s busy-spin injected into the network task no longer
    resets the board. The same capture showed the library prints its token request on every
    refresh — refresh token, client id and client secret — because it hard-codes
    `SPOTIFY_DEBUG`; `tools/patch_spotify_lib.py` (a pre-build script) turns it off.
    Smaller fixes from the review backlog: failed playlist-name lookups retry after 60 s
    (`util/ctxcache`); Pokémon whose walk sheet can never work (404, too big, too few rows)
    are remembered for the session instead of re-downloaded on every pick (`util/dexset`);
    the walk-sheet decode stops after the rows it needs; long local-file URIs keep a hash
    so they stay distinct (`txt::copyId`); the font bounds check can't wrap around on 32-bit;
    alloc-fail counters are atomic and in IRAM; `[mem]` lines show the network task's free
    stack (lowest seen: 3.7 KB of 10 KB).
13. **Missing lyrics.** Lyrics went missing even when Spotify had them: LRCLIB had only plain
    (untimed) text for some songs, returned 503s now and then (one try per song), and its exact
    lookup misses slightly different titles/durations. The reader now also keeps plain lyrics
    (spread over 10–90% of the song, drawn without note icons) and the instrumental flag, falls
    back to `/api/search` (entry chosen by duration ±3 s, 48 KB cap), and retries temporary
    errors after 5 s and 20 s. The dialogue box is never empty: Pokemon battle-text messages
    while searching ("PIKACHU used SING! Searching for the verses...") or retrying, a 1.5 s
    "It's super effective! Lyrics found!" when lyrics arrive, then the upcoming first line
    during the intro, and "But it failed!" then "PIKACHU is enjoying the music" when there are
    none (`ui/lyricmsg`). Every attempt logs
    `[lyrics] "<title>" / <artist>: <result> via get|search (try N)`. In the 10-minute session
    afterwards one song got 503, 503, then synced lyrics on the third try, and one plain-only
    song showed 46 lines; before, both would have shown an empty box.
14. **Genre badge.** Spotify no longer returns artist genres to this app, so the genre comes
    from Apple's iTunes Search API (artist search, no key, ~300 B): one request per new artist
    alongside the lyrics (~1.5 s on the network task; it also fills a lyrics-retry wait),
    32 artists cached. Apple's 478 genre ids map
    to 31 Gen-3 style badges (`tools/gen_genres.py` → `src/ui/genre_data.inc`: subgenres take
    the parent's badge, with overrides such as Baile Funk → FUNK BR); unknown ids show a grey
    `???`, a nod to Gen 3's mystery type.

## Credits

See `CREDITS.md`.
