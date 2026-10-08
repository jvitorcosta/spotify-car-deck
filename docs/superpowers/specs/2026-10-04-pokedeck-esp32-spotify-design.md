# PokeDeck — ESP32 Pokémon-Styled Spotify Deck

**Date:** 2026-10-04
**Status:** Implemented, partly superseded — the walker (§9) was replaced by
`2026-10-06-merged-status-panel-pmd-walker-design.md`; touch controls were not built.

## 1. Summary

A Spotify "now playing" controller and display for the car, built on an
ESP32 "Cheap Yellow Display" (CYD). It shows the currently-playing track with a
Gen 3 (GBA, Ruby/Sapphire era) Pokémon aesthetic, lets the user control playback
(play/pause, skip, volume) via the touchscreen, and assigns a **random Pokémon to
every play** using PokéAPI sprites. It does not play audio itself — it remote-
controls whatever device is already playing on the user's Spotify account (phone,
car head unit via CarPlay, speaker, etc.) through the Spotify Web API.

## 2. Goals

- Display rich now-playing info in a cohesive GBA Pokémon style
- Control playback: play/pause, previous, next, volume, (seek optional later)
- Show a random Pokémon per play, with name and type; the type tints the UI accent
- Show **synced lyrics** (karaoke-style) on demand via a LYRICS button, with a plain-text fallback
- Work reliably in the car, drawing internet from the iPhone's Personal Hotspot
- Coexist with Apple CarPlay with no conflict
- Keep secrets out of version control

## 3. Non-Goals (YAGNI)

- On-device audio playback / Spotify Connect speaker (hardware can't do it)
- Multitouch / pinch gestures (resistive panel is single-touch)
- Playlist browsing / search / library management (v1 is a *deck*, not a full client)
- Multi-user / account switching

## 4. Hardware

**Board:** ESP32-2432S028R ("Cheap Yellow Display" / CYD)

- ESP32-WROOM-32, dual-core, 2.4 GHz WiFi + Bluetooth
- **No PSRAM** — ~320 KB usable RAM (the dominant constraint)
- 2.8" ILI9341 TFT, **240×320**, used in **landscape (320×240)**
- **XPT2046 resistive touch**, single-touch, on a **separate SPI bus** (CYD gotcha)
- microSD slot (used for sprite/art caching)
- Backlight on GPIO 21; onboard RGB LED; this unit has a case + battery

**Known CYD pin facts to encode in config:**
- Display SPI (ILI9341) and touch SPI (XPT2046) are on **different buses** — touch
  needs its own `SPIClass`. Typical CYD touch pins: CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36.
- These are verified during the first bring-up milestone, not assumed blindly.

## 5. Spotify Account & API

- **Spotify Premium required** (user has it) — playback control endpoints return 403
  on free accounts.
- Uses the **Spotify Web API** (`/me/player`, `/me/player/currently-playing`,
  control endpoints `play`, `pause`, `next`, `previous`, `volume`).
- Auth: OAuth Authorization Code flow → long-lived **refresh token** reused by the
  device. Access tokens refreshed silently.

## 5.1 Lyrics (LRCLIB)

Spotify's own lyrics (Musixmatch) are **not** exposed by the Web API, so lyrics come
from **LRCLIB** (`lrclib.net`) — a free, open, no-API-key service.

- **Query** by track name + artist + album + duration (all already available from the
  now-playing response) → `GET /api/get`.
- **Synced first, plain fallback:** prefer `syncedLyrics` (LRC format with timestamps);
  if absent, use `plainLyrics`; if neither, show an in-theme **"No lyrics found"** state.
- **Synced playback:** LRC timestamps + the progress we already track drive line
  highlighting/auto-scroll — karaoke-style, no extra polling.
- **Trigger:** a **LYRICS button** on the deck toggles to the lyrics screen and back.
- **Size:** a full song's lyrics is a few KB — fits in RAM; parsed with ArduinoJson.
- **Font requirement:** the pixel font must include an **extended Latin charset**
  (accented characters) so Portuguese/other lyrics render correctly.
- **Caching:** optional — cache the current track's lyrics in RAM; re-fetch on track change.

## 6. Connectivity & Car Context

**Internet source:** iPhone **Personal Hotspot**.

**CarPlay decision:** user has wireless CarPlay but will use **wired CarPlay in the
car when the deck is in use**. Rationale: wired CarPlay frees the iPhone's WiFi radio
so Personal Hotspot runs reliably for the deck; wireless CarPlay + hotspot contend
for the same radio and are unreliable on iPhone.

**Interference:** none meaningful. CarPlay is USB (wired) or 5 GHz WiFi + BT
(wireless); the CYD is 2.4 GHz WiFi only. The ESP32 never talks to the head unit.

**Non-conflict (a feature):** Spotify Connect has one active session per account.
If Spotify plays through CarPlay, the deck's Web API commands target that same
session — the deck is simply a second remote for whatever is playing.

**Requirements this adds:**
- **Multiple stored WiFi networks** (home for setup/dev + phone hotspot for car),
  auto-roam to whichever is available.
- **Graceful offline mode** — tunnels / dead zones show last-known track or a
  "no signal" Pokémon screen; never hang or crash.
- **Power:** fed from car USB (5V). Use an adequate supply so WiFi current spikes
  don't brown out the board.
- **Environmental note:** avoid prolonged direct-sun dashboard heat (LCD + battery).

## 7. Software Stack

- **Toolchain:** PlatformIO extension in VS Code, ESP32 Arduino framework.
  Reproducible `platformio.ini` with pinned library versions.
- **UI rendering:** **custom drawing with TFT_eSPI + sprites** (not LVGL). Chosen for
  pixel-perfect GBA borders/fonts, low RAM, and native pairing with the JPEG decoder.
  Trade-off accepted: buttons/transitions are hand-built.

**Libraries:**
- `TFT_eSPI` — ILI9341 display + sprite drawing
- `XPT2046_Touchscreen` — resistive touch on its own SPI bus
- `spotify-api-arduino` (Brian Lough) — token refresh, now-playing, playback control
- `TJpg_Decoder` — JPEG album art (streams in MCU blocks → low RAM)
- `PNGdec` — PNG Pokémon sprites
- `ArduinoJson` — parsing

## 8. Architecture

Small, single-purpose modules with clean interfaces:

```
src/
  main.cpp        — setup/loop, orchestration, frame timing
  config.h        — secrets: WiFi networks, Spotify client id/secret (GIT-IGNORED)
  config.example.h— committed template
  net/wifi.*      — multi-network connect, auto-roam, reconnect, offline detection
  spotify/
    auth.*        — one-time OAuth (device-hosted page) + refresh token in NVS
    client.*      — now-playing poll + control commands; local progress interpolation
  images/
    jpeg.*        — fetch + decode album art to screen
    png.*         — fetch + decode Pokémon sprite to screen
    cache.*       — microSD cache for sprites (and optionally album art)
  pokemon/
    pokeapi.*     — pick random #1-1025, fetch sprite + name + type
  lyrics/
    lrclib.*      — fetch + parse LRCLIB lyrics (synced LRC or plain)
  ui/
    theme.*       — GBA palette, type→color map, extended-charset pixel font
    screen_now.*  — the now-playing deck layout
    screen_lyrics.*— lyrics view (synced highlight/scroll + plain fallback)
    widgets.*     — panels, HP bar, buttons
  input/touch.*   — touch read, calibration, button hit-testing
  app_state.*     — current track, pokemon, playback state (single source of truth)
```

## 9. UI Design

**Orientation:** landscape, 320×240.
**Style:** Gen 3 GBA / Ruby-Sapphire — cream panels (`#f7efd6`), navy borders
(`#20304f`) with a gold inner line, HP-bar progress, pokéball accents, pixel font.
**Accent color:** driven by the current Pokémon's **type**.

**Layout — "Pokédex Entry":**
- **Top bar (dark navy):** pokéball + "NOW PLAYING"; right side shows shuffle & repeat
  state icons and a device chip (e.g. "Living Room"). Top icons are tap targets that
  open a sub-panel rather than tiny hit zones.
- **Left column:** album cover (real art) on top; Pokémon box below it with the
  sprite, name, and a type badge.
- **Right column:** track title; artist; album · release year · explicit flag;
  the **playlist/context** it's playing from; a reskinned **"CP"** stat (= Spotify
  popularity, 0–100); then the control row.
- **Progress "route" bar:** a wide HP-style bar that doubles as a path the current
  song's **Pokémon walks along** (see mechanic below), with elapsed/total time.
- **Control row (finger-sized):** previous, play/pause, next, volume, and a **LYRICS**
button that toggles to the lyrics screen.

**Lyrics screen:** keeps the top bar (with a back/close target); main area shows the
lyrics. Synced mode auto-scrolls and highlights the current line in time with playback;
plain mode is a scrollable block; missing lyrics show an in-theme "No lyrics found"
message. Pixel font uses an extended Latin charset so accented lyrics render correctly.

**Pokémon mechanic:** a **fresh random Pokémon from #1–1025 each time a play starts**.
Sprites are the classic ~96×96 PokéAPI front sprites. Name + type shown; type sets
the accent color. Sprites cached to SD after first fetch.

**Walking-progress animation** (inspired by the JetBrains "Pokémon Progress" plugin):
the song's Pokémon **walks along the progress bar**, positioned at
`barStart + fraction × barWidth`, so it advances with the song. The walk is a cheap
**bob/step fake-walk** — the static sprite (downscaled once per song to ~40 px into a
small RAM buffer) bobs 1–2 px vertically and mirrors horizontally every few frames to
suggest walking. No extra assets, no per-frame network or decode. Animation runs at
~8 fps by repainting only the bar region. The IDE plugin uses bundled multi-frame
overworld sprites; we approximate that look with the single sprite we already have.

**Offline/edge states:** "connecting", "no signal / tunnel", and "nothing playing"
screens, each in-theme.

## 10. Key Technical Decisions & Constraints

- **Smooth progress without rate-limit risk:** poll now-playing every ~3–5 s;
  interpolate the HP bar locally each frame between polls.
- **RAM (no PSRAM):** both image decoders stream directly to the display; no full
  bitmap held in RAM. Album art fetched at Spotify's 300 px size and scaled on the fly.
- **Caching:** Pokémon sprites cached to microSD (keyed by dex number); optional album
  art cache.
- **HTTPS/TLS:** Spotify API + GitHub sprite CDN are TLS. Use a cert bundle;
  `setInsecure()` acceptable only for initial bring-up.
- **Secrets:** `config.h` git-ignored; `config.example.h` committed. Refresh token in
  NVS, never in source.
- **Touch:** calibration step; finger-sized primary controls; small status icons open
  sub-panels.

## 11. Security & Privacy

- Client secret and refresh token live only in NVS / git-ignored config, never
  committed.
- Scopes limited to what's needed: `user-read-playback-state`,
  `user-modify-playback-state`, `user-read-currently-playing`.
- No telemetry; the device talks only to Spotify, the sprite CDN, and LRCLIB.

## 12. Testing Strategy

- **Module bring-up per milestone** (below) — each is independently verifiable.
- **Serial-logged dry runs** for network/auth/API before any UI work.
- **Dummy-data UI pass** so the layout is validated without live API.
- **Offline simulation** (drop WiFi) to confirm graceful degradation.
- Touch calibration verified by on-screen crosshair test.

## 13. Build Order (each a working milestone)

1. **Hello screen** — PlatformIO builds; display lights up; touch reports coordinates.
2. **WiFi + Spotify auth** — multi-network connect; one-time on-device login; refresh
   token stored in NVS; now-playing fetched to serial.
3. **Static deck UI** — full GBA layout drawn with dummy data.
4. **Live data** — real track/artist/album/playlist/progress wired in (with interpolation).
5. **Album art** — fetch + JPEG decode + display.
6. **Pokémon** — random pick + PNG sprite + name + type-tinted accent + SD cache.
7. **Controls** — play/pause/skip/volume via touch.
8. **Lyrics** — LYRICS button → lyrics screen; LRCLIB fetch; synced highlight/scroll with
   plain + "not found" fallbacks; extended-charset font.
9. **Polish** — offline states, reconnect/roam handling, transitions, calibration.

## 14. Open Items / Future (post-v1)

- Seek-by-tapping the progress bar
- Like/❤️ current track (`user-library-modify`)
- Device picker sub-panel (transfer playback between Spotify devices)
- Optional album-art dithering for a more retro look
- Shiny-Pokémon chance for fun
