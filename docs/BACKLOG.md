# Backlog

Open items, not scheduled. Newest decisions first within each section.

## Verification (on device)

- **iPhone hotspot.** Saved as the second network in `src/config.h`; never connected yet.
  Needs "Maximize Compatibility" on (the ESP32 is 2.4 GHz only). Also exercises the
  half-dead link (WiFi up, no mobile data) path, which has never run on the board.

## Investigate

- **Dropped WiFi receive buffers.** ~4.7/s during a normal session (2 811 in 10 min), mostly
  around track changes while art, lyrics and the walker download. TCP resends them and every
  poll still succeeds, but each drop adds latency. Byte RAM shows 67 KB free / 34.8 KB largest
  at log time, so the shortage is momentary (several TLS sessions + RX bursts). Ideas: fewer
  concurrent TLS buffers, smaller TCP window, spacing the per-song downloads.

## Lyrics polish (deferred minors, final review 2026-10-08)

- Search can never match a track whose duration is 0 (lyrics wouldn't advance anyway: the
  position is clamped to the duration).
- HTTP 200 with a non-JSON body (captive portal / CDN page) is a final "not found": no retry.
  Fix idea: record whether the reader saw `{` or `[`; 200 without one -> temporary error.
- No overall time limit per lyrics request: the 8 s stall timer resets on every byte, so a
  trickling server can hold the network task (and the polls). A ~15 s deadline would bound it.
- `test_all_messages_fit_two_lines` checks bytes, not rendered pixel width (worst case: "...").

## Genre badge polish (deferred minors, final review 2026-10-08)

- The Genre step can block the network task ~24 s worst case on a bad link (8 s handshake +
  8 s GET + 8 s stall); a 3 s stall limit would do for a ~300 B answer.
- No stack high-water mark recorded for the genre path (`term[300]` + `url[400]` on the 10 KB
  network-task stack); making them `static` would remove the question.
- Test gaps vs the spec: real Apple answers for Pop 14 / Brasileira 1122, every badge's colour,
  and "network errors are not cached" (device code in nettask).
- The artist cache is FIFO, not LRU: an artist on repeat is evicted after 32 newer ones.

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

## Accepted as is

- If WiFi only comes up after boot, the Spotify setup portal is skipped. Only matters for a
  first-ever setup with no saved login; a reboot with WiFi up fixes it.
- `g_accessToken` is an Arduino `String`, reassigned once an hour: negligible heap churn.
- Backlight stays at full brightness (decided 2026-10-07): no auto-dim from the CYD light
  sensor (GPIO 34), even though it was suggested for night glare.

## Ideas

- **Musixmatch as a second lyrics source** (parked 2026-10-07). Official API free plan: API
  key, ~2k calls/day, only a ~30% untimed preview (full/synced lyrics are paid, licensed).
  Unofficial desktop endpoint (`apic-desktop.musixmatch.com`, `macro.subtitles.get` +
  usertoken): full synced lyrics, but bypasses their API auth (against Musixmatch's terms),
  can be blocked any time, token tied to an account; large JSON responses would need a
  second stream parser and another TLS host on a memory-tight board.
- **Bigger status panel when a song has no lyrics** (parked 2026-10-07; current layout kept).
  Instead of the dialogue box, the status panel grows into the bottom of the screen. Two
  mocked-up variants: (A) everything scales up: name/badge/time in the large font, Poke Ball
  and walker at 2x (pixel doubling), 14-16 px HP bar; (B) name row unchanged, a large
  time-remaining readout next to a 2x walker, thicker HP bar. The lyrics status
  (lyricstatus::Status::None / Instrumental) now reports that final state.
- Car power: brownout behaviour on noisy USB during engine start (`[boot] reset reason`
  now says "brownout" if it happens).
- Touch controls (play/pause/skip). The unused groundwork (Spotify play/next/prev/volume
  calls, button widgets) was removed in the 2026-10-08 cleanup; see git history.
- Korean / emoji / Cyrillic lyrics (render blank today; spec non-goal).
