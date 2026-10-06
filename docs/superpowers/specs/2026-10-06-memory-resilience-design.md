# Deterministic Memory + Resilience — Design

Date: 2026-10-06. Extends `2026-10-06-perf-network-task-design.md`. Approved scope: items 1–6
below (user, after the 10-minute logged session and the whole-branch review).

## 1. Problem (measured)

10-minute logged session: the deck was stuck for **500 of 600 s**. From t≈99 s, right after a
track's album art arrived, every TLS handshake failed (`-32512`, mbedTLS SSL_ALLOC_FAILED, 340×)
and never recovered; no further track change was detected.

A failed-allocation hook (`heap_caps_register_failed_alloc_callback`) showed what failed:

| Failed allocation | Caps | Largest block of that kind | Free of that kind |
|---|---|---|---|
| 16 717 B (mbedTLS input buffer) | INTERNAL \| 8BIT | 9.7–16.4 KB | 34–43 KB |
| 1 512 B (WiFi/lwIP packet buffer), ×230 | INTERNAL \| DEFAULT | 0.7–1.5 KB | 2–6 KB |

The "~100 KB free / 41 KB largest" logged so far (`ESP.getFreeHeap/getMaxAllocHeap`) includes
32-bit-only IRAM that byte buffers cannot use. **Byte-addressable internal RAM** was exhausted,
so even the WiFi driver could not receive packets. Long-lived/large consumers: two 10 KB walker
slots, the held album-art JPEG (20–60 KB, whole song), the UI's 21.6 KB JPEG decode buffer (same
moment the next TLS starts), the fallback sprite's ~27 KB full-size scratch, scattered lyric
`std::string`s, per-download `malloc(len)` with TLS open. Nothing frees memory except a track
change, which needs a successful poll → self-sustaining failure (review: Critical #1).

## 2. Goals

- Byte-addressable RAM use is **fixed after boot**: large buffers are allocated once in `setup()`
  before WiFi; steady state never `malloc`s more than a few KB besides TLS itself.
- No permanent freeze: the deck always recovers (worst case: automatic restart).
- Review Important findings fixed: late hotspot, half-dead link, same-title tracks, PNG pitch
  overflow, corrupt cached fallback sprite.

## 3. Design

### 3.1 Right metric and gating (item 4)
- `core/mem`: `byteFree()` / `byteLargest()` = `heap_caps_get_free_size` /
  `heap_caps_get_largest_free_block` with `MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL`;
  `mem::log(where)` prints `[mem] <where> free=… largest=…`. The failed-allocation hook stays
  permanently (`[allocfail] …`).
- Pure gate `netplan::canRun(step, largestByteBlock)`: TLS steps (lyrics, walk, prefetch) need
  `largest ≥ TLS_NEED (20 000)`; optional steps (walk, prefetch) need `TLS_NEED + 8 000`; art over
  plain HTTP needs `6 000`. A step that can't run waits for the next loop (polls always first).

### 3.2 Album art streamed into a fixed bitmap (item 1)
- `images/art`: one **92×92 RGB565 bitmap (16 928 B)** + tjpgd workspace (3 500 B), allocated in
  `art::begin()` at boot.
- Network task: GET the cover over **plain HTTP** (`https://i.scdn.co/…` → `http://i.scdn.co/…`;
  verified 200), feed the stream straight into tjpgd (`jd_prepare` with a stream reader, 512 B
  internal buffer), decode at the largest scale whose short side is ≥ 92 px, and
  nearest-neighbour **cover-fill** each decoded block into the bitmap (pure `util/artmap`).
  Pixels are stored big-endian (same as every other `pushImage` in the firmware).
- Hand-off: the net task marks the art invalid (under the shared lock) before writing, and posts
  `art ready for gen G` after. The UI pushes the bitmap (under the lock) only when the art is valid
  for the track on screen — on arrival and on full redraws.
- Removes the held JPEG (20–60 KB), the UI decode buffer (21.6 KB) and one TLS session per track.

### 3.3 Walker without large transients (item 2 + review #7/#8)
- One **16 KB download scratch** allocated in `walk::begin()`; XML, PMD sheet and fallback sprite
  are read into it (`fetch::httpsGetInto`, `cache::readInto`) instead of `malloc(len)`; anything
  larger is skipped (fallback / no walker).
- The fallback sprite uses the same two-pass row decode as PMD (bbox pass, then scaled write) —
  no full-size copy. Non-alpha sprites key out the top-left pixel colour.
- Pure `walkanim::pngFits(width, pixelType, bpp)`: reject when PNGdec's two-line buffer
  (`2·(pitch+16)`) exceeds `PNG_MAX_BUFFERED_PIXELS` (2 562). "Unsupported" sheets are **not**
  deleted from the cache; corrupt ones are (PMD and fallback sprite).

### 3.4 Lyrics in a fixed arena (item 3)
- Pure `util/lyricbuf`: `Lyrics { n; Line{tMs, off}[256]; char text[6144]; }` (8 KB, one static
  instance). `parse(lrc, out)` keeps tagged lines in time order, drops what doesn't fit.
- LRCLIB: HTTP/1.0 + filtered stream parse of **`syncedLyrics` only**, then parse into the arena.
  Single instance: the net task marks lyrics invalid (under the lock) before parsing and posts
  `lyrics ready for gen G` after; the UI copies the current line under the lock.
- Replaces `util/lrc` (`std::vector<std::string>`), whose cases move to `test_lyricbuf`.

### 3.5 Self-healing ladder (item 5)
Pure `netplan::Health::onPoll(ok, wifiUp, nowMs)`:
- success → reset; failures with WiFi down → nothing (WiFi reconnect handles it);
- 2 consecutive failures with WiFi up → **PauseOptional** (no lyrics/walk/prefetch until a poll
  succeeds);
- no successful poll for **180 s** with WiFi up → **Restart** (`ESP.restart()`, logged first).

### 3.6 Robustness (item 6)
- **Late hotspot:** the network task always starts; it connects WiFi itself and calls
  `spclient::begin()` the first time WiFi is up.
- **Half-dead link:** 8 s TLS handshake timeout and 8 s HTTP timeouts on every client; the UI shows
  "No signal" when the last successful poll is older than 20 s (in addition to `LinkGate`).
- **Track identity:** `TrackGen` is fed the Spotify track URI (`AppState::trackUri`), falling back
  to the name when the URI is empty.

## 4. Memory budget (allocated once in `setup()`, before WiFi)

| Buffer | Bytes |
|---|---|
| Walker slots 2 × 10 240 | 20 480 |
| Walker download scratch | 16 384 |
| Album-art bitmap 92×92×2 + tjpgd workspace | 20 428 |
| Lyrics arena | 8 192 (static) |
| Network task stack | 10 240 |

Task 14 measures byte-addressable free/largest after WiFi with this budget; the plan shrinks the
scratch to 12 KB if steady-state largest falls below 30 KB.

## 5. Testing

- Host: `test_artmap`, `test_lyricbuf`, `test_netplan` (canRun, Health), `test_walkanim`
  (pngFits).
- Device: boot/steady `[mem]` budget; no `[allocfail]` during a 10-minute logged session with
  skips; art appears; walker loads or falls back; forced restart path observed by blocking polls
  (temporary) once; late-hotspot boot.
