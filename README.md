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

## Build & flash

- Copy `include/config.example.h` to `src/config.h` and fill in the WiFi networks and the
  Spotify app credentials. Get a refresh token on a PC with `python .devtools/spotify_auth.py`
  (Spotify only allows loopback `http` redirects, so the on-device portal can't be used).
- Build/flash (this machine's wrapper): `.devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`.
- Host unit tests (Unity, compiled with g++ because `pio test -e native` is broken here):
  `.devtools\ntest.ps1 test\<suite>\<suite>.cpp <module.cpp>` — suites: theme, icon_map,
  animdata, walkanim, walkrect, textfit, walk, interp, lrc, text, dex, netplan.
- Regenerate the bundled Pokédex: `python tools/gen_dex.py`.

## Architecture

```
core 0: network task (src/core/nettask.cpp)            core 1: UI loop (src/main.cpp)
  Spotify poll 4 s / player 12 s                          snapshot AppState every frame
  per track, one step at a time (util/netplan):           redraw deck on new track generation
    album art -> lyrics -> walker -> prefetch next        take art / lyrics / walker from mailbox
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
- **Pure, host-tested modules** — `ui/theme`, `ui/icon_map`, `pokemon/dex`,
  `util/{animdata,walkanim,walkrect,netplan,textfit,interp,lrc,text}`.

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
8. **Open:** once, every TLS handshake failed with `-32512` (mbedTLS SSL_ALLOC_FAILED) after
   a song's album art arrived and never recovered; holding the compressed JPEG drops the
   largest block from 65 KB to 41 KB. Being investigated with a 10-minute logged session
   (`[net]` and `[heap]` lines on the serial port).

## Credits

See `CREDITS.md`.
