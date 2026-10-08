# Genre Badge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Gen-3 style genre badge before the artist name in the info box (`[R&B] D'Angelo`), from Apple's artist genre.

**Architecture:** A new network step `Genre` (after Lyrics) asks Apple's iTunes Search API for the first artist's `primaryGenreId`, unless a 32-artist cache already knows it. A generated table maps Apple's 478 genre ids to 29 badges (label + colour). The badge index reaches the UI through `core/shared` (generation-tagged, like album art); the UI repaints only the artist row.

**Tech Stack:** ESP32 Arduino (PlatformIO `espressif32@6.9.0`), TFT_eSPI, FreeRTOS; host tests with Unity via `.devtools\ntest.ps1`; Python generator (`tools/`).

**Spec:** `docs/superpowers/specs/2026-10-08-genre-badge-design.md`

## Global Constraints

- Source: `https://itunes.apple.com/search?term=<artist>&entity=musicArtist&limit=1&country=BR`; first result taken; first artist only.
- Badge 58×12 px, same style as the type badge (rounded, colour fill, `theme::darken` border, white font-1 capitals with a border-colour shadow). Labels ASCII, ≤ 9 chars. Artist name max width 108 px with a badge, 172 px without.
- 29 badges in this order and colours: POP #F070A8, K-POP #D040C0, J-POP #F08890, RAP #C89818, R&B #A04880, FUNK #E07020, ROCK #B83030, METAL #505058, ALT #6858A8, ELECTRO #18A8D8, DANCE #20B898, BRASIL #1E9E4E, FUNK BR #E0B000, SERTANEJO #A87030, MPB #2E8C80, SAMBA #E89818, PAGODE #58A838, FORRO #C85828, AXE #F07800, LATIN #E05040, REGGAE #3C8C28, JAZZ #3858A0, GOSPEL #C0A048, CLASSIC #8C6A48, OST #687898, COUNTRY #A08048, FOLK #788C48, WORLD #4890B0, ??? #8890A0 (unknown id; always last).
- Step order Art → Lyrics → Genre → Walk → Prefetch; `canRun(Genre)` needs `TLS_NEED`; optional (paused by `Health`).
- Cache: last 32 artists by FNV-1a hash of the name; "no genre" is cached (`genre::NONE`); network errors are not; no retries within a song. Empty artist name → no lookup, no badge.
- Response read into a fixed 1 KB static buffer; no JSON library, no heap.
- No new large RAM buffers; the id table lives in flash.
- Commits: Conventional Commits, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Secrets only in git-ignored `src/config.h`.
- Bash heredocs collapse backslashes on this machine: write files containing `\` escapes with the Write/Edit tools.

## Review Focus

1. Artist names with accents, `&`, apostrophes or spaces (Marília Mendonça, Racionais MC's, S. Kiyotaka & Omega Tribe) must be URL-encoded so Apple still finds them. (Task 2 `test_url_encode_reserved_and_utf8`)
2. A body cut off by the 1 KB buffer in the middle of the number must not yield a wrong genre id. (Task 2 `test_parse_rejects_cut_off_number`)
3. An empty artist name (podcast, local file) must not trigger a request and must show no badge. (Task 2 `test_cache_empty_artist_is_known_none`)
4. A subgenre id must take its parent's badge, but an override must win (Baile Funk 1229 → FUNK BR, not BRASIL; K-pop 51 → K-POP, not POP). (Task 3 `test_overrides_beat_parent`)
5. Adding a step must not starve the walker or prefetch when the genre is cached. (Task 1 `test_genre_done_skips_to_walk`)

---

## How to run things

- One host suite: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 <test.cpp> <module.cpp ...>`
- All host suites: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
- Firmware build: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
- Flash: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`
- Python: `& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"` (set `$env:PYTHONIOENCODING="utf-8"` for accented output)

---

### Task 1: `Genre` network step in the work plan

**Files:**
- Modify: `src/util/netplan.h`, `src/util/netplan.cpp`
- Test: `test/test_netplan/test_netplan.cpp`

**Interfaces:**
- Produces: `netplan::Step::Genre`; `struct Work { bool walk, art, lyrics, prefetch, genre; }`; `freshWork(walkerReady)` sets `genre = true`; `next()` order Art, Lyrics, Genre, Walk, Prefetch; `canRun(Step::Genre, largest)` = `largest >= TLS_NEED`.

- [ ] **Step 1: Update and add tests.** In `test/test_netplan/test_netplan.cpp`, replace `test_order_without_prefetched_walker` with:

```cpp
void test_order_without_prefetched_walker() {
    // Art and lyrics first: they are what the user waits for; then the small genre lookup;
    // the walker is usually prefetched, and when it isn't it can follow.
    netplan::Work w = netplan::freshWork(false);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
    netplan::done(w, Step::Art);
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w));
    netplan::done(w, Step::Lyrics);
    TEST_ASSERT_EQUAL_INT((int)Step::Genre, (int)netplan::next(w));
    netplan::done(w, Step::Genre);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w));
    netplan::done(w, Step::Walk);
    TEST_ASSERT_EQUAL_INT((int)Step::Prefetch, (int)netplan::next(w));
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w));
}
```

in `test_next_skips_lyrics_while_waiting` change the first expectation and the clean-up to:

```cpp
    TEST_ASSERT_EQUAL_INT((int)Step::Genre, (int)netplan::next(w, false));
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w, true));
    netplan::done(w, Step::Genre);
    netplan::done(w, Step::Walk);
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w, false));
```

and add before `int main`:

```cpp
void test_genre_done_skips_to_walk() {
    // A cached genre marks the step done at the track change: the walker comes right after lyrics.
    netplan::Work w = netplan::freshWork(false);
    netplan::done(w, Step::Genre);
    netplan::done(w, Step::Art);
    netplan::done(w, Step::Lyrics);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w));
}
void test_can_run_genre_needs_tls() {
    TEST_ASSERT_TRUE(netplan::canRun(Step::Genre, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(netplan::canRun(Step::Genre, netplan::TLS_NEED - 1));
}
```

registered after the last `RUN_TEST`:

```cpp
    RUN_TEST(test_genre_done_skips_to_walk);
    RUN_TEST(test_can_run_genre_needs_tls);
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `COMPILE FAILED` — `'Genre' is not a member of 'netplan::Step'`.

- [ ] **Step 3: Implement.** In `src/util/netplan.h` replace

```cpp
enum class Step { None, Walk, Art, Lyrics, Prefetch };
struct Work { bool walk, art, lyrics, prefetch; };   // true = still to do
```

with

```cpp
enum class Step { None, Walk, Art, Lyrics, Prefetch, Genre };
struct Work { bool walk, art, lyrics, prefetch, genre; };   // true = still to do
```

and the `next` comment's first line with `// Next step in priority order: Art, Lyrics, Genre, Walk, Prefetch; None when all done.`

In `src/util/netplan.cpp`:

```cpp
Work freshWork(bool walkerReady) { return {!walkerReady, true, true, true, true}; }
```

in `next`, after the Lyrics line add `    if (w.genre) return Step::Genre;`; in `done` add `        case Step::Genre:    w.genre = false; break;`; in `canRun` change `case Step::Lyrics:` to

```cpp
        case Step::Lyrics:
        case Step::Genre:
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `26 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 5: Build check** (nettask's `switch (step)` gains an unhandled enum value only as a warning; `Work{}` aggregate init still compiles).

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`

- [ ] **Step 6: Commit**

```bash
git add src/util/netplan.h src/util/netplan.cpp test/test_netplan/test_netplan.cpp
git commit -m "feat(net): genre step after lyrics in the work plan" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Apple answer parser, artist cache, URL encoding

**Files:**
- Create: `src/util/genre.h`, `src/util/genre.cpp`
- Modify: `src/util/text.h`, `src/util/text.cpp`
- Test: `test/test_genre/test_genre.cpp`, `test/test_text/test_text.cpp`

**Interfaces:**
- Produces:
  - `constexpr uint8_t genre::NONE = 0xFF;`
  - `bool genre::parsePrimaryGenreId(const char* body, uint32_t* id);`
  - `uint32_t genre::hashName(const char* s);`
  - `class genre::Cache { static constexpr int N = 32; bool find(const char* artist, uint8_t* badge) const; void store(const char* artist, uint8_t badge); }`
  - `size_t txt::urlEncode(const char* s, char* out, size_t n);` (RFC 3986 unreserved kept, everything else `%XX`, UTF-8 bytewise; returns length; truncates at a whole `%XX`)

- [ ] **Step 1: Write the failing tests.** Create `test/test_genre/test_genre.cpp` (Write tool):

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/genre.h"

void setUp() {}
void tearDown() {}

// Real shape of an iTunes Search API artist answer (country=BR).
static const char* DANGELO =
    "{\n \"resultCount\":1,\n \"results\": [\n{\"wrapperType\":\"artist\", \"artistType\":\"Artist\", "
    "\"artistName\":\"D'Angelo\", \"artistLinkUrl\":\"https://music.apple.com/br/artist/dangelo/185580?uo=4\", "
    "\"artistId\":185580, \"amgArtistId\":127412, \"primaryGenreName\":\"R&B/soul\", \"primaryGenreId\":15}]\n}\n";

void test_parse_real_answer() {
    uint32_t id = 0;
    TEST_ASSERT_TRUE(genre::parsePrimaryGenreId(DANGELO, &id));
    TEST_ASSERT_EQUAL_UINT32(15, id);
}
void test_parse_large_ids_and_spacing() {
    uint32_t id = 0;
    TEST_ASSERT_TRUE(genre::parsePrimaryGenreId("{\"primaryGenreId\" : 50000063 }", &id));
    TEST_ASSERT_EQUAL_UINT32(50000063, id);
    TEST_ASSERT_TRUE(genre::parsePrimaryGenreId("{\"primaryGenreId\":1122,\"x\":1}", &id));
    TEST_ASSERT_EQUAL_UINT32(1122, id);
}
void test_parse_no_result_is_false() {
    uint32_t id = 7;
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\n \"resultCount\":0,\n \"results\": []\n}\n", &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("", &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId(nullptr, &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\"primaryGenreId\":\"x\"}", &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\"primaryGenreId\":null}", &id));
    TEST_ASSERT_EQUAL_UINT32(7, id);                 // untouched on failure
}
void test_parse_rejects_cut_off_number() {
    uint32_t id = 0;
    // The 1 KB buffer could end inside the number: "11" of "1122" is not a genre.
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\"primaryGenreName\":\"Brasileira\", \"primaryGenreId\":11", &id));
}
void test_hash_is_stable_and_spreads() {
    TEST_ASSERT_EQUAL_UINT32(genre::hashName("BTS"), genre::hashName("BTS"));
    TEST_ASSERT_NOT_EQUAL(genre::hashName("BTS"), genre::hashName("bts"));
    TEST_ASSERT_NOT_EQUAL(genre::hashName("Mar\xC3\xADlia Mendon\xC3\xA7" "a"), genre::hashName("Marilia Mendonca"));
}
void test_cache_miss_then_hit() {
    genre::Cache c;
    uint8_t b = 0;
    TEST_ASSERT_FALSE(c.find("D'Angelo", &b));
    c.store("D'Angelo", 4);
    TEST_ASSERT_TRUE(c.find("D'Angelo", &b));
    TEST_ASSERT_EQUAL_UINT8(4, b);
    c.store("D'Angelo", 5);                          // re-store updates, no duplicate
    TEST_ASSERT_TRUE(c.find("D'Angelo", &b));
    TEST_ASSERT_EQUAL_UINT8(5, b);
}
void test_cache_remembers_none() {
    genre::Cache c;
    uint8_t b = 0;
    c.store("Unknown Band", genre::NONE);
    TEST_ASSERT_TRUE(c.find("Unknown Band", &b));
    TEST_ASSERT_EQUAL_UINT8(genre::NONE, b);
}
void test_cache_empty_artist_is_known_none() {
    genre::Cache c;
    uint8_t b = 0;
    TEST_ASSERT_TRUE(c.find("", &b));
    TEST_ASSERT_EQUAL_UINT8(genre::NONE, b);
    TEST_ASSERT_TRUE(c.find(nullptr, &b));
    TEST_ASSERT_EQUAL_UINT8(genre::NONE, b);
}
void test_cache_evicts_oldest_when_full() {
    genre::Cache c;
    char name[16];
    for (int i = 0; i < genre::Cache::N; ++i) {
        snprintf(name, sizeof(name), "artist %d", i);
        c.store(name, (uint8_t)(i % 29));
    }
    uint8_t b = 0;
    TEST_ASSERT_TRUE(c.find("artist 0", &b));
    c.store("newcomer", 3);
    TEST_ASSERT_FALSE(c.find("artist 0", &b));       // oldest replaced
    TEST_ASSERT_TRUE(c.find("artist 1", &b));
    TEST_ASSERT_TRUE(c.find("newcomer", &b));
    TEST_ASSERT_EQUAL_UINT8(3, b);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_real_answer);
    RUN_TEST(test_parse_large_ids_and_spacing);
    RUN_TEST(test_parse_no_result_is_false);
    RUN_TEST(test_parse_rejects_cut_off_number);
    RUN_TEST(test_hash_is_stable_and_spreads);
    RUN_TEST(test_cache_miss_then_hit);
    RUN_TEST(test_cache_remembers_none);
    RUN_TEST(test_cache_empty_artist_is_known_none);
    RUN_TEST(test_cache_evicts_oldest_when_full);
    return UNITY_END();
}
```

In `test/test_text/test_text.cpp` add before `int main`:

```cpp
void test_url_encode_reserved_and_utf8() {
    char o[128];
    TEST_ASSERT_EQUAL_INT(35, (int)txt::urlEncode("S. Kiyotaka & Omega Tribe", o, sizeof(o)));
    TEST_ASSERT_EQUAL_STRING("S.%20Kiyotaka%20%26%20Omega%20Tribe", o);
    txt::urlEncode("Racionais MC's", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("Racionais%20MC%27s", o);
    txt::urlEncode("Mar\xC3\xADlia", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("Mar%C3%ADlia", o);
    txt::urlEncode("a-b_c.d~e", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("a-b_c.d~e", o);         // unreserved kept
}
void test_url_encode_truncates_whole_escapes() {
    char o[6];                                       // room for 5 chars
    txt::urlEncode("ab&cd", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("ab%26", o);
    char p[5];                                       // room for 4: "%26" no longer fits after "ab"
    txt::urlEncode("ab&cd", p, sizeof(p));
    TEST_ASSERT_EQUAL_STRING("ab", p);
    txt::urlEncode(nullptr, p, sizeof(p));
    TEST_ASSERT_EQUAL_STRING("", p);
}
```

registered in `main`:

```cpp
    RUN_TEST(test_url_encode_reserved_and_utf8);
    RUN_TEST(test_url_encode_truncates_whole_escapes);
```

- [ ] **Step 2: Run to verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_genre\test_genre.cpp` and `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_text\test_text.cpp src\util\text.cpp`
Expected: both `COMPILE FAILED` (`genre.h: No such file or directory`; `'urlEncode' is not a member of 'txt'`).

- [ ] **Step 3: Implement.** Create `src/util/genre.h` (Write tool):

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
// Artist genre from Apple's iTunes Search API (spec 2026-10-08-genre-badge-design.md): read
// primaryGenreId out of the small JSON answer, and remember the badge per artist.
// PURE, host-tested.
namespace genre {
constexpr uint8_t NONE = 0xFF;   // Apple has no genre for this artist: no badge
// First "primaryGenreId": <digits> in body. False when absent, not a number, or cut off
// (digits must be followed by a delimiter: the read buffer may end inside the number).
bool parsePrimaryGenreId(const char* body, uint32_t* id);
uint32_t hashName(const char* s);   // FNV-1a 32-bit over the UTF-8 bytes
// Badge index per artist, last N artists (oldest replaced). Callers store NONE for "Apple has
// no genre" and nothing for network errors, so those are looked up again next time.
class Cache {
public:
    static constexpr int N = 32;
    // True when the badge for `artist` is known (possibly NONE). An empty name is known: NONE.
    bool find(const char* artist, uint8_t* badge) const;
    void store(const char* artist, uint8_t badge);
private:
    uint32_t hash_[N] = {};
    uint8_t badge_[N] = {};
    int count_ = 0, next_ = 0;
};
}
```

Create `src/util/genre.cpp`:

```cpp
#include "genre.h"
#include <cstring>

namespace genre {

static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool parsePrimaryGenreId(const char* body, uint32_t* id) {
    if (!body) return false;
    const char* p = strstr(body, "\"primaryGenreId\"");
    if (!p) return false;
    p += 16;                                   // strlen("\"primaryGenreId\"")
    while (isSpace(*p)) ++p;
    if (*p != ':') return false;
    ++p;
    while (isSpace(*p)) ++p;
    uint32_t v = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 9) return false;        // no genre id is that long
        v = v * 10 + (uint32_t)(*p - '0');
        ++p;
    }
    if (digits == 0) return false;
    if (*p != ',' && *p != '}' && !isSpace(*p)) return false;   // cut off mid-number
    *id = v;
    return true;
}

uint32_t hashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; s && *s; ++s) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

bool Cache::find(const char* artist, uint8_t* badge) const {
    if (!artist || !artist[0]) { *badge = NONE; return true; }
    uint32_t h = hashName(artist);
    for (int i = 0; i < count_; ++i)
        if (hash_[i] == h) { *badge = badge_[i]; return true; }
    return false;
}

void Cache::store(const char* artist, uint8_t badge) {
    if (!artist || !artist[0]) return;
    uint32_t h = hashName(artist);
    for (int i = 0; i < count_; ++i)
        if (hash_[i] == h) { badge_[i] = badge; return; }
    int slot;
    if (count_ < N) slot = count_++;
    else { slot = next_; next_ = (next_ + 1) % N; }   // replace the oldest
    hash_[slot] = h;
    badge_[slot] = badge;
}

}
```

In `src/util/text.h`, before the closing `}` add:

```cpp
// Percent-encodes s for a URL query value: RFC 3986 unreserved characters (A-Z a-z 0-9 - _ . ~)
// are kept, every other byte (UTF-8 included) becomes %XX. Writes at most n bytes incl. NUL,
// never a partial %XX; returns the encoded length. nullptr -> "".
size_t urlEncode(const char* s, char* out, size_t n);
```

In `src/util/text.cpp`, after `copyId` add:

```cpp
size_t urlEncode(const char* s, char* out, size_t n) {
    if (n == 0) return 0;
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (; s && *s; ++s) {
        unsigned char c = (unsigned char)*s;
        bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~';
        size_t need = keep ? 1 : 3;
        if (o + need > n - 1) break;
        if (keep) {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0xF];
        }
    }
    out[o] = '\0';
    return o;
}
```

- [ ] **Step 4: Run to verify they pass**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_genre\test_genre.cpp src\util\genre.cpp`
Expected: `9 Tests 0 Failures 0 Ignored` / `OK`
Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_text\test_text.cpp src\util\text.cpp`
Expected: `13 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 5: Commit**

```bash
git add src/util/genre.h src/util/genre.cpp src/util/text.h src/util/text.cpp test/test_genre/test_genre.cpp test/test_text/test_text.cpp
git commit -m "feat(genre): Apple genre-id parser, 32-artist cache, URL encoding" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Badge table generated from Apple's genre tree

**Files:**
- Create: `tools/gen_genres.py`, `src/ui/genre_data.inc` (generated, committed), `src/ui/genrebadge.h`, `src/ui/genrebadge.cpp`
- Test: `test/test_genrebadge/test_genrebadge.cpp`

**Interfaces:**
- Consumes: `theme::rgb` (`src/ui/theme.h`).
- Produces:
  - `constexpr int genrebadge::W = 58, genrebadge::H = 12;`
  - `struct genrebadge::Badge { const char* label; uint16_t color; };`
  - `int genrebadge::count();` (29; the `???` fallback is the last index)
  - `const genrebadge::Badge& genrebadge::at(int index);`
  - `uint8_t genrebadge::forGenreId(uint32_t id);`
  - `int genrebadge::idCount();` `uint32_t genrebadge::idAt(int i);` (tests)

- [ ] **Step 1: Write the failing tests.** Create `test/test_genrebadge/test_genrebadge.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/ui/genrebadge.h"
#include "../../src/ui/theme.h"

void setUp() {}
void tearDown() {}

static const char* L(uint32_t id) { return genrebadge::at(genrebadge::forGenreId(id)).label; }

void test_top_level_genres() {
    TEST_ASSERT_EQUAL_STRING("POP", L(14));
    TEST_ASSERT_EQUAL_STRING("RAP", L(18));
    TEST_ASSERT_EQUAL_STRING("R&B", L(15));
    TEST_ASSERT_EQUAL_STRING("ROCK", L(21));
    TEST_ASSERT_EQUAL_STRING("ALT", L(20));
    TEST_ASSERT_EQUAL_STRING("ELECTRO", L(7));
    TEST_ASSERT_EQUAL_STRING("DANCE", L(17));
    TEST_ASSERT_EQUAL_STRING("BRASIL", L(1122));
    TEST_ASSERT_EQUAL_STRING("LATIN", L(12));
    TEST_ASSERT_EQUAL_STRING("LATIN", L(100024));
    TEST_ASSERT_EQUAL_STRING("REGGAE", L(24));
    TEST_ASSERT_EQUAL_STRING("JAZZ", L(11));
    TEST_ASSERT_EQUAL_STRING("JAZZ", L(2));
    TEST_ASSERT_EQUAL_STRING("GOSPEL", L(22));
    TEST_ASSERT_EQUAL_STRING("CLASSIC", L(5));
    TEST_ASSERT_EQUAL_STRING("OST", L(16));
    TEST_ASSERT_EQUAL_STRING("COUNTRY", L(6));
    TEST_ASSERT_EQUAL_STRING("FOLK", L(10));
    TEST_ASSERT_EQUAL_STRING("WORLD", L(19));
}
void test_subgenres_take_parent_badge() {
    TEST_ASSERT_EQUAL_STRING("ALT", L(1004));      // Indie Rock
    TEST_ASSERT_EQUAL_STRING("ROCK", L(1146));     // Arena rock
    TEST_ASSERT_EQUAL_STRING("RAP", L(1073));      // Hip-hop
    TEST_ASSERT_EQUAL_STRING("DANCE", L(1050));    // Techno
    TEST_ASSERT_EQUAL_STRING("WORLD", L(1263));    // Bollywood
    TEST_ASSERT_EQUAL_STRING("LATIN", L(100026));  // Cha-cha-cha
    TEST_ASSERT_EQUAL_STRING("BRASIL", L(1224));   // Frevo
}
void test_overrides_beat_parent() {
    TEST_ASSERT_EQUAL_STRING("K-POP", L(51));
    TEST_ASSERT_EQUAL_STRING("FUNK", L(1139));
    TEST_ASSERT_EQUAL_STRING("METAL", L(1153));
    TEST_ASSERT_EQUAL_STRING("METAL", L(1152));    // Hard rock
    TEST_ASSERT_EQUAL_STRING("FUNK BR", L(1229));  // Baile Funk, not BRASIL
    TEST_ASSERT_EQUAL_STRING("SERTANEJO", L(1228));
    TEST_ASSERT_EQUAL_STRING("MPB", L(1225));
    TEST_ASSERT_EQUAL_STRING("MPB", L(1221));      // Bossa nova
    TEST_ASSERT_EQUAL_STRING("SAMBA", L(1227));
    TEST_ASSERT_EQUAL_STRING("SAMBA", L(1222));    // Choro
    TEST_ASSERT_EQUAL_STRING("PAGODE", L(1226));
    TEST_ASSERT_EQUAL_STRING("FORRO", L(1223));
    TEST_ASSERT_EQUAL_STRING("AXE", L(1220));
    TEST_ASSERT_EQUAL_STRING("J-POP", L(27));
    TEST_ASSERT_EQUAL_STRING("J-POP", L(30));      // Kayokyoku
    TEST_ASSERT_EQUAL_STRING("OST", L(29));        // Anime
}
void test_unknown_is_question_marks() {
    TEST_ASSERT_EQUAL_STRING("???", L(999999));
    TEST_ASSERT_EQUAL_STRING("???", L(0));
    TEST_ASSERT_EQUAL_STRING("???", L(3));         // Comedia: not a music badge
    TEST_ASSERT_EQUAL_INT(genrebadge::count() - 1, genrebadge::forGenreId(999999));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x88, 0x90, 0xA0), genrebadge::at(genrebadge::count() - 1).color);
}
void test_badges_fit_and_colours_are_distinct() {
    TEST_ASSERT_EQUAL_INT(29, genrebadge::count());
    for (int i = 0; i < genrebadge::count(); ++i) {
        const genrebadge::Badge& b = genrebadge::at(i);
        TEST_ASSERT_TRUE_MESSAGE(strlen(b.label) <= 9, b.label);
        TEST_ASSERT_TRUE((int)strlen(b.label) * 6 - 1 <= genrebadge::W - 4);
        for (int j = 0; j < i; ++j) TEST_ASSERT_NOT_EQUAL(genrebadge::at(j).color, b.color);
    }
}
void test_table_is_sorted_and_complete() {
    TEST_ASSERT_TRUE(genrebadge::idCount() > 400);
    for (int i = 1; i < genrebadge::idCount(); ++i)
        TEST_ASSERT_TRUE(genrebadge::idAt(i - 1) < genrebadge::idAt(i));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_top_level_genres);
    RUN_TEST(test_subgenres_take_parent_badge);
    RUN_TEST(test_overrides_beat_parent);
    RUN_TEST(test_unknown_is_question_marks);
    RUN_TEST(test_badges_fit_and_colours_are_distinct);
    RUN_TEST(test_table_is_sorted_and_complete);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_genrebadge\test_genrebadge.cpp`
Expected: `COMPILE FAILED` — `genrebadge.h: No such file or directory`.

- [ ] **Step 3: Write the generator** `tools/gen_genres.py` (Write tool):

```python
"""Generate src/ui/genre_data.inc from Apple's music genre tree (dev-time; output is committed).

Every Apple genre id maps to one of the deck's genre badges: a top-level genre and all its
subgenres take the TOP badge, with OVERRIDES winning (Baile Funk -> FUNK BR, K-pop -> K-POP ...).
Ids not mapped fall back to the grey "???" badge at runtime. Spec:
docs/superpowers/specs/2026-10-08-genre-badge-design.md
Run:  python tools/gen_genres.py
"""
import json
import os
import urllib.request

TREE_URL = "https://itunes.apple.com/WebObjects/MZStoreServices.woa/ws/genres?id=34&cc=br"
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "genre_data.inc")

# Badge order is the runtime index; "???" must stay last (unknown-id fallback).
BADGES = [
    ("POP", 0xF070A8), ("K-POP", 0xD040C0), ("J-POP", 0xF08890), ("RAP", 0xC89818),
    ("R&B", 0xA04880), ("FUNK", 0xE07020), ("ROCK", 0xB83030), ("METAL", 0x505058),
    ("ALT", 0x6858A8), ("ELECTRO", 0x18A8D8), ("DANCE", 0x20B898), ("BRASIL", 0x1E9E4E),
    ("FUNK BR", 0xE0B000), ("SERTANEJO", 0xA87030), ("MPB", 0x2E8C80), ("SAMBA", 0xE89818),
    ("PAGODE", 0x58A838), ("FORRO", 0xC85828), ("AXE", 0xF07800), ("LATIN", 0xE05040),
    ("REGGAE", 0x3C8C28), ("JAZZ", 0x3858A0), ("GOSPEL", 0xC0A048), ("CLASSIC", 0x8C6A48),
    ("OST", 0x687898), ("COUNTRY", 0xA08048), ("FOLK", 0x788C48), ("WORLD", 0x4890B0),
    ("???", 0x8890A0),
]
TOP = {
    14: "POP", 50000066: "POP", 50000064: "POP", 18: "RAP", 15: "R&B", 21: "ROCK", 20: "ALT",
    7: "ELECTRO", 17: "DANCE", 1122: "BRASIL", 12: "LATIN", 100024: "LATIN", 24: "REGGAE",
    11: "JAZZ", 2: "JAZZ", 22: "GOSPEL", 5: "CLASSIC", 1290: "CLASSIC", 16: "OST",
    6: "COUNTRY", 1289: "FOLK", 10: "FOLK", 50000068: "FOLK",
    19: "WORLD", 1203: "WORLD", 1197: "WORLD", 1262: "WORLD", 1232: "WORLD", 1243: "WORLD",
    1300: "WORLD", 100084: "WORLD", 1299: "WORLD", 50000121: "WORLD",
}
OVERRIDES = {
    51: "K-POP", 1252: "K-POP", 27: "J-POP", 30: "J-POP", 28: "J-POP", 29: "OST", 50000063: "OST",
    1139: "FUNK", 1153: "METAL", 1149: "METAL", 1151: "METAL", 1152: "METAL",
    1229: "FUNK BR", 1228: "SERTANEJO", 1225: "MPB", 1221: "MPB", 1227: "SAMBA", 1222: "SAMBA",
    1226: "PAGODE", 1223: "FORRO", 1220: "AXE",
}

index = {label: i for i, (label, _) in enumerate(BADGES)}
assert BADGES[-1][0] == "???"
assert all(b in index for b in list(TOP.values()) + list(OVERRIDES.values()))

req = urllib.request.Request(TREE_URL, headers={"User-Agent": "PokeDeck/1.0 (gen_genres.py)"})
tree = json.loads(urllib.request.urlopen(req, timeout=30).read())["34"]["subgenres"]

ids = {}


def walk(gid, node, parent):
    badge = OVERRIDES.get(gid, TOP.get(gid, parent))
    if badge:
        ids[gid] = index[badge]
    for sub_id, sub in node.get("subgenres", {}).items():
        walk(int(sub_id), sub, badge)


for gid, node in tree.items():
    walk(int(gid), node, None)

with open(OUT, "w", encoding="ascii", newline="\n") as f:
    f.write("// Generated by tools/gen_genres.py from Apple's genre tree. Do not edit.\n")
    f.write("static const Badge BADGES[] = {\n")
    for label, c in BADGES:
        f.write('    {"%s", rgb(0x%02X, 0x%02X, 0x%02X)},\n' % (label, c >> 16, (c >> 8) & 255, c & 255))
    f.write("};\n")
    f.write("static const IdBadge IDS[] = {   // sorted by id\n")
    for gid in sorted(ids):
        f.write("    {%d, %d},\n" % (gid, ids[gid]))
    f.write("};\n")
print("wrote", OUT, "-", len(ids), "ids")
```

Run: `& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe" tools\gen_genres.py`
Expected: `wrote ...genre_data.inc - 478 ids` (Apple's tree can grow; > 400 is what the test requires).

- [ ] **Step 4: Implement the lookup.** Create `src/ui/genrebadge.h`:

```cpp
#pragma once
#include <cstdint>
// Genre badge for the info box: Apple genre id -> label + colour. The table is generated by
// tools/gen_genres.py from Apple's genre tree (subgenres take their parent's badge, with
// overrides such as Baile Funk -> FUNK BR). Spec 2026-10-08-genre-badge-design.md. PURE, tested.
namespace genrebadge {
constexpr int W = 58, H = 12;        // fits 9 letters of font 1 (6 px each) with padding
struct Badge { const char* label; uint16_t color; };
int count();                         // number of badges; the "???" fallback is the last one
const Badge& at(int index);          // 0 <= index < count()
uint8_t forGenreId(uint32_t id);     // badge index; unknown id -> the "???" fallback
int idCount();                       // generated table size (tests)
uint32_t idAt(int i);                // i-th id, ascending (tests)
}
```

Create `src/ui/genrebadge.cpp`:

```cpp
#include "genrebadge.h"
#include "theme.h"

namespace genrebadge {

using theme::rgb;
struct IdBadge { uint32_t id; uint8_t badge; };
#include "genre_data.inc"

static constexpr int BADGE_N = (int)(sizeof(BADGES) / sizeof(BADGES[0]));
static constexpr int ID_N = (int)(sizeof(IDS) / sizeof(IDS[0]));

int count() { return BADGE_N; }
const Badge& at(int index) { return BADGES[index]; }
int idCount() { return ID_N; }
uint32_t idAt(int i) { return IDS[i].id; }

uint8_t forGenreId(uint32_t id) {
    int lo = 0, hi = ID_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (IDS[mid].id < id) lo = mid + 1;
        else if (IDS[mid].id > id) hi = mid - 1;
        else return IDS[mid].badge;
    }
    return (uint8_t)(BADGE_N - 1);   // "???"
}

}
```

- [ ] **Step 5: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_genrebadge\test_genrebadge.cpp src\ui\genrebadge.cpp src\ui\theme.cpp`
Expected: `6 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 6: Commit**

```bash
git add tools/gen_genres.py src/ui/genre_data.inc src/ui/genrebadge.h src/ui/genrebadge.cpp test/test_genrebadge/test_genrebadge.cpp
git commit -m "feat(ui): genre badge table generated from Apple's genre tree" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Fetch the genre on the network task (device)

Device code: verified by building, then on the board in Task 6. Its decisions are the ones tested in Tasks 1–3.

**Files:**
- Create: `src/genre/apple.h`, `src/genre/apple.cpp`
- Modify: `src/core/shared.h`, `src/core/shared.cpp`, `src/core/nettask.cpp`

**Interfaces:**
- Consumes: `netplan::Step::Genre`, `genre::parsePrimaryGenreId`, `genre::Cache`, `genre::NONE`, `genrebadge::forGenreId`, `genrebadge::at`, `txt::urlEncode`.
- Produces (used by Task 5):
  - `enum class applegenre::Result { Found, None, Error };`
  - `applegenre::Result applegenre::lookup(const char* artist, uint32_t* genreId, int* rc);`
  - `void shared::postGenre(uint32_t gen, uint8_t badge);`
  - `bool shared::takeGenre(uint32_t gen, uint8_t* badge);` (true once per arrival)

- [ ] **Step 1: Create `src/genre/apple.h`:**

```cpp
#pragma once
#include <stdint.h>
// Artist genre from Apple's iTunes Search API (no key): one ~300-byte HTTPS request,
// https://itunes.apple.com/search?term=<artist>&entity=musicArtist&limit=1&country=BR.
// Spotify no longer returns artist genres to this app (spec 2026-10-08-genre-badge-design.md).
namespace applegenre {
enum class Result { Found, None, Error };   // None: Apple has no match/genre; Error: try later
// First artist only. rc: HTTP status (negative: connect/TLS/timeout) for the log.
Result lookup(const char* artist, uint32_t* genreId, int* rc);
}
```

- [ ] **Step 2: Create `src/genre/apple.cpp`:**

```cpp
#include "apple.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../util/genre.h"
#include "../util/text.h"

namespace applegenre {

static char g_body[1024];   // static: off the 10 KB network-task stack; answers are ~300 B

Result lookup(const char* artist, uint32_t* genreId, int* rc) {
    *rc = 0;
    if (!artist || !artist[0]) return Result::None;
    char term[300];
    txt::urlEncode(artist, term, sizeof(term));
    char url[400];
    snprintf(url, sizeof(url),
             "https://itunes.apple.com/search?term=%s&entity=musicArtist&limit=1&country=BR", term);
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient https;
    https.useHTTP10(true);
    https.setTimeout(8000);
    if (!https.begin(client, url)) { *rc = -1; return Result::Error; }
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    *rc = https.GET();
    if (*rc != 200) { https.end(); return Result::Error; }
    WiFiClient* s = https.getStreamPtr();
    size_t len = 0;
    uint32_t last = millis();
    while (len < sizeof(g_body) - 1 && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (millis() - last > 8000) break;   // stalled: parse what arrived
            delay(1);
            continue;
        }
        size_t want = sizeof(g_body) - 1 - len;
        if ((size_t)a < want) want = (size_t)a;
        len += s->readBytes(g_body + len, want);
        last = millis();
    }
    https.end();
    g_body[len] = '\0';
    return genre::parsePrimaryGenreId(g_body, genreId) ? Result::Found : Result::None;
}

}
```

- [ ] **Step 3: `src/core/shared.h`** — after the walker declarations (`bool takeWalker(uint32_t gen);`) add:

```cpp
// Genre badge (genrebadge index) for the track generation; true once per arrival (UI).
void postGenre(uint32_t gen, uint8_t badge);
bool takeGenre(uint32_t gen, uint8_t* badge);
```

`src/core/shared.cpp` — after `static lyricstatus::Status g_status = ...;` add

```cpp
static uint32_t g_genreGen = 0;    // gen g_genre was posted for (0 = none waiting)
static uint8_t g_genre = 0;
```

and after `takeWalker` add:

```cpp
void postGenre(uint32_t gen, uint8_t badge) {
    Guard g;
    if (gen != g_state.trackGen) return;   // stale: track moved on
    g_genreGen = gen;
    g_genre = badge;
}

bool takeGenre(uint32_t gen, uint8_t* badge) {
    Guard g;
    if (gen == 0 || g_genreGen != gen) return false;
    g_genreGen = 0;
    *badge = g_genre;
    return true;
}
```

- [ ] **Step 4: `src/core/nettask.cpp`** — add includes after `#include "../util/netplan.h"`:

```cpp
#include "../util/genre.h"
#include "../ui/genrebadge.h"
#include "../genre/apple.h"
```

after `static netplan::LyricsRetry s_lyricsRetry;` add

```cpp
static genre::Cache s_genres;   // last 32 artists' badges: repeats cost no request
```

in `onTrackChange()`, after `s_lyricsRetry.reset();` add

```cpp
    uint8_t badge;
    if (s_genres.find(s_st.artist, &badge)) {   // known artist (or none): no request
        netplan::done(s_work, netplan::Step::Genre);
        if (badge != genre::NONE) shared::postGenre(s_st.trackGen, badge);
    }
```

and add a case to `doStep` before `case netplan::Step::None:`:

```cpp
        case netplan::Step::Genre: {
            tick();
            uint32_t id = 0;
            int rc = 0;
            applegenre::Result r = applegenre::lookup(s_st.artist, &id, &rc);
            if (r == applegenre::Result::Found) {
                uint8_t badge = genrebadge::forGenreId(id);
                s_genres.store(s_st.artist, badge);
                shared::postGenre(gen, badge);
                Serial.printf("[genre] \"%s\": %u -> %s\n", s_st.artist, (unsigned)id,
                              genrebadge::at(badge).label);
            } else if (r == applegenre::Result::None) {
                s_genres.store(s_st.artist, genre::NONE);   // Apple doesn't know: don't ask again
                Serial.printf("[genre] \"%s\": none\n", s_st.artist);
            } else {
                Serial.printf("[genre] \"%s\": error rc=%d\n", s_st.artist, rc);   // not cached
            }
            tock("genre");
            break;
        }
```

- [ ] **Step 5: Build**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`; RAM up by ~1.2 KB (1 KB body buffer + cache), within the budget.

- [ ] **Step 6: Commit**

```bash
git add src/genre/apple.h src/genre/apple.cpp src/core/shared.h src/core/shared.cpp src/core/nettask.cpp
git commit -m "feat(genre): look up the artist genre on Apple, cache 32 artists, post the badge" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Draw the badge on the artist row (device)

**Files:**
- Modify: `src/ui/screen_now.h`, `src/ui/screen_now.cpp`, `src/main.cpp`

**Interfaces:**
- Consumes: `genrebadge::W/H/at/count`, `genre::NONE`, `shared::takeGenre`.
- Produces: `void ui::drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre);` (the unused `accent` parameter becomes the genre badge index, `genre::NONE` = none); `void ui::drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre);`

- [ ] **Step 1: `src/ui/screen_now.h`** — replace the `drawNow` declaration line with

```cpp
// Full deck. genre: genrebadge index for the artist row, or genre::NONE (no badge yet/none).
void drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre);
// Repaints only the artist row in the info box (badge + name) when the genre arrives.
void drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre);
```

- [ ] **Step 2: `src/ui/screen_now.cpp`** — add includes after `#include "typebadge.h"`:

```cpp
#include "genrebadge.h"
#include "../util/genre.h"
```

add before `void drawNow(`:

```cpp
// Gen-3 summary-style badge: rounded box in the colour, darker border, white font-1 label.
static void drawBadge(TFT_eSPI& t, int x, int y, int w, int h, const char* label, uint16_t c) {
    uint16_t edge = theme::darken(c);
    t.fillRoundRect(x, y, w, h, 3, c);
    t.drawRoundRect(x, y, w, h, 3, edge);
    shadowText(t, label, x + w / 2, y + h / 2, 1, TFT_WHITE, edge, MC_DATUM);
}

// Info box, artist row: [GENRE] Artist. Without a badge the name keeps the full width.
void drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre) {
    const int TW = INFO_W - 26, x0 = INFO_X + 8, y = INFO_Y + 32;
    t.fillRect(x0, y - 1, TW, 19, theme::BOX_FILL);
    int x = x0, maxW = TW;
    if (genre != genre::NONE && genre < genrebadge::count()) {
        const genrebadge::Badge& b = genrebadge::at(genre);
        drawBadge(t, x, y + 2, genrebadge::W, genrebadge::H, b.label, b.color);
        x += genrebadge::W + 6;
        maxW -= genrebadge::W + 6;
    }
    drawText(t, st.artist[0] ? st.artist : "Artist", x, y, 2, theme::TEXT, theme::TEXT_SHADOW,
             TL_DATUM, maxW);
}
```

change `void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent) {` to `void drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre) {`, replace the two-line artist `drawText(...)` call in `drawNow` with

```cpp
    drawArtistRow(t, st, genre);
```

and replace the type badge's three drawing lines

```cpp
        uint16_t c = theme::typeColor(st.pokeType), edge = theme::darken(c);
        t.fillRoundRect(x, 131, typebadge::W, typebadge::H, 3, c);
        t.drawRoundRect(x, 131, typebadge::W, typebadge::H, 3, edge);
        shadowText(t, lbl, x + typebadge::W / 2, 131 + typebadge::H / 2, 1, TFT_WHITE, edge, MC_DATUM);
```

with

```cpp
        drawBadge(t, x, 131, typebadge::W, typebadge::H, lbl, theme::typeColor(st.pokeType));
```

- [ ] **Step 3: `src/main.cpp`** — add `#include "util/genre.h"` after `#include "ui/lyricmsg.h"`. In `loop()`, after `static bool walkerOn = false;` add

```cpp
    static uint8_t shownGenre = genre::NONE;   // badge for the track on screen
```

in the full-redraw branch, inside `if (newTrack) {` add `shownGenre = genre::NONE;`, and change `ui::drawNow(tft, view, theme::typeColor(st.pokeType));` to

```cpp
        ui::drawNow(tft, view, shownGenre);
```

in the media-arrival branch, after `if (shared::takeWalker(shownGen)) walkerOn = true;` add

```cpp
        uint8_t g;
        if (shared::takeGenre(shownGen, &g)) {   // genre arrived (or was cached): artist row only
            shownGenre = g;
            ui::drawArtistRow(tft, view, g);
        }
```

- [ ] **Step 4: Build**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`

- [ ] **Step 5: All host suites**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: `suites pass=25 fail=0`

- [ ] **Step 6: Commit**

```bash
git add src/ui/screen_now.h src/ui/screen_now.cpp src/main.cpp
git commit -m "feat(ui): genre badge before the artist name" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: On-device verification and docs

**Files:**
- Modify: `README.md` (history item 14), `CREDITS.md`

- [ ] **Step 1: Flash**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`
Expected: `[SUCCESS]`

- [ ] **Step 2: Capture ~5 minutes** while playing/skipping songs by different artists, including one artist twice.

Run: `& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe" tools\capture_serial.py COM11 300 <scratchpad>\genre-session.log`, then `Select-String -Path <log> -Pattern "\[genre\]|\[net\] genre|task_wdt|rst:"`
Expected: one `[genre] "<artist>": <id> -> <LABEL>` (or `none`) per *new* artist, none for the repeated artist; `[net] genre` steps ~1–2 s; no `task_wdt`; one `rst:` (power-on) only.

- [ ] **Step 3: Check on screen** (ask the user): badge before the artist name in the info box, right colour/label; repeated artist shows the badge with the title; long artist names end in "..." at 108 px; no badge leaves the name at full width.

- [ ] **Step 4: Docs.** Append to `README.md` "Design & performance history" after item 13:

```markdown
14. **Genre badge.** Spotify no longer returns artist genres to this app, so the genre comes
    from Apple's iTunes Search API (artist search, no key, ~300 B): one request per new artist
    after the lyrics, 32 artists cached. Apple's 478 genre ids map to 29 Gen-3 style badges
    (`tools/gen_genres.py` → `src/ui/genre_data.inc`: subgenres take the parent's badge, with
    overrides such as Baile Funk → FUNK BR); unknown ids show a grey `???`, a nod to Gen 3's
    mystery type.
```

In `CREDITS.md` add a line: `- Artist genres: Apple iTunes Search API (https://performance-partners.apple.com/search-api).`

- [ ] **Step 5: Commit**

```bash
git add README.md CREDITS.md
git commit -m "docs: record the genre badge in the history and credits" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
