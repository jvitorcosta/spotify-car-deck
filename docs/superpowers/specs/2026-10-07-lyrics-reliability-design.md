# Lyrics reliability and dialogue-box messages: design

Date: 2026-10-07. Status: approved in brainstorming, awaiting written-spec review.

## Problem

Lyrics sometimes don't appear even when the Spotify app shows them, and the dialogue box is
then empty for the whole song. Evidence from session logs and LRCLIB requests:

1. **Plain-only entries.** LRCLIB can have plain text but no timestamps ("MIDNIGHT DOWN TOWN",
   S. Kiyotaka & Omega Tribe: `syncedLyrics` = "", `plainLyrics` = 384 chars; no synced entry in
   any search result). The deck reads only `syncedLyrics`, so it shows nothing.
2. **Temporary server errors.** LRCLIB returned 503 to the deck once, and to the PC on several
   attempts; one search request timed out. The deck tries once per song and never retries.
3. **Exact-match misses.** `/api/get` needs title, artist, album and duration (±2 s) to match.
   (Spotify-style title suffixes like "- Remastered 2009" and "(with …)" were checked and do match.)
4. **Empty box during intros.** Even with lyrics, the box is blank until the first timed line.

Spotify's own lyrics come from Musixmatch, which is not usable here (see Non-goals).

## Goals

- Recover the missing cases above: retry temporary errors, fall back to plain lyrics, fall back
  to LRCLIB search on an exact-match miss, recognise instrumentals.
- Never leave the dialogue box empty: playful Pokemon-style messages while searching, retrying,
  in the intro and when there are no lyrics.
- No new large buffers; byte RAM stays as today.

## Non-goals

- Musixmatch (official free plan: ~30% untimed preview; unofficial endpoint: against their terms).
  Parked in `docs/BACKLOG.md`.
- Bigger status panel when there are no lyrics. Parked in `docs/BACKLOG.md`; the layout is unchanged.
- Korean / emoji / Cyrillic glyphs (still blank, as before).

## Design

### 1. Fetching (network task)

**Stream reader** (`util/lrcstream`, extended; still one pass, no JSON document). It recognises,
per response object:

- `instrumental` (`true`/`false`).
- `plainLyrics`: stored into the lyrics arena as untimed lines (blank lines dropped). When the
  arena fills, remaining non-blank lines are still counted (`plainTotal`) but not stored.
- `syncedLyrics`: if non-empty, its lines **replace** any stored plain lines. Synced always wins.
  Field order does not matter for `/api/get` (LRCLIB sends plain before synced today).
- `null` or `""` values count as absent.

**Search mode** (`/api/search?track_name=&artist_name=`, a JSON array of up to 20 objects,
~120 KB for popular songs). Each object lists `duration` before its lyrics fields (checked on the
live API). The reader tracks object boundaries and:

- reads `duration` (a float) and accepts the object only if |duration − track| ≤ 3 s;
- skips lyrics of non-matching objects without storing them;
- stops reading at the first matching object with synced lines;
- otherwise keeps the first matching object's plain lyrics;
- gives up after **48 KB** read without a match (time and data cap on the hotspot).

An object with no `duration` before its lyrics is treated as non-matching.

**Plain spreading** (pure, `lyricbuf`). Untimed lines get times spread evenly from 10% to 90%
of the track duration, using `plainTotal` (stored + counted) as the line count, so stored lines
keep their true share of the song when the tail did not fit.

**Attempt result:**

| Result | When |
|---|---|
| `Synced` | ≥ 1 timed line |
| `Plain` | no timed lines, ≥ 1 plain line |
| `Instrumental` | `instrumental: true` (wins even if the entry also has text) |
| `NotFound` | 404 from `/api/get` and search found nothing, or an entry with no text |
| `TempError` | HTTP 5xx or 429, connect/TLS failure, timeout, stalled read |

Order per attempt: `/api/get`; on 404 → `/api/search`. A `TempError` in either counts as the
attempt's result.

**Retry rule** (pure, in `util/netplan`): `TempError` retries after **5 s**, then **20 s**; the
third `TempError` becomes `NotFound`. `NotFound`/`Synced`/`Plain`/`Instrumental` are final. A
track change cancels pending retries. While waiting, the Lyrics step is not runnable, so polls,
art, walker and prefetch proceed. A step deferred by the memory gate (`canRun`) or paused by
`Health` is not an attempt.

**Status published** to the UI with the track generation (same mechanism as art/lyric lines in
`core/shared`): `Searching`, `Retrying`, `Synced`, `Plain`, `Instrumental`, `None`. A new track
starts at `Searching`.

**Logging**, one line per attempt, e.g.
`[lyrics] "MIDNIGHT DOWN TOWN" / S. Kiyotaka & Omega Tribe: plain 24 lines via get (try 1)`
(results: `synced N`, `plain N`, `instrumental`, `not found`, `temp error rc=503`; source `get`
or `search`).

### 2. Dialogue-box messages (UI)

A pure module (`ui/lyricmsg`) maps
*(status, ms since the UI saw the status, track position, first-line time, Pokemon name,
track generation)* → *(text, show ♪ icons)*.

| Status | Box shows | ♪ icons |
|---|---|---|
| Searching | one SEARCH line | yes |
| Retrying | one RETRY line | yes |
| Synced | FOUND line until the first timed line, then the current lyric line | yes |
| Plain | same as Synced, using spread times | **no** (approximate timing) |
| Instrumental | one INSTRUMENTAL line for 3 s, then the IDLE line | yes |
| None | one FAIL line for 3 s, then the IDLE line | yes |

- `{P}` is replaced by the Pokemon name in capitals (accented and CJK names already render).
- The line from each pool is chosen pseudo-randomly, seeded by the track generation: stable for
  the whole song, different between songs, never flickers.
- Text uses the existing two-line wrap; the box still redraws only when the text changes.
- Offline / nothing-playing screens are unchanged.

**Message pools** (real move and item names):

SEARCH
- `{P} used SING! Searching for the verses...`
- `{P} is fetching this piece of art's verses...`
- `{P} used FORESIGHT! Reading the song...`
- `{P} used ECHOED VOICE! Listening for words...`
- `{P} used MIMIC! Learning the lyrics...`
- `{P} used ODOR SLEUTH! Sniffing out verses...`
- `A wild LYRIC appeared? {P} is chasing it...`
- `Accessed BILL's PC... Opening the Lyrics Box.`
- `{P} checked the POKéDEX for these verses...`
- `{P} is listening closely...`

RETRY
- `The verses fled! {P} is trying again...`
- `Spotify is fast asleep... {P} used WAKE-UP SLAP!` (messages say Spotify, not the LRCLIB backend)
- `It's not very effective... trying again!`

FAIL
- `But it failed!`
- `OAK: There's a time and place for everything! But not now.`
- `{P}'s search missed!`
- `The verses got away!`

INSTRUMENTAL
- `No words here! {P} is humming along...`
- `{P} used TEETER DANCE! It's instrumental!`
- `{P} used GRASSWHISTLE! Nothing to sing...`

FOUND (intro, before the first line)
- `It's super effective! Lyrics found!`
- `Gotcha! The verses were caught!`

IDLE
- `{P} is enjoying the music`

(The ♪ note icons drawn at both ends of the box stand in for the "♪" characters in the
brainstorm drafts; font 2 has no ♪ glyph. "POKéDEX" uses the existing accent-mark rendering.)

## Memory

No new large buffers. Search and plain lyrics use the existing ~5.6 KB lyrics arena (192 lines,
4 KB text) and the reader's 256-byte line buffer on the network-task stack. New state is a few
hundred bytes (status, retry clock, search object tracking). The search response is never held
in memory.

## Testing

Host tests (test first):

- `lrcstream`: instrumental flag; plain only; synced replaces plain in either order; empty synced
  + plain; null fields; plain overflow counting (`plainTotal`); search: entry chosen by duration
  ±3 s, non-matching skipped, stop at first synced match, plain-only match fallback, object
  without duration rejected, 48 KB cap.
- `lyricbuf` spreading: 10–90% window, blank lines dropped, `plainTotal` respected.
- `netplan` retry rule: 5 s / 20 s / give up; 404 no retry; track change resets; deferral is not
  an attempt.
- `lyricmsg`: every status; 3 s switch to IDLE; FOUND during the intro then lyric lines; stable
  per-song pick; `{P}` substitution; ♪ flag false for Plain.

On device: a 10-minute session with the per-attempt log lines; check the messages on screen,
including "MIDNIGHT DOWN TOWN" showing plain lyrics without ♪ icons.
