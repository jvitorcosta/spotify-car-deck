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

- Car power: brownout behaviour on noisy USB during engine start (`[boot] reset reason`
  now says "brownout" if it happens).
- Touch controls (play/pause/skip).
- Korean / emoji / Cyrillic lyrics (render blank today; spec non-goal).
