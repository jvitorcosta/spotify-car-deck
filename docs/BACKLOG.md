# Backlog

Open items, not scheduled. Newest decisions first within each section.

## Verification (on device)

- **iPhone hotspot.** Saved as the second network in `src/config.h`; never connected yet.
  Needs "Maximize Compatibility" on (the ESP32 is 2.4 GHz only). Also exercises the
  half-dead link (WiFi up, no mobile data) path, which has never run on the board.
- **One unexplained `rst:0xc`.** A single software reset on one early boot; never seen again,
  including the 10-minute session. Watch for it in longer runs.

## Parked: SD card cache (not used)

Decided 2026-10-07: the deck runs without a card. The cache code stays; with no card every
call is a no-op and the walker is downloaded each time (~25 KB/song, usually prefetched).

- What a card would add: walk sheets (`/pmd/<dex>.xml|.png`) and fallback sprites
  (`/sprites/<dex>.png`) load in ~0.2 s on repeat songs, with no download. Album art and
  lyrics are never cached.
- Never verified: SD (VSPI) alongside the display (moved to HSPI in `2b75c40`), cache hits,
  and corrupt-file eviction.
- To test: FAT32 card in, power-cycle, look for `[cache] SD ready`, replay songs and compare
  `[net] walk` times; check `/pmd` and `/sprites` on a PC.

## Code polish (deferred review minors)

- Log the network task's stack high-water mark (stack cut to 10 KB; peak seen 5.3 KB).
- Make the alloc-fail hook counters atomic / IRAM-safe (diagnostics only).
- 32-bit overflow in the `cjkfont` bounds checks (low risk: the font is generated at build time).
- Walk sheets with fewer than 3 direction rows are treated as corrupt and re-downloaded on every
  pick; oversized sheets have no "too big" marker.
- `decodeRegion` keeps decoding after the rows it needs.
- If WiFi only comes up after boot, the setup portal is skipped.
- `trackUri[64]` can truncate long local-file URIs (identity collisions).
- Heap churn: `g_lastLyric` is a `std::string`, `g_accessToken` is a `String`.
- `resolveContext` caches the URI even when the lookup failed (playlist name stays "Playlist").

## Ideas

- Car power: brownout behaviour on noisy USB during engine start.
- Backlight auto-dim at night.
- Touch controls (play/pause/skip).
- Korean / emoji / Cyrillic lyrics (render blank today; spec non-goal).
