# Lyrics Reliability and Dialogue-Box Messages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lyrics that exist on LRCLIB show up (retries, plain-lyrics fallback, search fallback, instrumental detection), and the dialogue box is never empty: Pokemon-style messages while searching, retrying, in the intro and when there are no lyrics.

**Architecture:** The network task fetches (`lyrics/lrclib` → `util/lrcstream` one-pass reader → `util/lyricbuf` arena), classifies each attempt (`util/lyricstatus`), retries temporary errors (`netplan::LyricsRetry`) and publishes a per-track lyrics status through `core/shared`. The UI loop turns *status + time + song position* into the box text with a pure module (`ui/lyricmsg`) and draws it with `ui::drawLyricArea`.

**Tech Stack:** ESP32 Arduino (PlatformIO, `espressif32@6.9.0`), TFT_eSPI, FreeRTOS; host tests with Unity compiled by g++ via `.devtools\ntest.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md`

## Global Constraints

- No new large buffers: the lyrics arena stays `MAX_LINES = 192`, `TEXT_CAP = 4096`; the search response is never held in memory; new state is a few hundred bytes.
- Synced always wins over plain; `instrumental: true` wins even if the entry also has text.
- Search fallback only on HTTP 404 from `/api/get`; accept an object only if its `duration` comes before its lyrics and |duration − track| ≤ 3 s; stop at the first matching synced object; keep the first matching plain object; stop after **48 KB** read.
- Temporary error = HTTP 5xx or 429, negative HTTPClient code (connect/TLS failure, timeout), or a stalled read. Retry after **5 s**, then **20 s**; the third temporary error gives up (`None`).
- Plain lines: blank lines dropped; times spread evenly from **10% to 90%** of the duration using `plainTotal` (stored + counted lines).
- FAIL / INSTRUMENTAL line shows for **3 s**, then the IDLE line. Plain lyrics (and their FOUND line) draw **without** the ♪ note icons.
- Message pick is seeded by the track generation (stable per song). `{P}` = Pokemon name in capitals.
- Commits: Conventional Commits `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Secrets live only in `src/config.h` (git-ignored); never commit it.
- Bash heredocs on this machine collapse backslashes: write any file containing `\` escapes with the Write/Edit tools, never `cat <<EOF`.

## Review Focus

1. Lyrics text containing braces, brackets or escaped quotes (`{`, `]`, `\"`) must not shift object tracking in search responses: the right entry is still chosen. (Task 3 `test_search_braces_in_strings_do_not_confuse_objects`)
2. A track with no duration (`durationMs == 0`) must still get plain lines with increasing times, not all at 0. (Task 1 `test_spread_plain_without_duration_is_4s_apart`)
3. Long, accented or empty Pokemon names in `{P}` must never overflow the 160-byte text or cut a UTF-8 character in half. (Task 5 `test_fill_never_cuts_a_utf8_character`, `test_empty_name_uses_pokemon`)
4. Every message, with a 12-letter name, must fit the two-line box (≤ 72 bytes). (Task 5 `test_all_messages_fit_two_lines`)
5. A retry clock across the `millis()` wrap must not stall lyrics for 49 days. (Task 4 `test_lyrics_retry_ready_survives_millis_wrap`)

---

## How to run things

- One host suite: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 <test.cpp> <module.cpp ...>` (exit 0 = pass).
- All host suites: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` (created in Task 1).
- Firmware build: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
- Flash: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`
- Timestamped serial capture: `& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe" tools\capture_serial.py COM11 600 <out.log>`; summary: `... python.exe tools\analyze_session.py <out.log>`

---

### Task 1: Plain lines and spreading in the lyrics arena

**Files:**
- Create: `tools/run_tests.ps1`
- Modify: `src/util/lyricbuf.h`, `src/util/lyricbuf.cpp`
- Test: `test/test_lyricbuf/test_lyricbuf.cpp`

**Interfaces:**
- Consumes: existing `lyricbuf::Lyrics`, `reset`, `addLine`, `finish`, `currentIndex`, `lineText`.
- Produces:
  - `struct Lyrics { int n; int used; int plainTotal; Line lines[MAX_LINES]; char text[TEXT_CAP]; };`
  - `bool hasTag(const char* line, size_t len);`
  - `bool addPlain(Lyrics& out, const char* line, size_t len);` (blank → true, not counted; non-blank → `plainTotal++`; false when it did not fit)
  - `void spreadPlain(Lyrics& out, uint32_t durationMs);`

- [ ] **Step 1: Add the all-suites runner** `tools/run_tests.ps1` (write with the Write tool):

```powershell
# Runs every host test suite in test/ with .devtools\ntest.ps1, compiling each test together
# with the src/ modules it (transitively) #includes.
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
function Deps($file, $seen) {
    $dir = Split-Path $file -Parent
    foreach ($m in (Select-String -Path $file -Pattern '#include\s+"([^"]+)"' -AllMatches).Matches) {
        $inc = [System.IO.Path]::GetFullPath((Join-Path $dir $m.Groups[1].Value))
        if (-not (Test-Path $inc) -or $seen.Contains($inc)) { continue }
        [void]$seen.Add($inc)
        Deps $inc $seen
        if ($inc -like "*.h") {
            $cpp = [System.IO.Path]::ChangeExtension($inc, ".cpp")
            if ((Test-Path $cpp) -and -not $seen.Contains($cpp)) { [void]$seen.Add($cpp); Deps $cpp $seen }
        }
    }
}
$fail = 0; $pass = 0
foreach ($t in Get-ChildItem test -Directory) {
    $tf = Get-ChildItem $t.FullName -Filter *.cpp | Select-Object -First 1
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    Deps $tf.FullName $seen
    $mods = @($seen | Where-Object { $_ -like "*.cpp" })
    $o = & powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 $tf.FullName @mods 2>&1
    if ($LASTEXITCODE -eq 0) { $pass++; Write-Host "PASS $($t.Name): $(($o | Select-String 'Tests').Line)" }
    else { $fail++; Write-Host "FAIL $($t.Name)"; $o | Select-Object -Last 15 }
}
Write-Host "suites pass=$pass fail=$fail"
exit $fail
```

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: `suites pass=22 fail=0`

- [ ] **Step 2: Write the failing tests.** In `test/test_lyricbuf/test_lyricbuf.cpp`, add before `int main`:

```cpp
void test_has_tag() {
    TEST_ASSERT_TRUE(lyricbuf::hasTag("[00:01.00]x", 11));
    TEST_ASSERT_FALSE(lyricbuf::hasTag("hello there", 11));
    TEST_ASSERT_FALSE(lyricbuf::hasTag("[bad]x", 6));
}
void test_plain_lines_keep_order_and_skip_blanks() {
    lyricbuf::reset(L);
    const char* in[] = {"first", "", "  ", "second\r"};
    for (const char* s : in) TEST_ASSERT_TRUE(lyricbuf::addPlain(L, s, strlen(s)));
    TEST_ASSERT_EQUAL_INT(2, L.n);
    TEST_ASSERT_EQUAL_INT(2, L.plainTotal);
    TEST_ASSERT_EQUAL_STRING("first", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_STRING("second", lyricbuf::lineText(L, 1));
}
void test_plain_overflow_still_counts_lines() {
    lyricbuf::reset(L);
    std::string line(50, 'p');
    int stored = 0;
    for (int i = 0; i < 200; ++i)
        if (lyricbuf::addPlain(L, line.c_str(), line.size())) ++stored;
    TEST_ASSERT_EQUAL_INT(stored, L.n);
    TEST_ASSERT_TRUE(L.n > 50 && L.n < 200);
    TEST_ASSERT_EQUAL_INT(200, L.plainTotal);
}
void test_spread_plain_over_10_to_90_percent() {
    lyricbuf::reset(L);
    for (const char* s : {"a", "b", "c", "d"}) lyricbuf::addPlain(L, s, 1);
    lyricbuf::spreadPlain(L, 100000);
    TEST_ASSERT_EQUAL_UINT32(10000, L.lines[0].tMs);
    TEST_ASSERT_EQUAL_UINT32(30000, L.lines[1].tMs);
    TEST_ASSERT_EQUAL_UINT32(50000, L.lines[2].tMs);
    TEST_ASSERT_EQUAL_UINT32(70000, L.lines[3].tMs);
    TEST_ASSERT_EQUAL_INT(-1, lyricbuf::currentIndex(L, 9999));
    TEST_ASSERT_EQUAL_INT(0, lyricbuf::currentIndex(L, 10000));
}
void test_spread_plain_keeps_share_of_lines_that_did_not_fit() {
    lyricbuf::reset(L);
    lyricbuf::addPlain(L, "a", 1);
    lyricbuf::addPlain(L, "b", 1);
    L.plainTotal = 8;   // 6 more lines were counted but not stored
    lyricbuf::spreadPlain(L, 100000);
    TEST_ASSERT_EQUAL_UINT32(10000, L.lines[0].tMs);
    TEST_ASSERT_EQUAL_UINT32(20000, L.lines[1].tMs);
}
void test_spread_plain_without_duration_is_4s_apart() {
    lyricbuf::reset(L);
    for (const char* s : {"a", "b", "c"}) lyricbuf::addPlain(L, s, 1);
    lyricbuf::spreadPlain(L, 0);
    TEST_ASSERT_EQUAL_UINT32(0, L.lines[0].tMs);
    TEST_ASSERT_EQUAL_UINT32(4000, L.lines[1].tMs);
    TEST_ASSERT_EQUAL_UINT32(8000, L.lines[2].tMs);
}
void test_reset_clears_plain_total() {
    lyricbuf::reset(L);
    lyricbuf::addPlain(L, "a", 1);
    lyricbuf::reset(L);
    TEST_ASSERT_EQUAL_INT(0, L.plainTotal);
}
```

and register them in `main` after `RUN_TEST(test_null_and_out_of_range);`:

```cpp
    RUN_TEST(test_has_tag);
    RUN_TEST(test_plain_lines_keep_order_and_skip_blanks);
    RUN_TEST(test_plain_overflow_still_counts_lines);
    RUN_TEST(test_spread_plain_over_10_to_90_percent);
    RUN_TEST(test_spread_plain_keeps_share_of_lines_that_did_not_fit);
    RUN_TEST(test_spread_plain_without_duration_is_4s_apart);
    RUN_TEST(test_reset_clears_plain_total);
```

- [ ] **Step 3: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lyricbuf\test_lyricbuf.cpp src\util\lyricbuf.cpp`
Expected: `COMPILE FAILED` — `'hasTag' is not a member of 'lyricbuf'`, `'struct lyricbuf::Lyrics' has no member named 'plainTotal'`.

- [ ] **Step 4: Implement.** In `src/util/lyricbuf.h` replace the `Lyrics` struct and add declarations after `void finish(Lyrics& out);   // sorts by time`:

```cpp
// plainTotal: non-blank plain lines seen, including those that did not fit (spreadPlain).
struct Lyrics { int n; int used; int plainTotal; Line lines[MAX_LINES]; char text[TEXT_CAP]; };
```

```cpp
// True when `line` starts with a valid "[mm:ss.xx]" tag.
bool hasTag(const char* line, size_t len);
// Adds one untimed (plain) line, tMs 0. Blank lines (verse breaks) are skipped and not counted.
// Every other line increments out.plainTotal; false when it did not fit (arena full).
bool addPlain(Lyrics& out, const char* line, size_t len);
// Gives plain lines times spread evenly from 10% to 90% of durationMs, as if plainTotal lines
// were spread (stored lines keep their true share when the tail did not fit). No duration
// (0): 4 s apart from the start.
void spreadPlain(Lyrics& out, uint32_t durationMs);
```

In `src/util/lyricbuf.cpp`: in `reset` add `out.plainTotal = 0;`, and add after `finish`:

```cpp
bool hasTag(const char* line, size_t len) {
    uint32_t t;
    const char* txt;
    return parseTag(line, line + len, &t, &txt);
}

bool addPlain(Lyrics& out, const char* p, size_t len) {
    const char* end = p + len;
    while (end > p && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) --end;
    const char* b = p;
    while (b < end && (*b == ' ' || *b == '\t')) ++b;
    if (b == end) return true;            // blank: verse break, not a line
    ++out.plainTotal;
    int n = (int)(end - p);
    if (out.n >= MAX_LINES || out.used + n + 1 > TEXT_CAP) return false;
    memcpy(out.text + out.used, p, n);
    out.text[out.used + n] = '\0';
    out.lines[out.n++] = {0, (uint16_t)out.used};
    out.used += n + 1;
    return true;
}

void spreadPlain(Lyrics& out, uint32_t durationMs) {
    int total = out.plainTotal > out.n ? out.plainTotal : out.n;
    if (total <= 0) return;
    uint64_t start = (uint64_t)durationMs / 10, span = (uint64_t)durationMs * 8 / 10;
    for (int i = 0; i < out.n; ++i)
        out.lines[i].tMs = durationMs ? (uint32_t)(start + span * (uint64_t)i / (uint64_t)total)
                                      : (uint32_t)i * 4000u;
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lyricbuf\test_lyricbuf.cpp src\util\lyricbuf.cpp`
Expected: `14 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 6: Commit**

```bash
git add tools/run_tests.ps1 src/util/lyricbuf.h src/util/lyricbuf.cpp test/test_lyricbuf/test_lyricbuf.cpp
git commit -m "feat(lyrics): plain lines and 10-90% spreading in the lyrics arena" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Stream reader: plain, instrumental, synced-replaces-plain (`/api/get`)

**Files:**
- Modify: `src/util/lrcstream.h`, `src/util/lrcstream.cpp` (full rewrite)
- Test: `test/test_lrcstream/test_lrcstream.cpp` (full rewrite)

**Interfaces:**
- Consumes: Task 1 `lyricbuf::hasTag`, `lyricbuf::addPlain`, `Lyrics::plainTotal`.
- Produces (used by Tasks 3 and 6):
  - `enum class lrcstream::Mode { Get, Search };`
  - `enum class lrcstream::Kind { None, Synced, Plain, Instrumental };`
  - `constexpr size_t lrcstream::SEARCH_CAP = 48 * 1024;` `constexpr uint32_t lrcstream::DURATION_TOL_MS = 3000;` `constexpr int lrcstream::LINE_CAP = 256;`
  - `Extractor(lyricbuf::Lyrics& out, Mode mode = Mode::Get, uint32_t durationMs = 0);`
  - `void feed(char)`, `bool done() const`, `bool found() const` (synced lines stored), `bool truncated() const`, `int finish()` (lines stored), `Kind kind() const` (valid after `finish`).

- [ ] **Step 1: Write the failing tests.** Replace `test/test_lrcstream/test_lrcstream.cpp` with (Write tool):

```cpp
#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/lrcstream.h"

using lrcstream::Kind;
static lyricbuf::Lyrics L;
void setUp() {}
void tearDown() {}

static int feedAll(lrcstream::Extractor& ex, const char* json) {
    for (const char* p = json; *p && !ex.done(); ++p) ex.feed(*p);
    return ex.finish();
}

void test_extracts_synced_lines() {
    lrcstream::Extractor ex(L);
    int n = feedAll(ex, "{\"id\":1,\"syncedLyrics\":\"[00:01.00]hello\\n[00:03.50]world\\n\"}");
    TEST_ASSERT_TRUE(ex.found());
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_STRING("hello", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_UINT32(3500, L.lines[1].tMs);
    TEST_ASSERT_FALSE(ex.truncated());
    TEST_ASSERT_EQUAL_INT((int)Kind::Synced, (int)ex.kind());
}
void test_unescapes_quotes_backslash_and_unicode() {
    lrcstream::Extractor ex(L);
    feedAll(ex, "{\"syncedLyrics\":\"[00:01.00]say \\\"hi\\\" a\\\\b\\n"
                "[00:02.00]cora\\u00e7\\u00e3o\\n[00:03.00]\\u3042\\u611b\\n"
                "[00:04.00]x\\ud83d\\ude00y\"}");
    TEST_ASSERT_EQUAL_STRING("say \"hi\" a\\b", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_STRING("cora\xC3\xA7\xC3\xA3o", lyricbuf::lineText(L, 1));
    TEST_ASSERT_EQUAL_STRING("\xE3\x81\x82\xE6\x84\x9B", lyricbuf::lineText(L, 2));
    TEST_ASSERT_EQUAL_STRING("x\xF0\x9F\x98\x80y", lyricbuf::lineText(L, 3));
}
void test_raw_utf8_passes_through() {
    lrcstream::Extractor ex(L);
    feedAll(ex, "{\"syncedLyrics\":\"[00:01.00]\xE6\x84\x9B\xE3\x81\x82\"}");
    TEST_ASSERT_EQUAL_STRING("\xE6\x84\x9B\xE3\x81\x82", lyricbuf::lineText(L, 0));
}
void test_whitespace_around_colon_and_done_after_string() {
    lrcstream::Extractor ex(L);
    const char* json = "{ \"syncedLyrics\" :  \"[00:01.00]a\" , \"x\": \"[00:09.00]zzz\" }";
    const char* p = json;
    for (; *p && !ex.done(); ++p) ex.feed(*p);
    TEST_ASSERT_TRUE(ex.done());
    TEST_ASSERT_TRUE(*p != '\0');                 // stopped before the end of the document
    TEST_ASSERT_EQUAL_INT(1, ex.finish());
}
void test_arena_full_sets_truncated() {
    std::string json = "{\"syncedLyrics\":\"";
    for (int i = 0; i < 200; ++i) {               // 200 lines x 50 chars > 4096 B arena
        char tag[16];
        snprintf(tag, sizeof(tag), "[%02d:%02d.00]", i / 60, i % 60);
        json += tag + std::string(50, 'x') + "\\n";
    }
    json += "\"}";
    lrcstream::Extractor ex(L);
    int n = feedAll(ex, json.c_str());
    TEST_ASSERT_TRUE(ex.truncated());
    TEST_ASSERT_TRUE(n > 50 && n < 200);
}
void test_overlong_line_is_cut_not_overflowed() {
    std::string json = "{\"syncedLyrics\":\"[00:01.00]" + std::string(1000, 'y') + "\\n[00:02.00]ok\"}";
    lrcstream::Extractor ex(L);
    TEST_ASSERT_EQUAL_INT(2, feedAll(ex, json.c_str()));
    TEST_ASSERT_TRUE(strlen(lyricbuf::lineText(L, 0)) < lrcstream::LINE_CAP);
    TEST_ASSERT_EQUAL_STRING("ok", lyricbuf::lineText(L, 1));
}
void test_null_synced_falls_back_to_plain() {
    lrcstream::Extractor ex(L);
    TEST_ASSERT_EQUAL_INT(1, feedAll(ex, "{\"plainLyrics\":\"x\",\"syncedLyrics\":null}"));
    TEST_ASSERT_FALSE(ex.found());
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
    TEST_ASSERT_EQUAL_STRING("x", lyricbuf::lineText(L, 0));
}
void test_no_lyrics_fields_is_none() {
    lrcstream::Extractor ex(L);
    TEST_ASSERT_EQUAL_INT(0, feedAll(ex, "{\"id\":7,\"plainLyrics\":null,\"syncedLyrics\":null}"));
    TEST_ASSERT_EQUAL_INT((int)Kind::None, (int)ex.kind());
}
void test_plain_only_skips_blank_lines() {
    lrcstream::Extractor ex(L);
    int n = feedAll(ex, "{\"plainLyrics\":\"one\\ntwo\\n\\nthree\\n\",\"syncedLyrics\":\"\"}");
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
    TEST_ASSERT_EQUAL_STRING("three", lyricbuf::lineText(L, 2));
    TEST_ASSERT_EQUAL_INT(3, L.plainTotal);
    TEST_ASSERT_FALSE(ex.found());                // "" counts as absent
}
void test_synced_after_plain_replaces_it() {
    lrcstream::Extractor ex(L);
    feedAll(ex, "{\"plainLyrics\":\"p1\\np2\\np3\",\"syncedLyrics\":\"[00:02.00]s1\\n[00:04.00]s2\"}");
    TEST_ASSERT_EQUAL_INT((int)Kind::Synced, (int)ex.kind());
    TEST_ASSERT_EQUAL_INT(2, L.n);
    TEST_ASSERT_EQUAL_STRING("s1", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_INT(0, L.plainTotal);
}
void test_synced_before_plain_ignores_plain() {
    lrcstream::Extractor ex(L);
    feedAll(ex, "{\"syncedLyrics\":\"[00:02.00]s1\",\"plainLyrics\":\"p1\\np2\"}");
    TEST_ASSERT_EQUAL_INT((int)Kind::Synced, (int)ex.kind());
    TEST_ASSERT_EQUAL_INT(1, L.n);
}
void test_instrumental_wins_even_with_text() {
    lrcstream::Extractor ex(L);
    const char* json = "{\"duration\":200,\"instrumental\":true,\"plainLyrics\":\"la la\","
                       "\"syncedLyrics\":\"[00:01.00]la\"}";
    const char* p = json;
    for (; *p && !ex.done(); ++p) ex.feed(*p);
    TEST_ASSERT_TRUE(ex.done());
    TEST_ASSERT_TRUE(*p != '\0');                 // stopped early: the text is not needed
    ex.finish();
    TEST_ASSERT_EQUAL_INT((int)Kind::Instrumental, (int)ex.kind());
}
void test_instrumental_false_is_ignored() {
    lrcstream::Extractor ex(L);
    feedAll(ex, "{\"instrumental\":false,\"plainLyrics\":\"words\"}");
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
}
void test_plain_overflow_is_counted_and_truncated() {
    std::string json = "{\"plainLyrics\":\"";
    for (int i = 0; i < 200; ++i) json += std::string(50, 'q') + "\\n";
    json += "\"}";
    lrcstream::Extractor ex(L);
    int n = feedAll(ex, json.c_str());
    TEST_ASSERT_TRUE(ex.truncated());
    TEST_ASSERT_TRUE(n > 50 && n < 200);
    TEST_ASSERT_EQUAL_INT(200, L.plainTotal);
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
}
void test_keys_inside_strings_and_nested_objects_are_ignored() {
    lrcstream::Extractor ex(L);
    feedAll(ex, "{\"name\":\"say \\\"syncedLyrics\\\": {hi}\",\"meta\":{\"syncedLyrics\":\"[00:01.00]no\"},"
                "\"plainLyrics\":\"yes\"}");
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
    TEST_ASSERT_EQUAL_INT(1, L.n);
    TEST_ASSERT_EQUAL_STRING("yes", lyricbuf::lineText(L, 0));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_extracts_synced_lines);
    RUN_TEST(test_unescapes_quotes_backslash_and_unicode);
    RUN_TEST(test_raw_utf8_passes_through);
    RUN_TEST(test_whitespace_around_colon_and_done_after_string);
    RUN_TEST(test_arena_full_sets_truncated);
    RUN_TEST(test_overlong_line_is_cut_not_overflowed);
    RUN_TEST(test_null_synced_falls_back_to_plain);
    RUN_TEST(test_no_lyrics_fields_is_none);
    RUN_TEST(test_plain_only_skips_blank_lines);
    RUN_TEST(test_synced_after_plain_replaces_it);
    RUN_TEST(test_synced_before_plain_ignores_plain);
    RUN_TEST(test_instrumental_wins_even_with_text);
    RUN_TEST(test_instrumental_false_is_ignored);
    RUN_TEST(test_plain_overflow_is_counted_and_truncated);
    RUN_TEST(test_keys_inside_strings_and_nested_objects_are_ignored);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lrcstream\test_lrcstream.cpp src\util\lrcstream.cpp src\util\lyricbuf.cpp`
Expected: `COMPILE FAILED` — `'Kind' in namespace 'lrcstream' does not name a type` / `'class lrcstream::Extractor' has no member named 'kind'`.

- [ ] **Step 3: Implement.** Replace `src/util/lrcstream.h` with (Write tool):

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "lyricbuf.h"
// Streams lyrics out of an LRCLIB JSON response byte by byte, without a JSON document
// (ArduinoJson grew strings by doubling, up to 16 KB contiguous while TLS was open).
// Reads "instrumental", "plainLyrics" and "syncedLyrics" (plus "duration" in search mode),
// unescaping strings (\n \" \\ \/ \t \uXXXX incl. surrogate pairs -> UTF-8) straight into a
// lyricbuf arena one line at a time. Synced lines replace plain ones; instrumental wins.
// Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md. PURE, host-tested.
namespace lrcstream {
constexpr int LINE_CAP = 256;               // longer lines are cut
constexpr size_t SEARCH_CAP = 48 * 1024;    // search mode: stop after this many bytes
constexpr uint32_t DURATION_TOL_MS = 3000;  // search mode: accepted |duration - track|
enum class Mode { Get, Search };            // /api/get object, /api/search array of objects
enum class Kind { None, Synced, Plain, Instrumental };
class Extractor {
public:
    // Search mode skips objects whose duration (listed before their lyrics) is more than
    // DURATION_TOL_MS away from durationMs.
    explicit Extractor(lyricbuf::Lyrics& out, Mode mode = Mode::Get, uint32_t durationMs = 0);
    void feed(char c);
    bool done() const { return done_; }               // stop reading the stream
    bool found() const { return syncedLines_ > 0; }   // synced lines were stored
    bool truncated() const { return truncated_; }     // arena filled up
    int finish();                                      // flush + sort; returns stored lines
    Kind kind() const { return kind_; }                // valid after finish()
private:
    enum class Str { None, Key, Skip, Plain, Synced };
    enum class Esc { None, Backslash, Unicode };
    static constexpr int KEY_CAP = 16, LIT_CAP = 24;
    bool accepted() const;
    void stringChar(char c);
    void beginValueString();
    void endValueString();
    void endLiteral();
    void put(char c);
    void putCodepoint(uint32_t cp);
    void endLine();
    lyricbuf::Lyrics& out_;
    Mode mode_;
    uint32_t targetMs_;
    int objDepth_;                 // nesting level whose keys we read (get: 1, search: 2)
    int depth_ = 0;
    bool inStr_ = false, inLit_ = false, expectValue_ = false;
    Str str_ = Str::None;
    Esc esc_ = Esc::None;
    char key_[KEY_CAP] = {0}, curKey_[KEY_CAP] = {0};
    int keyLen_ = 0;
    char lit_[LIT_CAP] = {0};
    int litLen_ = 0;
    char line_[LINE_CAP];
    int len_ = 0;
    uint32_t hex_ = 0, hi_ = 0;
    int hexDigits_ = 0;
    bool objMatch_ = false;        // search: this object's duration matched
    bool plainTaken_ = false;      // the first accepted plain string was read
    bool instrumental_ = false, truncated_ = false, done_ = false;
    int syncedLines_ = 0;
    size_t bytes_ = 0;
    Kind kind_ = Kind::None;
};
}
```

Replace `src/util/lrcstream.cpp` with (Write tool):

```cpp
#include "lrcstream.h"
#include <cstdlib>
#include <cstring>

namespace lrcstream {

static bool keyIs(const char* k, const char* name) { return strcmp(k, name) == 0; }

Extractor::Extractor(lyricbuf::Lyrics& out, Mode mode, uint32_t durationMs)
    : out_(out), mode_(mode), targetMs_(durationMs), objDepth_(1) {
    lyricbuf::reset(out_);
}

bool Extractor::accepted() const { return true; }

void Extractor::put(char c) {
    if (len_ < LINE_CAP - 1) line_[len_++] = c;   // overlong line: cut, never overflow
}

void Extractor::putCodepoint(uint32_t cp) {
    if (cp < 0x80) {
        put((char)cp);
    } else if (cp < 0x800) {
        put((char)(0xC0 | (cp >> 6)));
        put((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        put((char)(0xE0 | (cp >> 12)));
        put((char)(0x80 | ((cp >> 6) & 0x3F)));
        put((char)(0x80 | (cp & 0x3F)));
    } else {
        put((char)(0xF0 | (cp >> 18)));
        put((char)(0x80 | ((cp >> 12) & 0x3F)));
        put((char)(0x80 | ((cp >> 6) & 0x3F)));
        put((char)(0x80 | (cp & 0x3F)));
    }
}

void Extractor::endLine() {
    if (str_ == Str::Synced) {
        if (lyricbuf::hasTag(line_, (size_t)len_)) {          // untagged lines are skipped
            if (syncedLines_ == 0) {                          // synced replaces any plain lines
                lyricbuf::reset(out_);
                truncated_ = false;
            }
            if (lyricbuf::addLine(out_, line_, (size_t)len_)) ++syncedLines_;
            else { truncated_ = true; done_ = true; }        // arena full: rest not needed
        }
    } else if (str_ == Str::Plain) {
        if (!lyricbuf::addPlain(out_, line_, (size_t)len_)) truncated_ = true;   // still counted
    }
    len_ = 0;
}

void Extractor::beginValueString() {
    inStr_ = true;
    esc_ = Esc::None;
    len_ = 0;
    str_ = Str::Skip;
    if (depth_ != objDepth_ || !accepted()) return;
    if (keyIs(curKey_, "syncedLyrics")) {
        str_ = Str::Synced;
    } else if (keyIs(curKey_, "plainLyrics") && syncedLines_ == 0 && !plainTaken_) {
        str_ = Str::Plain;
        plainTaken_ = true;
    }
}

void Extractor::endValueString() {
    if (len_ > 0) endLine();
    Str s = str_;
    inStr_ = false;
    expectValue_ = false;
    str_ = Str::None;
    if (s == Str::Synced && syncedLines_ > 0) done_ = true;   // timed lyrics: nothing else needed
}

void Extractor::stringChar(char c) {
    if (str_ == Str::Key || str_ == Str::Skip) {
        if (esc_ != Esc::None) {
            esc_ = Esc::None;
        } else if (c == '\\') {
            esc_ = Esc::Backslash;
            return;
        } else if (c == '"') {
            inStr_ = false;
            if (str_ == Str::Key) key_[keyLen_] = '\0';
            else expectValue_ = false;
            str_ = Str::None;
            return;
        }
        if (str_ == Str::Key && keyLen_ < KEY_CAP - 1) key_[keyLen_++] = c;
        return;
    }
    // Lyric text (Plain / Synced).
    if (esc_ == Esc::Unicode) {
        int v = (c >= '0' && c <= '9') ? c - '0'
              : (c >= 'a' && c <= 'f') ? c - 'a' + 10
              : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) { esc_ = Esc::None; return; }               // malformed escape: dropped
        hex_ = (hex_ << 4) | (uint32_t)v;
        if (++hexDigits_ < 4) return;
        esc_ = Esc::None;
        if (hex_ >= 0xD800 && hex_ <= 0xDBFF) {                 // high surrogate: wait for low
            hi_ = hex_;
        } else if (hex_ >= 0xDC00 && hex_ <= 0xDFFF) {          // low surrogate
            if (hi_) putCodepoint(0x10000 + ((hi_ - 0xD800) << 10) + (hex_ - 0xDC00));
            hi_ = 0;
        } else {
            hi_ = 0;
            putCodepoint(hex_);
        }
        return;
    }
    if (esc_ == Esc::Backslash) {
        esc_ = Esc::None;
        switch (c) {
            case 'n': endLine(); break;
            case 't': put(' '); break;
            case 'r': break;
            case 'u': esc_ = Esc::Unicode; hex_ = 0; hexDigits_ = 0; break;
            default: put(c); break;   // \" \\ \/ (and anything unexpected, literally)
        }
        return;
    }
    if (c == '\\') { esc_ = Esc::Backslash; return; }
    if (c == '"') { endValueString(); return; }
    put(c);                           // raw UTF-8 bytes pass through
}

void Extractor::endLiteral() {
    lit_[litLen_] = '\0';
    expectValue_ = false;
    if (depth_ != objDepth_) return;
    if (keyIs(curKey_, "instrumental") && strcmp(lit_, "true") == 0 && accepted()) {
        instrumental_ = true;
        done_ = true;                 // instrumental wins over any text
    }
}

void Extractor::feed(char c) {
    if (done_) return;
    if (inStr_) { stringChar(c); return; }
    if (inLit_) {
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            inLit_ = false;
            endLiteral();
            if (done_) return;
        } else {
            if (litLen_ < LIT_CAP - 1) lit_[litLen_++] = c;
            return;
        }
    }
    switch (c) {
        case '"':
            if (expectValue_) {
                beginValueString();
            } else {                  // a key (or a string inside a nested value)
                inStr_ = true;
                esc_ = Esc::None;
                str_ = Str::Key;
                keyLen_ = 0;
                key_[0] = '\0';
            }
            break;
        case ':':
            if (depth_ == objDepth_) {
                memcpy(curKey_, key_, sizeof(curKey_));
                expectValue_ = true;
            }
            break;
        case '{':
            ++depth_;
            expectValue_ = false;
            break;
        case '[':
            ++depth_;
            expectValue_ = false;
            break;
        case '}':
        case ']':
            --depth_;
            expectValue_ = false;
            break;
        case ',':
            expectValue_ = false;
            break;
        case ' ': case '\t': case '\r': case '\n':
            break;
        default:                      // number / true / false / null
            if (expectValue_) {
                inLit_ = true;
                litLen_ = 0;
                lit_[litLen_++] = c;
            }
            break;
    }
}

int Extractor::finish() {
    if (inStr_ && (str_ == Str::Plain || str_ == Str::Synced) && len_ > 0) endLine();   // cut off mid-string
    lyricbuf::finish(out_);
    if (instrumental_) kind_ = Kind::Instrumental;
    else if (syncedLines_ > 0) kind_ = Kind::Synced;
    else if (out_.n > 0) kind_ = Kind::Plain;
    else kind_ = Kind::None;
    return out_.n;
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lrcstream\test_lrcstream.cpp src\util\lrcstream.cpp src\util\lyricbuf.cpp`
Expected: `15 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 5: Build check** (lrclib.cpp still calls `found()` / `truncated()` / `finish()`, which keep their meaning).

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`

- [ ] **Step 6: Commit**

```bash
git add src/util/lrcstream.h src/util/lrcstream.cpp test/test_lrcstream/test_lrcstream.cpp
git commit -m "feat(lyrics): stream reader keeps plain lyrics and detects instrumentals" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Stream reader: search mode (`/api/search`)

**Files:**
- Modify: `src/util/lrcstream.cpp`
- Test: `test/test_lrcstream/test_lrcstream.cpp`

**Interfaces:**
- Consumes: Task 2 `Extractor(out, Mode::Search, durationMs)`, `SEARCH_CAP`, `DURATION_TOL_MS`.
- Produces: search-mode behaviour (no new names).

- [ ] **Step 1: Write the failing tests.** Add before `int main` in `test/test_lrcstream/test_lrcstream.cpp`:

```cpp
using lrcstream::Mode;
void test_search_picks_entry_by_duration() {
    lrcstream::Extractor ex(L, Mode::Search, 305000);
    feedAll(ex, "[{\"id\":1,\"duration\":100.0,\"syncedLyrics\":\"[00:01.00]wrong\"},"
                "{\"id\":2,\"duration\":304.2,\"syncedLyrics\":\"[00:01.00]right\"}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::Synced, (int)ex.kind());
    TEST_ASSERT_EQUAL_STRING("right", lyricbuf::lineText(L, 0));
}
void test_search_tolerance_is_3_seconds() {
    lrcstream::Extractor ex(L, Mode::Search, 189000);
    feedAll(ex, "[{\"duration\":192.5,\"plainLyrics\":\"far\"},{\"duration\":187.43,\"plainLyrics\":\"near\"}]");
    TEST_ASSERT_EQUAL_INT(1, L.n);
    TEST_ASSERT_EQUAL_STRING("near", lyricbuf::lineText(L, 0));
}
void test_search_prefers_a_later_synced_match_over_plain() {
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    feedAll(ex, "[{\"duration\":200,\"plainLyrics\":\"plain one\",\"syncedLyrics\":null},"
                "{\"duration\":201,\"plainLyrics\":\"plain two\",\"syncedLyrics\":\"[00:03.00]timed\"}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::Synced, (int)ex.kind());
    TEST_ASSERT_EQUAL_INT(1, L.n);
    TEST_ASSERT_EQUAL_STRING("timed", lyricbuf::lineText(L, 0));
}
void test_search_keeps_first_plain_match() {
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    feedAll(ex, "[{\"duration\":200,\"plainLyrics\":\"first\"},{\"duration\":200,\"plainLyrics\":\"second\"}]");
    TEST_ASSERT_EQUAL_INT(1, L.n);
    TEST_ASSERT_EQUAL_STRING("first", lyricbuf::lineText(L, 0));
}
void test_search_stops_at_first_synced_match() {
    const char* json = "[{\"duration\":200,\"syncedLyrics\":\"[00:01.00]a\"},"
                       "{\"duration\":200,\"syncedLyrics\":\"[00:01.00]b\"}]";
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    const char* p = json;
    for (; *p && !ex.done(); ++p) ex.feed(*p);
    TEST_ASSERT_TRUE(ex.done());
    TEST_ASSERT_NOT_NULL(strstr(p, "[00:01.00]b"));   // second entry never read
}
void test_search_rejects_entry_without_duration_first() {
    lrcstream::Extractor ex(L, Mode::Search, 305000);
    feedAll(ex, "[{\"syncedLyrics\":\"[00:01.00]x\",\"duration\":305}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::None, (int)ex.kind());
}
void test_search_empty_array_is_none() {
    lrcstream::Extractor ex(L, Mode::Search, 305000);
    TEST_ASSERT_EQUAL_INT(0, feedAll(ex, "[]"));
    TEST_ASSERT_EQUAL_INT((int)Kind::None, (int)ex.kind());
}
void test_search_matching_instrumental() {
    lrcstream::Extractor ex(L, Mode::Search, 120000);
    feedAll(ex, "[{\"duration\":120,\"instrumental\":true,\"plainLyrics\":null}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::Instrumental, (int)ex.kind());
}
void test_search_ignores_instrumental_of_other_versions() {
    lrcstream::Extractor ex(L, Mode::Search, 120000);
    feedAll(ex, "[{\"duration\":90,\"instrumental\":true},{\"duration\":120,\"plainLyrics\":\"sung\"}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
}
void test_search_braces_in_strings_do_not_confuse_objects() {
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    feedAll(ex, "[{\"duration\":10,\"plainLyrics\":\"a {b} [c] \\\"d\\\" }\"},"
                "{\"duration\":200,\"plainLyrics\":\"ok\"}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
    TEST_ASSERT_EQUAL_STRING("ok", lyricbuf::lineText(L, 0));
}
void test_search_reads_at_most_48_kb() {
    std::string json = "[";
    while (json.size() < lrcstream::SEARCH_CAP + 1000)
        json += "{\"duration\":10,\"plainLyrics\":\"" + std::string(500, 'z') + "\"},";
    json += "{\"duration\":200,\"syncedLyrics\":\"[00:01.00]late\"}]";
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    feedAll(ex, json.c_str());
    TEST_ASSERT_TRUE(ex.done());                    // gave up before the end
    TEST_ASSERT_EQUAL_INT((int)Kind::None, (int)ex.kind());
}
```

Register in `main` after the last `RUN_TEST`:

```cpp
    RUN_TEST(test_search_picks_entry_by_duration);
    RUN_TEST(test_search_tolerance_is_3_seconds);
    RUN_TEST(test_search_prefers_a_later_synced_match_over_plain);
    RUN_TEST(test_search_keeps_first_plain_match);
    RUN_TEST(test_search_stops_at_first_synced_match);
    RUN_TEST(test_search_rejects_entry_without_duration_first);
    RUN_TEST(test_search_empty_array_is_none);
    RUN_TEST(test_search_matching_instrumental);
    RUN_TEST(test_search_ignores_instrumental_of_other_versions);
    RUN_TEST(test_search_braces_in_strings_do_not_confuse_objects);
    RUN_TEST(test_search_reads_at_most_48_kb);
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lrcstream\test_lrcstream.cpp src\util\lrcstream.cpp src\util\lyricbuf.cpp`
Expected: FAIL. Every search test that expects lyrics or `done()` fails (keys inside the array's objects are at depth 2 and are not read yet). `test_search_rejects_entry_without_duration_first` and `test_search_empty_array_is_none` already pass: they assert `None`, which they guard after the change.

- [ ] **Step 3: Implement** in `src/util/lrcstream.cpp`:

Constructor: replace `objDepth_(1)` with

```cpp
      objDepth_(mode == Mode::Search ? 2 : 1) {
```

`accepted()`: replace the body with

```cpp
bool Extractor::accepted() const { return mode_ == Mode::Get || objMatch_; }
```

`feed()`: right after `if (done_) return;` add

```cpp
    if (mode_ == Mode::Search && ++bytes_ > SEARCH_CAP) { done_ = true; return; }   // time/data cap
```

`feed()`: replace the `case '{':` block with

```cpp
        case '{':
            ++depth_;
            expectValue_ = false;
            if (depth_ == objDepth_) objMatch_ = false;   // new object: duration not seen yet
            break;
```

`endLiteral()`: after `if (depth_ != objDepth_) return;` add

```cpp
    if (keyIs(curKey_, "duration")) {
        double s = strtod(lit_, nullptr);
        uint32_t ms = (uint32_t)(s * 1000.0 + 0.5);
        uint32_t d = ms > targetMs_ ? ms - targetMs_ : targetMs_ - ms;
        objMatch_ = s > 0 && d <= DURATION_TOL_MS;
        return;
    }
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lrcstream\test_lrcstream.cpp src\util\lrcstream.cpp src\util\lyricbuf.cpp`
Expected: `26 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 5: Commit**

```bash
git add src/util/lrcstream.cpp test/test_lrcstream/test_lrcstream.cpp
git commit -m "feat(lyrics): search-mode reader picks the entry by duration, 48 KB cap" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Lyrics status and retry rule

**Files:**
- Create: `src/util/lyricstatus.h`
- Modify: `src/util/netplan.h`, `src/util/netplan.cpp`
- Test: `test/test_netplan/test_netplan.cpp`

**Interfaces:**
- Produces (used by Tasks 5–7):
  - `enum class lyricstatus::Result : uint8_t { Synced, Plain, Instrumental, NotFound, TempError };`
  - `enum class lyricstatus::Status : uint8_t { Searching, Retrying, Synced, Plain, Instrumental, None };`
  - `lyricstatus::Status lyricstatus::statusFor(Result r, bool final);`
  - `class netplan::LyricsRetry { void reset(); bool onResult(lyricstatus::Result, uint32_t nowMs); bool ready(uint32_t nowMs) const; int attempts() const; }` with `RETRY1_MS = 5000`, `RETRY2_MS = 20000`.
  - `netplan::Step netplan::next(const Work& w, bool lyricsReady = true);`

- [ ] **Step 1: Write the failing tests.** In `test/test_netplan/test_netplan.cpp` add after the existing `#include`:

```cpp
#include "../../src/util/lyricstatus.h"
```

and before `int main`:

```cpp
using lyricstatus::Result;
using lyricstatus::Status;
void test_lyrics_retry_after_5s_then_20s_then_gives_up() {
    netplan::LyricsRetry r;
    TEST_ASSERT_TRUE(r.ready(0));
    TEST_ASSERT_FALSE(r.onResult(Result::TempError, 1000));
    TEST_ASSERT_FALSE(r.ready(1000 + 4999));
    TEST_ASSERT_TRUE(r.ready(1000 + 5000));
    TEST_ASSERT_FALSE(r.onResult(Result::TempError, 7000));
    TEST_ASSERT_FALSE(r.ready(7000 + 19999));
    TEST_ASSERT_TRUE(r.ready(7000 + 20000));
    TEST_ASSERT_TRUE(r.onResult(Result::TempError, 30000));   // third: give up
    TEST_ASSERT_EQUAL_INT(3, r.attempts());
}
void test_lyrics_other_results_are_final() {
    for (Result x : {Result::Synced, Result::Plain, Result::Instrumental, Result::NotFound}) {
        netplan::LyricsRetry r;
        TEST_ASSERT_TRUE(r.onResult(x, 0));
    }
}
void test_lyrics_retry_reset_on_track_change() {
    netplan::LyricsRetry r;
    r.onResult(Result::TempError, 1000);
    r.reset();
    TEST_ASSERT_TRUE(r.ready(1001));
    TEST_ASSERT_EQUAL_INT(0, r.attempts());
}
void test_lyrics_retry_ready_survives_millis_wrap() {
    netplan::LyricsRetry r;
    r.onResult(Result::TempError, 0xFFFFF000u);
    TEST_ASSERT_FALSE(r.ready(0x00000100u));                 // 4352 ms later
    TEST_ASSERT_TRUE(r.ready(0xFFFFF000u + 5000u));
}
void test_next_skips_lyrics_while_waiting() {
    netplan::Work w = netplan::freshWork(false);
    netplan::done(w, Step::Art);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w, false));
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w, true));
    netplan::done(w, Step::Walk);
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w, false));
}
void test_status_for_results() {
    TEST_ASSERT_EQUAL_INT((int)Status::Retrying, (int)lyricstatus::statusFor(Result::TempError, false));
    TEST_ASSERT_EQUAL_INT((int)Status::None, (int)lyricstatus::statusFor(Result::TempError, true));
    TEST_ASSERT_EQUAL_INT((int)Status::None, (int)lyricstatus::statusFor(Result::NotFound, true));
    TEST_ASSERT_EQUAL_INT((int)Status::Synced, (int)lyricstatus::statusFor(Result::Synced, true));
    TEST_ASSERT_EQUAL_INT((int)Status::Plain, (int)lyricstatus::statusFor(Result::Plain, true));
    TEST_ASSERT_EQUAL_INT((int)Status::Instrumental, (int)lyricstatus::statusFor(Result::Instrumental, true));
}
```

Register them in `main` after the last `RUN_TEST`:

```cpp
    RUN_TEST(test_lyrics_retry_after_5s_then_20s_then_gives_up);
    RUN_TEST(test_lyrics_other_results_are_final);
    RUN_TEST(test_lyrics_retry_reset_on_track_change);
    RUN_TEST(test_lyrics_retry_ready_survives_millis_wrap);
    RUN_TEST(test_next_skips_lyrics_while_waiting);
    RUN_TEST(test_status_for_results);
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `COMPILE FAILED` — `lyricstatus.h: No such file or directory`.

- [ ] **Step 3: Implement.** Create `src/util/lyricstatus.h`:

```cpp
#pragma once
#include <cstdint>
// Lyrics outcome, shared by the network task (result of one fetch attempt) and the UI (what
// the dialogue box shows). Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md.
// PURE, host-tested (test_netplan).
namespace lyricstatus {
enum class Result : uint8_t { Synced, Plain, Instrumental, NotFound, TempError };
enum class Status : uint8_t { Searching, Retrying, Synced, Plain, Instrumental, None };
// What the UI shows after an attempt; final = no more attempts for this track.
inline Status statusFor(Result r, bool final) {
    switch (r) {
        case Result::Synced:       return Status::Synced;
        case Result::Plain:        return Status::Plain;
        case Result::Instrumental: return Status::Instrumental;
        case Result::NotFound:     return Status::None;
        case Result::TempError:    return final ? Status::None : Status::Retrying;
    }
    return Status::None;
}
}
```

In `src/util/netplan.h`: add `#include "lyricstatus.h"` after `#include <cstdint>`; replace the `next` declaration with

```cpp
// Next step in priority order: Art, Lyrics, Walk, Prefetch; None when all done.
// Art and lyrics are what the listener waits for; the walker is usually prefetched.
// lyricsReady false (waiting to retry): Lyrics is skipped for now.
Step next(const Work& w, bool lyricsReady = true);
```

and add before the closing `}` of the namespace:

```cpp
// Lyrics retries: a temporary error (HTTP 5xx/429, connect/TLS failure, timeout, stalled read)
// retries after RETRY1_MS, then RETRY2_MS; the third gives up. Other results are final.
// LRCLIB returned 503 on several requests while lyrics went missing (spec 2026-10-07).
class LyricsRetry {
public:
    static constexpr uint32_t RETRY1_MS = 5000;
    static constexpr uint32_t RETRY2_MS = 20000;
    void reset() { attempts_ = 0; waiting_ = false; }   // new track
    // Records one attempt; true when no further attempt will be made for this track.
    bool onResult(lyricstatus::Result r, uint32_t nowMs);
    bool ready(uint32_t nowMs) const { return !waiting_ || nowMs - since_ >= wait_; }   // wrap-safe
    int attempts() const { return attempts_; }
private:
    int attempts_ = 0;
    bool waiting_ = false;
    uint32_t since_ = 0, wait_ = 0;
};
```

In `src/util/netplan.cpp` replace `next` with

```cpp
Step next(const Work& w, bool lyricsReady) {
    if (w.art) return Step::Art;
    if (w.lyrics && lyricsReady) return Step::Lyrics;
    if (w.walk) return Step::Walk;
    if (w.prefetch) return Step::Prefetch;
    return Step::None;
}
```

and add before the closing `}`:

```cpp
bool LyricsRetry::onResult(lyricstatus::Result r, uint32_t nowMs) {
    ++attempts_;
    if (r != lyricstatus::Result::TempError || attempts_ >= 3) {
        waiting_ = false;
        return true;
    }
    waiting_ = true;
    since_ = nowMs;
    wait_ = attempts_ == 1 ? RETRY1_MS : RETRY2_MS;
    return false;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp`
Expected: `24 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 5: Commit**

```bash
git add src/util/lyricstatus.h src/util/netplan.h src/util/netplan.cpp test/test_netplan/test_netplan.cpp
git commit -m "feat(net): lyrics status and retry rule (5 s, 20 s, then give up)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Dialogue-box messages (`ui/lyricmsg`)

**Files:**
- Create: `src/ui/lyricmsg.h`, `src/ui/lyricmsg.cpp`
- Test: `test/test_lyricmsg/test_lyricmsg.cpp`

**Interfaces:**
- Consumes: Task 4 `lyricstatus::Status`.
- Produces (used by Task 7):
  - `constexpr uint32_t lyricmsg::FAIL_SHOW_MS = 3000;` `constexpr size_t lyricmsg::TEXT_CAP = 160;`
  - `struct lyricmsg::In { lyricstatus::Status status; uint32_t statusAgeMs; uint32_t posMs; uint32_t firstLineMs; const char* line; const char* pokeName; uint32_t seed; };`
  - `struct lyricmsg::Out { char text[TEXT_CAP]; bool notes; };`
  - `void lyricmsg::compose(const In&, Out&);` `void lyricmsg::fill(const char* tmpl, const char* name, char* out, size_t n);`
  - pools `SEARCH[10]`, `RETRY[3]`, `FAIL[4]`, `INSTRUMENTAL[3]`, `FOUND[2]`, `IDLE` with `*_N` constants.

- [ ] **Step 1: Write the failing tests.** Create `test/test_lyricmsg/test_lyricmsg.cpp` (Write tool):

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/ui/lyricmsg.h"

using lyricstatus::Status;
void setUp() {}
void tearDown() {}

static lyricmsg::Out out;
static lyricmsg::In base(Status s) { return {s, 0, 0, 0, "", "Pikachu", 7}; }
static bool inPool(const char* const* pool, int n, const char* text, const char* name) {
    char buf[lyricmsg::TEXT_CAP];
    for (int i = 0; i < n; ++i) {
        lyricmsg::fill(pool[i], name, buf, sizeof(buf));
        if (strcmp(buf, text) == 0) return true;
    }
    return false;
}

void test_pools_have_spec_sizes() {
    TEST_ASSERT_EQUAL_INT(10, lyricmsg::SEARCH_N);
    TEST_ASSERT_EQUAL_INT(3, lyricmsg::RETRY_N);
    TEST_ASSERT_EQUAL_INT(4, lyricmsg::FAIL_N);
    TEST_ASSERT_EQUAL_INT(3, lyricmsg::INSTRUMENTAL_N);
    TEST_ASSERT_EQUAL_INT(2, lyricmsg::FOUND_N);
}
void test_fill_replaces_every_P_with_capitals() {
    char b[64];
    lyricmsg::fill("{P} used SING! {P}!", "Pikachu", b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("PIKACHU used SING! PIKACHU!", b);
}
void test_fill_keeps_accents() {
    char b[64];
    lyricmsg::fill("{P}!", "Flab\xC3\xA9" "b\xC3\xA9", b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("FLAB\xC3\xA9" "B\xC3\xA9" "!", b);
}
void test_fill_never_cuts_a_utf8_character() {
    char b[15];                                   // room for 14 bytes
    lyricmsg::fill("{P}", "aaaaaaaaaaaaa\xC3\xA9", b, sizeof(b));   // 13 + 2 bytes
    TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAA", b);
}
void test_fill_long_name_stays_in_bounds() {
    char name[300];
    memset(name, 'x', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    lyricmsg::fill("{P} used SING!", name, out.text, lyricmsg::TEXT_CAP);
    TEST_ASSERT_EQUAL_INT(lyricmsg::TEXT_CAP - 1, (int)strlen(out.text));
}
void test_empty_name_uses_pokemon() {
    char b[32];
    lyricmsg::fill("{P}!", "", b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("POK\xC3\xA9" "MON!", b);
    lyricmsg::fill("{P}!", nullptr, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("POK\xC3\xA9" "MON!", b);
}
void test_all_messages_fit_two_lines() {
    const char* const* pools[] = {lyricmsg::SEARCH, lyricmsg::RETRY, lyricmsg::FAIL,
                                  lyricmsg::INSTRUMENTAL, lyricmsg::FOUND};
    const int sizes[] = {lyricmsg::SEARCH_N, lyricmsg::RETRY_N, lyricmsg::FAIL_N,
                         lyricmsg::INSTRUMENTAL_N, lyricmsg::FOUND_N};
    char b[lyricmsg::TEXT_CAP];
    for (int p = 0; p < 5; ++p)
        for (int i = 0; i < sizes[p]; ++i) {
            lyricmsg::fill(pools[p][i], "Crabominable", b, sizeof(b));
            TEST_ASSERT_TRUE_MESSAGE(strlen(b) <= 72, b);
        }
    lyricmsg::fill(lyricmsg::IDLE, "Crabominable", b, sizeof(b));
    TEST_ASSERT_TRUE(strlen(b) <= 72);
}
void test_searching_uses_search_pool_with_notes() {
    lyricmsg::compose(base(Status::Searching), out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::SEARCH, lyricmsg::SEARCH_N, out.text, "Pikachu"));
    TEST_ASSERT_TRUE(out.notes);
}
void test_retrying_uses_retry_pool() {
    lyricmsg::compose(base(Status::Retrying), out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::RETRY, lyricmsg::RETRY_N, out.text, "Pikachu"));
}
void test_pick_is_stable_per_song_and_varies_between_songs() {
    lyricmsg::In in = base(Status::Searching);
    char first[lyricmsg::TEXT_CAP];
    lyricmsg::compose(in, out);
    strcpy(first, out.text);
    in.statusAgeMs = 2500;
    in.posMs = 9000;
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING(first, out.text);   // same song: same line
    int different = 0;
    for (uint32_t s = 1; s <= 20; ++s) {
        in.seed = s;
        lyricmsg::compose(in, out);
        if (strcmp(out.text, first) != 0) ++different;
    }
    TEST_ASSERT_TRUE(different >= 5);
}
void test_synced_intro_shows_found_then_lyric_line() {
    lyricmsg::In in = base(Status::Synced);
    in.firstLineMs = 12000;
    in.posMs = 5000;
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::FOUND, lyricmsg::FOUND_N, out.text, "Pikachu"));
    TEST_ASSERT_TRUE(out.notes);
    in.posMs = 12000;
    in.line = "first verse";
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING("first verse", out.text);
    TEST_ASSERT_TRUE(out.notes);
}
void test_plain_has_no_notes() {
    lyricmsg::In in = base(Status::Plain);
    in.firstLineMs = 20000;
    in.posMs = 1000;
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::FOUND, lyricmsg::FOUND_N, out.text, "Pikachu"));
    TEST_ASSERT_FALSE(out.notes);
    in.posMs = 25000;
    in.line = "words";
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING("words", out.text);
    TEST_ASSERT_FALSE(out.notes);
}
void test_none_shows_fail_then_idle() {
    lyricmsg::In in = base(Status::None);
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::FAIL, lyricmsg::FAIL_N, out.text, "Pikachu"));
    in.statusAgeMs = lyricmsg::FAIL_SHOW_MS - 1;
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::FAIL, lyricmsg::FAIL_N, out.text, "Pikachu"));
    in.statusAgeMs = lyricmsg::FAIL_SHOW_MS;
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING("PIKACHU is enjoying the music", out.text);
    TEST_ASSERT_TRUE(out.notes);
}
void test_instrumental_shows_its_line_then_idle() {
    lyricmsg::In in = base(Status::Instrumental);
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::INSTRUMENTAL, lyricmsg::INSTRUMENTAL_N, out.text, "Pikachu"));
    in.statusAgeMs = lyricmsg::FAIL_SHOW_MS;
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING("PIKACHU is enjoying the music", out.text);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pools_have_spec_sizes);
    RUN_TEST(test_fill_replaces_every_P_with_capitals);
    RUN_TEST(test_fill_keeps_accents);
    RUN_TEST(test_fill_never_cuts_a_utf8_character);
    RUN_TEST(test_fill_long_name_stays_in_bounds);
    RUN_TEST(test_empty_name_uses_pokemon);
    RUN_TEST(test_all_messages_fit_two_lines);
    RUN_TEST(test_searching_uses_search_pool_with_notes);
    RUN_TEST(test_retrying_uses_retry_pool);
    RUN_TEST(test_pick_is_stable_per_song_and_varies_between_songs);
    RUN_TEST(test_synced_intro_shows_found_then_lyric_line);
    RUN_TEST(test_plain_has_no_notes);
    RUN_TEST(test_none_shows_fail_then_idle);
    RUN_TEST(test_instrumental_shows_its_line_then_idle);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lyricmsg\test_lyricmsg.cpp`
Expected: `COMPILE FAILED` — `lyricmsg.h: No such file or directory`.

- [ ] **Step 3: Implement.** Create `src/ui/lyricmsg.h` (Write tool):

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "../util/lyricstatus.h"
// Dialogue-box text for the lyrics status: Pokemon battle-text style messages while searching
// or retrying, in the intro, and when there are no lyrics, so the box is never empty.
// Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md. PURE, host-tested.
namespace lyricmsg {
constexpr uint32_t FAIL_SHOW_MS = 3000;   // FAIL / INSTRUMENTAL line, then the IDLE line
constexpr size_t TEXT_CAP = 160;
struct In {
    lyricstatus::Status status;
    uint32_t statusAgeMs;   // since the UI first saw this status for the track
    uint32_t posMs;         // track position
    uint32_t firstLineMs;   // time of the first lyric line (Synced / Plain)
    const char* line;       // current lyric line (Synced / Plain), may be ""
    const char* pokeName;   // as stored; written in capitals
    uint32_t seed;          // track generation: one stable pick per song
};
struct Out {
    char text[TEXT_CAP];
    bool notes;             // draw the note icons (false for approximate plain timing)
};
void compose(const In& in, Out& out);
// Copies tmpl into out (n bytes incl. NUL), replacing each "{P}" with the name in capitals
// ("POKéMON" if empty). Never cuts a UTF-8 character.
void fill(const char* tmpl, const char* name, char* out, size_t n);

constexpr int SEARCH_N = 10, RETRY_N = 3, FAIL_N = 4, INSTRUMENTAL_N = 3, FOUND_N = 2;
extern const char* const SEARCH[SEARCH_N];
extern const char* const RETRY[RETRY_N];
extern const char* const FAIL[FAIL_N];
extern const char* const INSTRUMENTAL[INSTRUMENTAL_N];
extern const char* const FOUND[FOUND_N];
extern const char* const IDLE;
}
```

Create `src/ui/lyricmsg.cpp` (Write tool; the `\xC3\xA9` escapes are split from the next letter so they don't swallow it as a hex digit):

```cpp
#include "lyricmsg.h"
#include <cstring>

namespace lyricmsg {

// Real move and item names; {P} = the Pokemon on screen.
const char* const SEARCH[SEARCH_N] = {
    "{P} used SING! Searching for the verses...",
    "{P} is fetching this piece of art's verses...",
    "{P} used FORESIGHT! Reading the song...",
    "{P} used ECHOED VOICE! Listening for words...",
    "{P} used MIMIC! Learning the lyrics...",
    "{P} used ODOR SLEUTH! Sniffing out verses...",
    "A wild LYRIC appeared? {P} is chasing it...",
    "Accessed BILL's PC... Opening the Lyrics Box.",
    "{P} checked the POK\xC3\xA9" "DEX for these verses...",
    "{P} is listening closely...",
};
const char* const RETRY[RETRY_N] = {
    "The verses fled! {P} is trying again...",
    "LRCLIB is fast asleep... {P} used WAKE-UP SLAP!",
    "It's not very effective... trying again!",
};
const char* const FAIL[FAIL_N] = {
    "But it failed!",
    "OAK: There's a time and place for everything! But not now.",
    "{P}'s search missed!",
    "The verses got away!",
};
const char* const INSTRUMENTAL[INSTRUMENTAL_N] = {
    "No words here! {P} is humming along...",
    "{P} used TEETER DANCE! It's instrumental!",
    "{P} used GRASSWHISTLE! Nothing to sing...",
};
const char* const FOUND[FOUND_N] = {
    "It's super effective! Lyrics found!",
    "Gotcha! The verses were caught!",
};
const char* const IDLE = "{P} is enjoying the music";

static const char* const DEFAULT_NAME = "POK\xC3\xA9" "MON";

static size_t utf8Len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;   // stray continuation byte: copy it alone
}

// Appends len bytes of s, whole UTF-8 characters only; false once out is full.
static bool append(char* out, size_t cap, size_t& o, const char* s, size_t len, bool upper) {
    size_t i = 0;
    while (i < len) {
        size_t k = utf8Len((unsigned char)s[i]);
        if (i + k > len) k = len - i;
        if (o + k > cap - 1) return false;
        for (size_t j = 0; j < k; ++j) {
            char c = s[i + j];
            if (upper && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            out[o++] = c;
        }
        i += k;
    }
    return true;
}

void fill(const char* tmpl, const char* name, char* out, size_t n) {
    if (n == 0) return;
    const char* nm = (name && name[0]) ? name : DEFAULT_NAME;
    size_t o = 0;
    for (const char* p = tmpl; p && *p;) {
        const char* hit = strstr(p, "{P}");
        size_t lit = hit ? (size_t)(hit - p) : strlen(p);
        if (!append(out, n, o, p, lit, false) || !hit) break;
        if (!append(out, n, o, nm, strlen(nm), true)) break;
        p = hit + 3;
    }
    out[o] = '\0';
}

// Stable pseudo-random index per song; salt keeps the pools from moving in lockstep.
static int pick(uint32_t seed, int n, uint32_t salt) {
    uint32_t h = (seed + salt * 0x9E3779B9u) * 2654435761u;
    return (int)((h >> 16) % (uint32_t)n);
}

void compose(const In& in, Out& out) {
    using lyricstatus::Status;
    out.notes = true;
    const char* tmpl = IDLE;
    switch (in.status) {
        case Status::Searching:
            tmpl = SEARCH[pick(in.seed, SEARCH_N, 1)];
            break;
        case Status::Retrying:
            tmpl = RETRY[pick(in.seed, RETRY_N, 2)];
            break;
        case Status::Synced:
        case Status::Plain:
            out.notes = in.status == Status::Synced;
            if (in.posMs < in.firstLineMs) {
                tmpl = FOUND[pick(in.seed, FOUND_N, 3)];
                break;
            }
            strncpy(out.text, in.line ? in.line : "", TEXT_CAP - 1);
            out.text[TEXT_CAP - 1] = '\0';
            return;
        case Status::Instrumental:
            tmpl = in.statusAgeMs < FAIL_SHOW_MS ? INSTRUMENTAL[pick(in.seed, INSTRUMENTAL_N, 4)] : IDLE;
            break;
        case Status::None:
            tmpl = in.statusAgeMs < FAIL_SHOW_MS ? FAIL[pick(in.seed, FAIL_N, 5)] : IDLE;
            break;
    }
    fill(tmpl, in.pokeName, out.text, TEXT_CAP);
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 test\test_lyricmsg\test_lyricmsg.cpp src\ui\lyricmsg.cpp`
Expected: `14 Tests 0 Failures 0 Ignored` / `OK`

- [ ] **Step 5: Commit**

```bash
git add src/ui/lyricmsg.h src/ui/lyricmsg.cpp test/test_lyricmsg/test_lyricmsg.cpp
git commit -m "feat(ui): Pokemon-style dialogue messages for the lyrics status" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Network side: get → search, results, retries, status (device)

Device code (Arduino/FreeRTOS): verified by building, then on the board in Task 8. The decisions it makes are the ones tested in Tasks 1–4.

**Files:**
- Modify: `src/lyrics/lrclib.h`, `src/lyrics/lrclib.cpp`, `src/core/shared.h`, `src/core/shared.cpp`, `src/core/nettask.cpp`

**Interfaces:**
- Consumes: Tasks 1–4 (`lrcstream::Extractor/Mode/Kind`, `lyricbuf::spreadPlain`, `lyricstatus::Result/Status/statusFor`, `netplan::LyricsRetry`, `netplan::next(w, lyricsReady)`).
- Produces (used by Task 7):
  - `lyricstatus::Result lyricsvc::fetchInto(const AppState& st, lyricbuf::Lyrics& out, int attempt);`
  - `void shared::postLyricsStatus(uint32_t gen, lyricstatus::Status s);`
  - `struct shared::LyricView { lyricstatus::Status status; uint32_t firstLineMs; char line[160]; };`
  - `void shared::lyricView(uint32_t gen, uint32_t posMs, LyricView& out);`
  - (`shared::lyricLine` stays until Task 7 removes it.)

- [ ] **Step 1: `src/lyrics/lrclib.h`** — replace the `fetchInto` comment and declaration with:

```cpp
// Fetches lyrics for the current track from LRCLIB into `out`: /api/get (exact match), then
// /api/search on a 404. Synced beats plain (plain lines get spread times); instrumental wins.
// `attempt` (1-based) is only for the log line. Never throws; TempError means "retry later".
lyricstatus::Result fetchInto(const AppState& st, lyricbuf::Lyrics& out, int attempt);
```

and add `#include "../util/lyricstatus.h"` after the existing includes.

- [ ] **Step 2: `src/lyrics/lrclib.cpp`** — replace the whole `fetchInto` function with:

```cpp
// One request into `out`. *rc: HTTP status (negative: HTTPClient connect/TLS/timeout error).
// *stalled: the body stopped arriving before the reader was done.
static lrcstream::Kind request(const String& url, lrcstream::Mode mode, uint32_t durationMs,
                               lyricbuf::Lyrics& out, int* rc, bool* stalled) {
    *stalled = false;
    lyricbuf::reset(out);
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient https;
    https.useHTTP10(true);          // plain (non-chunked) body: scan straight from the stream
    https.setTimeout(8000);
    if (!https.begin(client, url)) { *rc = -1; return lrcstream::Kind::None; }
    https.addHeader("User-Agent", "PokeDeck/1.0 (ESP32)");
    *rc = https.GET();
    if (*rc != 200) { https.end(); return lrcstream::Kind::None; }
    // Stream straight into the arena (no JSON document: ArduinoJson grew the string by
    // doubling, up to 16 KB contiguous while TLS was open).
    lrcstream::Extractor ex(out, mode, durationMs);
    WiFiClient* s = https.getStreamPtr();
    uint8_t chunk[128];
    uint32_t last = millis();
    while (!ex.done() && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (millis() - last > 8000) { *stalled = true; break; }
            delay(1);
            continue;
        }
        int r = s->read(chunk, a < (int)sizeof(chunk) ? a : (int)sizeof(chunk));
        for (int i = 0; i < r && !ex.done(); ++i) ex.feed((char)chunk[i]);
        last = millis();
    }
    https.end();
    ex.finish();
    return ex.kind();
}

// 5xx and 429 are the server's problem; negative codes are connect/TLS/timeout failures.
static bool temporary(int rc) { return rc < 0 || rc == 429 || rc >= 500; }

lyricstatus::Result fetchInto(const AppState& st, lyricbuf::Lyrics& out, int attempt) {
    using lyricstatus::Result;
    lyricbuf::reset(out);
    if (!st.trackName[0]) return Result::NotFound;
    String q = "track_name=" + urlEncode(st.trackName) + "&artist_name=" + urlEncode(st.artist);
    int rc = 0;
    bool stalled = false;
    const char* via = "get";
    lrcstream::Kind k = request("https://lrclib.net/api/get?" + q + "&album_name=" + urlEncode(st.album) +
                                    "&duration=" + String(st.durationMs / 1000),
                                lrcstream::Mode::Get, st.durationMs, out, &rc, &stalled);
    if (rc == 404) {   // no exact match: fuzzy search, entry chosen by duration
        via = "search";
        k = request("https://lrclib.net/api/search?" + q, lrcstream::Mode::Search, st.durationMs,
                    out, &rc, &stalled);
    }
    Result r;
    if (temporary(rc) || stalled) r = Result::TempError;
    else if (rc != 200) r = Result::NotFound;
    else if (k == lrcstream::Kind::Synced) r = Result::Synced;
    else if (k == lrcstream::Kind::Plain) r = Result::Plain;
    else if (k == lrcstream::Kind::Instrumental) r = Result::Instrumental;
    else r = Result::NotFound;
    if (r == Result::Plain) lyricbuf::spreadPlain(out, st.durationMs);

    char what[40];
    switch (r) {
        case Result::Synced:       snprintf(what, sizeof(what), "synced %d lines", out.n); break;
        case Result::Plain:        snprintf(what, sizeof(what), "plain %d lines", out.n); break;
        case Result::Instrumental: snprintf(what, sizeof(what), "instrumental"); break;
        case Result::NotFound:     snprintf(what, sizeof(what), "not found"); break;
        case Result::TempError:
            if (stalled) snprintf(what, sizeof(what), "temp error (stalled)");
            else snprintf(what, sizeof(what), "temp error rc=%d", rc);
            break;
    }
    Serial.printf("[lyrics] \"%s\" / %s: %s via %s (try %d)\n", st.trackName, st.artist, what, via, attempt);
    return r;
}
```

- [ ] **Step 3: `src/core/shared.h`** — add `#include "../util/lyricstatus.h"` after `#include "app_state.h"`, and after the `lyricLine` declaration add:

```cpp
// Lyrics status for the track (the UI sees Searching until the network task posts one).
// Post it after postLyrics(), so a Synced/Plain status never arrives before its lines.
void postLyricsStatus(uint32_t gen, lyricstatus::Status s);
// Everything the dialogue box needs, read under one lock.
struct LyricView {
    lyricstatus::Status status;
    uint32_t firstLineMs;   // time of the first line (0 if no valid lines)
    char line[160];         // current line for posMs ("" if none)
};
void lyricView(uint32_t gen, uint32_t posMs, LyricView& out);
```

- [ ] **Step 4: `src/core/shared.cpp`** — after `static uint32_t g_walkerGen = 0;` add

```cpp
static uint32_t g_statusGen = 0;   // gen g_status belongs to (0 = none)
static lyricstatus::Status g_status = lyricstatus::Status::Searching;
```

and after the `lyricLine` function add:

```cpp
void postLyricsStatus(uint32_t gen, lyricstatus::Status s) {
    Guard g;
    if (gen != g_state.trackGen) return;   // stale: track moved on
    g_statusGen = gen;
    g_status = s;
}

void lyricView(uint32_t gen, uint32_t posMs, LyricView& out) {
    Guard g;
    out.status = (gen != 0 && g_statusGen == gen) ? g_status : lyricstatus::Status::Searching;
    out.firstLineMs = 0;
    out.line[0] = '\0';
    if (gen == 0 || g_lyricsGen != gen) return;
    const lyricbuf::Lyrics& l = lyricsvc::arena();
    if (l.n > 0) out.firstLineMs = l.lines[0].tMs;
    strncpy(out.line, lyricbuf::lineText(l, lyricbuf::currentIndex(l, posMs)), sizeof(out.line) - 1);
    out.line[sizeof(out.line) - 1] = '\0';
}
```

- [ ] **Step 5: `src/core/nettask.cpp`**:

After `static netplan::Health s_health;` add

```cpp
static netplan::LyricsRetry s_lyricsRetry;   // 5 s / 20 s retries after LRCLIB hiccups
```

In `onTrackChange()`, after `s_work = netplan::freshWork(prefetched);` add

```cpp
    s_lyricsRetry.reset();               // UI shows "searching" until the first result
```

Change `static void doStep(netplan::Step step) {` to

```cpp
// Runs one step; false when the step must run again later (lyrics retry).
static bool doStep(netplan::Step step) {
```

Replace the whole `case netplan::Step::Lyrics: { ... break; }` block with

```cpp
        case netplan::Step::Lyrics: {
            tick();
            shared::lyricsInvalidate();       // UI stops reading the arena before we overwrite it
            lyricstatus::Result r =
                lyricsvc::fetchInto(s_st, lyricsvc::arena(), s_lyricsRetry.attempts() + 1);
            bool final = s_lyricsRetry.onResult(r, millis());
            if (r == lyricstatus::Result::Synced || r == lyricstatus::Result::Plain)
                shared::postLyrics(gen);      // lines first, then the status that points at them
            shared::postLyricsStatus(gen, lyricstatus::statusFor(r, final));
            tock(final ? "lyrics" : "lyrics (retry later)");
            mem::log("lyrics");
            if (!final) return false;
            break;
        }
```

Replace the end of `doStep` (`    netplan::done(s_work, step);\n}`) with

```cpp
    netplan::done(s_work, step);
    return true;
}
```

In `run()`, replace `netplan::Step step = netplan::next(s_work);` with

```cpp
            netplan::Step step = netplan::next(s_work, s_lyricsRetry.ready(now));
```

- [ ] **Step 6: Build**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]` (RAM within ~0.1% of the previous 34.5%).

- [ ] **Step 7: Host suites still green**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: `suites pass=23 fail=0`

- [ ] **Step 8: Commit**

```bash
git add src/lyrics/lrclib.h src/lyrics/lrclib.cpp src/core/shared.h src/core/shared.cpp src/core/nettask.cpp
git commit -m "feat(lyrics): LRCLIB search fallback, plain lyrics, retries and per-track status" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: UI side: draw the composed text (device)

**Files:**
- Modify: `src/ui/screen_now.h`, `src/ui/screen_now.cpp`, `src/main.cpp`, `src/core/shared.h`, `src/core/shared.cpp`, `src/lyrics/lrclib.h`

**Interfaces:**
- Consumes: Task 5 `lyricmsg::In/Out/compose`; Task 6 `shared::LyricView`, `shared::lyricView`.
- Produces: `void ui::drawLyricArea(TFT_eSPI& t, const char* text, bool notes);`

- [ ] **Step 1: `src/ui/screen_now.h`** — replace the `drawLyricArea` declaration with

```cpp
// Dialogue-box text (a lyric line or a status message), redrawn only when it changes.
// notes: draw the note icons (off for plain lyrics, whose timing is approximate).
void drawLyricArea(TFT_eSPI& t, const char* text, bool notes);
```

- [ ] **Step 2: `src/ui/screen_now.cpp`** — in `drawLyricArea`, replace the signature and the change check:

```cpp
static bool g_lastNotes = true;

void drawLyricArea(TFT_eSPI& t, const char* text, bool notes) {
    const char* line = text ? text : "";
    if (notes == g_lastNotes && strncmp(g_lastLyric, line, sizeof(g_lastLyric)) == 0) return;
    g_lastNotes = notes;
    strncpy(g_lastLyric, line, sizeof(g_lastLyric) - 1);
    g_lastLyric[sizeof(g_lastLyric) - 1] = '\0';
```

(the `static bool g_lastNotes` line goes right after `void resetLyricArea() { ... }`), and make the two note icons conditional:

```cpp
    if (notes) {
        drawIcon(t, icons::Icon::Note, ix + 2, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
        drawIcon(t, icons::Icon::Note, ix + iw - 14, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    }
```

- [ ] **Step 3: `src/main.cpp`** — add `#include "ui/lyricmsg.h"` after `#include "ui/pokeball.h"`, and add this function after `pushArtIfValid`:

```cpp
// Dialogue box for the track on screen: the lyric line, or a Pokemon-style status message
// (searching / retrying / intro / no lyrics). The status age drives the FAIL -> IDLE switch.
static void drawDialogue(const AppState& view, uint32_t gen) {
    static lyricstatus::Status lastStatus = lyricstatus::Status::Searching;
    static uint32_t lastGen = 0, since = 0;
    static shared::LyricView lv;      // static: keep the loop stack small
    static lyricmsg::Out out;
    shared::lyricView(gen, view.progressMs, lv);
    uint32_t now = millis();
    if (gen != lastGen || lv.status != lastStatus) {
        lastGen = gen;
        lastStatus = lv.status;
        since = now;
    }
    lyricmsg::In in{lv.status, now - since, view.progressMs, lv.firstLineMs, lv.line,
                    view.pokeName, gen};
    lyricmsg::compose(in, out);
    ui::drawLyricArea(tft, out.text, out.notes);
}
```

Replace both occurrences of

```cpp
        char line[160];
        shared::lyricLine(shownGen, view.progressMs, line, sizeof(line));
        ui::drawLyricArea(tft, line);
```

(the second one is indented one level deeper) with

```cpp
        drawDialogue(view, shownGen);
```

- [ ] **Step 4: Remove `shared::lyricLine`** (no callers left): delete its declaration and comment from `src/core/shared.h` and its definition from `src/core/shared.cpp`; in `src/lyrics/lrclib.h` change the comment `shared::lyricLine() — never directly.` to `shared::lyricView() — never directly.`

Run: `git grep -n "lyricLine" -- src`
Expected: no output.

- [ ] **Step 5: Build**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev`
Expected: `[SUCCESS]`

- [ ] **Step 6: Commit**

```bash
git add src/ui/screen_now.h src/ui/screen_now.cpp src/main.cpp src/core/shared.h src/core/shared.cpp src/lyrics/lrclib.h
git commit -m "feat(ui): dialogue box shows lyrics status messages, no notes for plain lyrics" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: On-device verification and docs

**Files:**
- Modify: `README.md` (history item 13), `docs/BACKLOG.md`

- [ ] **Step 1: Flash**

Run: `powershell -ExecutionPolicy Bypass -File .devtools\pio.ps1 run -e esp32dev -t upload --upload-port COM11`
Expected: `[SUCCESS]`

- [ ] **Step 2: Capture a 10-minute session** while playing and skipping songs every 30–60 s; include "MIDNIGHT DOWN TOWN" (S. Kiyotaka & Omega Tribe) and one instrumental track.

Run: `& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe" tools\capture_serial.py COM11 600 <scratchpad>\lyrics-session.log`
Then: `... python.exe tools\analyze_session.py <scratchpad>\lyrics-session.log` and `Select-String -Path <log> -Pattern "\[lyrics\]"`
Expected:
- one `[lyrics] "<title>" / <artist>: ... via get|search (try N)` line per attempt;
- "MIDNIGHT DOWN TOWN" → `plain 2x lines`;
- any `temp error` followed by a later `(try 2)` or `(try 3)` line for the same title;
- 0 watchdog, 0 restarts; byte RAM free/largest in the same range as before (66–68 KB / 34.8 KB).

- [ ] **Step 3: Check on screen** (ask the user): a SEARCH line with the Pokemon's name right after a skip; the FOUND line during an intro; plain lyrics without note icons; `But it failed!`-style line then `<NAME> is enjoying the music` on a song without lyrics.

- [ ] **Step 4: Docs.** Append history item 13 to `README.md` "Design & performance history" (after item 12):

```markdown
13. **Missing lyrics.** Lyrics went missing even when Spotify had them: LRCLIB had only plain
    (untimed) text for some songs, returned 503s now and then (one try per song), and its exact
    lookup misses slightly different titles/durations. The reader now also keeps plain lyrics
    (spread over 10–90% of the song, drawn without note icons) and the instrumental flag, falls
    back to `/api/search` (entry chosen by duration ±3 s, 48 KB cap), and retries temporary
    errors after 5 s and 20 s. The dialogue box is never empty: Pokemon battle-text messages
    while searching ("PIKACHU used SING! Searching for the verses..."), retrying, in the intro,
    and "But it failed!" then "PIKACHU is enjoying the music" when there are none
    (`ui/lyricmsg`). Every attempt logs `[lyrics] "<title>" / <artist>: <result> via get|search (try N)`.
```

In `docs/BACKLOG.md`, under the "Bigger status panel" idea, replace the sentence

```
Needs the lyrics pipeline to report a final "no lyrics" state (planned with the lyrics-reliability work).
```

with

```
The lyrics status (lyricstatus::Status::None / Instrumental) now reports that final state.
```

- [ ] **Step 5: Commit**

```bash
git add README.md docs/BACKLOG.md
git commit -m "docs: record lyrics reliability and dialogue messages in the history" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
