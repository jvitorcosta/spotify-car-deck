# Code review and cleanup plan — 2026-10-08

Whole-repository review: best-practice research (sources below), a read-only audit of the
firmware (`src/`, `include/`) and of the repository (tests, tools, docs, build config). Every
finding has file evidence; the key ones were spot-checked.

## Ground rules

- **Behaviour-preserving only.** No change to what the deck shows or does, to timing, RAM or
  flash budgets. Each phase is checked against a baseline (below) before it is committed.
- Anything that *would* change behaviour or performance is listed separately (sections G and H)
  and needs its own decision.
- Items already in `docs/BACKLOG.md` are marked *(backlog)*.

## Baseline to hold (Phase 0)

Record before any change, compare after every phase:
- `pio run -e esp32dev` RAM and flash numbers (RAM 35.0 % at `6836503`).
- `tools/run_tests.ps1` → 25 suites, all green.
- A 5-minute board session (`tools/capture_serial.py` + `tools/analyze_session.py`): byte RAM
  ~65–67 KB free / 34.8 KB largest, no watchdog, no restarts, lyrics + genre per track.

Gate for every phase: tests green, firmware builds, **RAM and flash equal or lower**, board
session matches the baseline (phases touching device code), one Conventional Commit per phase.

---

## A. Remove — unused code and leftovers

| Item | Evidence | Note |
|---|---|---|
| `AppState::popularity` | `include/app_state.h:13`, no reader/writer | −4 B per AppState copy |
| `spclient::poll()` return value + `trackChanged` name compare | `client.cpp:15,26,131,148`; caller ignores it (`nettask.cpp:160`) | track change is detected by URI in `netplan::TrackGen` |
| `cache::has(int)`, `cache::open(int)`, `hasPath`, `<FS.h>` | `images/cache.h:21,24`, `cache.cpp:50,52` | no callers |
| `spauth::saveRefreshToken` in the header | `spotify/auth.h:5` | make file-local `static` |
| `drawText(..., font, ...)` parameter | `ui/textdraw.h:12`, `(void)font`; every caller passes 2 | 5 call sites |
| `SCREEN_W/SCREEN_H`, `GBA_GOLD` | `include/pins.h:22-23`, `ui/theme.h:8` | unused constants |
| `#include "ui/theme.h"`, `<SPI.h>` in `main.cpp` | `main.cpp:2,12` | unused / redundant |
| Redundant `g_info[...].ready = false` | `images/walksprite.cpp:192,224` | already cleared on entry |
| Touch-control code: `togglePlay/next/prev/setVolume`, `ui/widgets.*`, `NowButtons`/`nowButtons()` | `client.cpp:176-186`, `screen_now.*` | **decision D1**: remove, or keep with a `// touch controls (backlog)` note *(backlog: touch)* |
| `XPT2046_Touchscreen` in the firmware `lib_deps` | `platformio.ini`; only `hwcheck.cpp` uses it | move to `[env:hwcheck]` |
| Merge `test_walk` into `test_interp` | both test only `util/interp` | one suite |

## B. Deduplicate (no runtime change: inline/constexpr helpers, no new buffers)

| Item | Sites |
|---|---|
| HTTPS client setup (`setInsecure`, 8 s handshake, `useHTTP10`, 8000 ms) → header-only `net/http.h` with named constants + `inline` setup | `fetch.cpp`, `lrclib.cpp`, `apple.cpp`, `client.cpp` ×2, `art.cpp` |
| Two URL encoders → `txt::urlEncode` (bounded, no heap String) | `lrclib.cpp:12-20` vs `util/text.cpp` |
| "strncpy + terminate" ×12 → null-safe `txt::copy(dst, src, n)` (host-tested) | `client.cpp`, `shared.cpp`, `pick.cpp`, `ctxcache.cpp`, `netplan.cpp`, `artmap.cpp`, `lyricmsg.cpp`, `screen_now.cpp` |
| Three case-insensitive compares → one helper in `util/text` | `icon_map.cpp:50`, `typebadge.cpp:11`, `theme.cpp:24-27` |
| 18 type names listed twice | `typebadge.cpp:7-9`, `theme.cpp:9-19` |
| RGB565 byte swap ×4 → `constexpr theme::be()` | `icons.cpp:7`, `screen_now.cpp:87,140`, `art.cpp:59` |
| Refresh-token fallback ×2, redirect URI ×3, User-Agent ×2 | `client.cpp:46,67`, `auth.cpp:26,36,65`, `lrclib.cpp:35`, `apple.cpp:26` |
| Coupled constants → `static_assert` | `ART_W == art::W`; `walk::MAX_FRAMES == animdata::MAX_FRAMES` |
| Stream-read loop ×3 (only if it stays inline, buffers stay with callers) | `fetch.cpp`, `lrclib.cpp`, `apple.cpp` |

## C. Consistency

- One prefix for file-static state (`s_`): today `g_`/`s_`/none are mixed, even within `art.cpp` and `walksprite.cpp`.
- `constexpr` for file constants (`static const int` in `screen_now.cpp`, `walksprite.cpp`, `main.cpp`).
- Include order (project, then system) and one include style; `pick.cpp` gets `Serial` only via `app_state.h`.
- Named timing constants: 4000/12000/20 (`nettask.cpp`), 20000/160/250/330/120 (`main.cpp`), 8000/5000 (`wifi.cpp`), art gate 6000 (`netplan.cpp`).
- Formatting outliers: `theme.cpp:20-30`, `interp.cpp` (two `namespace` blocks), `client.cpp:184` (two `if`s on one line).
- Logging: `[genre]` lines are printed from `nettask.cpp` instead of `genre/apple.cpp`; `println`/`printf` mixed.
- Namespaces vs file names (`img`/`png.h`, `walk`/`walksprite.h`, `net`/`wifi.h`, three genre modules): **document, don't rename** (high churn).

## D. Stale comments and docs

- Code: glued comments `nettask.cpp:32`; stack note `nettask.cpp:204` (says 5.3 KB of 16 KB; now ~6.3 of 10 KB);
  "Task 14-opt" `client.cpp:34`; plan/task references `nettask.cpp:33`, `screen_now.h:6`, `pins.h:2`, `screen_now.cpp:23`;
  duplicated doc comments `screen_now.h:13-15,30-31`; `netplan.h:10` (TrackGen is fed the URI);
  `text.h:5-8` (`asciiFold` is test-only now); `lrclib.h:11` ("never throws").
- README: module list names `util/textfit`, `util/lrc` (gone) and misses ~15 modules; test-suite list stale
  (lists `textfit`, `lrc`; misses 15 suites) → point to `tools/run_tests.ps1`; item 14 wording *(backlog)*.
- `platformio.ini`: `[env:native]` comment says modules live in `lib/` (they don't); "final review, Important #1" → commit hash; hwcheck comment uses the machine wrapper.
- Spec status lines still "awaiting review" / "ready for planning" for implemented work; add `docs/superpowers/README.md` (spec → plan → commits → status).
- `CREDITS.md`: add a "Libraries" section (TFT_eSPI, ArduinoJson, TJpg_Decoder, PNGdec, spotify-api-arduino, XPT2046) with licences.
- `docs/BACKLOG.md`: stray blank line splits the Ideas list.

## E. Reproducibility and repository hygiene

- **Test runner depends on a git-ignored, machine-specific `.devtools/ntest.ps1`** (hard-coded MinGW path) → commit a portable `tools/ntest.ps1` (g++ from PATH, optional `$env:MINGW_BIN`).
- **`spotify_auth.py` lives only in `.devtools/`**, yet README and `config.example.h:18` tell users to run it → move it to `tools/`.
- `COM11` and `.devtools\pio.ps1` in README / `capture_serial.py` / `platformio.ini` → generic `pio` commands and port placeholders (auto-detect in `capture_serial.py`).
- Add `requirements.txt` (pyserial), `.editorconfig`; optional `.clang-format`, `.vscode/extensions.json`.
- README "Setup" for a newcomer: prerequisites (PlatformIO, Python, Spotify app + redirect URI), `config.example.h` → `config.h`, build/flash/monitor/test commands.
- **`tools/patch_spotify_lib.py` only warns if the header/define is missing**: a library change could silently bring back the secret-printing debug mode → fail the build instead (changes the build, not the firmware).
- Optional CI (GitHub Actions): `pio run -e esp32dev` with a stub `config.h` + host tests with g++; optional check that generated `.inc` files match their generators.
- Secrets: none tracked in any revision (checked); `src/config.h` ignored and never committed.

## F. Safe small improvements

- `static TFT_eSPI tft` (`main.cpp:48`); `lock()/unlock()` internal to `Guard` (`shared.h:11-12`).
- Literals → named constants: `items[163]`/`160` (`screen_now.cpp:188`), `96` (`textdraw.cpp:73`).
- `-Wall -Wextra` in `build_src_flags` (our code only), fix what it reports without changing behaviour.
- `pio check` (cppcheck) as an optional local/CI step.
- Add `test_pick` (pure); split or document `lyricstatus` tests living in `test_netplan`.

## G. Defects found (behaviour fixes — separate from the cleanup)

1. **Medium — Spotify ad / unknown item type.** For `currently_playing_type` other than track/episode the
   library never fills `trackName`, `trackUri`, `albumName`, `numArtists`, `numImages`
   (`SpotifyArduino.cpp:538,610-632`, uninitialised `CurrentlyPlaying`), but `client.cpp:22-34`
   reads them → garbage or a crash during free-tier ads. Fix: check `cp.currentlyPlayingType` first.
2. Low — a 401 from a context lookup doesn't clear the cached access token (`client.cpp:65-126`): names fail until the 50-min expiry.
3. Low — the self-heal restart reason re-reads `net::isOnline()` and can mislabel the cause (`nettask.cpp:170`).

## H. Researched but out of scope (would change behaviour, performance or security posture)

- TLS certificate validation (`setCACertBundle` + embedded Mozilla bundle, ~64 KB flash) instead of `setInsecure()`.
- `ESP_LOGx`/`log_x` with tags and compiled-out verbose logs instead of `Serial.printf`.
- Task notifications for network → UI signals; mutex take with a timeout instead of `portMAX_DELAY`.
- ArduinoJson filter for the Spotify response (library-internal parse).
These are candidates for `docs/BACKLOG.md`, each needing a board session.

---

## Phased plan

| Phase | Content | Risk | Verification |
|---|---|---|---|
| 0 | Baseline numbers (build, tests, 5-min session) | — | recorded |
| 1 | Docs and comments only (D, README setup, CREDITS libraries, spec statuses, index) | none | read-through; build unchanged |
| 2 | Reproducibility (E): portable `tools/ntest.ps1`, `tools/spotify_auth.py`, port placeholders, `requirements.txt`, `.editorconfig`, patch script fails loudly | low | fresh-clone dry run of `tools/run_tests.ps1`; build |
| 3 | Remove dead code (A) | low | tests, build (RAM/flash ≤ baseline) |
| 4 | Deduplicate (B) with host tests for new pure helpers (`txt::copy`, case-insensitive compare, `theme::be`) | low | TDD for helpers; tests; build ≤ baseline; board session |
| 5 | Consistency (C, F): prefixes, constexpr, named constants, includes, warnings | low | tests; build; board session |
| 6 | Optional CI workflow and `pio check` | none (repo only) | workflow green |
| 7 | Defect fixes (G), each test-first where pure, as `fix:` commits | behaviour fix | targeted test; board session (play an ad if possible) |

## Decisions (2026-10-08)

- **D1** Touch-control leftovers: **remove now**, keep the idea in the backlog (code in git history).
- **D2** **Add CI** (GitHub Actions: firmware build + host tests).
- **D3** **Fix the defects in G** (G1–G3) as part of this effort, as `fix:` commits.
- **D4** Prefix renaming (`s_` for file-static state): **yes, on a separate branch** after D1–D3 land.

## Sources (best-practice research)

- PlatformIO: dependencies/pinning https://docs.platformio.org/en/latest/librarymanager/dependencies.html ·
  `build_src_flags` https://docs.platformio.org/en/latest/projectconf/sections/env/options/build/build_src_flags.html ·
  static analysis https://docs.platformio.org/en/latest/advanced/static-code-analysis/index.html ·
  GitHub Actions https://docs.platformio.org/en/latest/integration/ci/github-actions.html ·
  unit testing https://docs.platformio.org/en/latest/advanced/unit-testing/index.html
- ESP-IDF v4.4: heap debugging https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/heap_debug.html ·
  RAM usage https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/performance/ram-usage.html ·
  SMP FreeRTOS https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/freertos-smp.html ·
  logging https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/log.html ·
  watchdogs https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/wdts.html
- Arduino-ESP32 2.0.17 WiFiClientSecure https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFiClientSecure/README.md
- ArduinoJson 7 memory https://arduinojson.org/v7/how-to/reduce-memory-usage/
- C++ Core Guidelines https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines · Google C++ style (headers) https://google.github.io/styleguide/cppguide.html#Header_Files
- FreeRTOS task notifications https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/01-Task-notifications
- Repo hygiene: https://github.com/github/gitignore · https://www.conventionalcommits.org/en/v1.0.0/ · https://keepachangelog.com/en/1.1.0/ · https://editorconfig.org/ · https://pre-commit.com/
