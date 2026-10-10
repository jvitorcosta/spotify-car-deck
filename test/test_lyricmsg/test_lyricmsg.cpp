#include <unity.h>
#include <cstring>
#include <initializer_list>
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
static const char* idle() {                      // the IDLE line for base()'s Pikachu
    static char buf[lyricmsg::TEXT_CAP];
    lyricmsg::fill(lyricmsg::IDLE, "Pikachu", buf, sizeof(buf));
    return buf;
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
// Every message fits two lines, and none names LRCLIB: listeners know Spotify, not the
// lyrics backend.
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
            TEST_ASSERT_NULL_MESSAGE(strstr(pools[p][i], "LRCLIB"), pools[p][i]);
        }
    lyricmsg::fill(lyricmsg::IDLE, "Crabominable", b, sizeof(b));
    TEST_ASSERT_TRUE(strlen(b) <= 72);
    TEST_ASSERT_NULL(strstr(lyricmsg::IDLE, "LRCLIB"));
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
// After a skip the lyrics should take over at once: FOUND is a brief confirmation only, then
// the intro shows the first (upcoming) line instead of a message (user feedback, 2026-10-08).
void test_synced_intro_flashes_found_then_shows_first_line() {
    lyricmsg::In in = base(Status::Synced);
    in.firstLineMs = 12000;
    in.posMs = 500;
    in.line = "first verse";                     // lyricView gives the upcoming line in the intro
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::FOUND, lyricmsg::FOUND_N, out.text, "Pikachu"));
    TEST_ASSERT_TRUE(out.notes);
    in.statusAgeMs = lyricmsg::FOUND_SHOW_MS;     // still in the intro, but the flash is over
    in.posMs = 2000;
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING("first verse", out.text);
    TEST_ASSERT_TRUE(out.notes);
}
void test_lyrics_arriving_mid_song_skip_found() {
    lyricmsg::In in = base(Status::Synced);
    in.firstLineMs = 12000;
    in.posMs = 40000;                            // past the intro: straight to the current line
    in.line = "chorus";
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING("chorus", out.text);
}
// Final review (re-graded Important): empty timed lines ("[01:23.45]" breaks, or an empty first
// line in the intro) must not blank the box: show the IDLE line instead.
void test_empty_lyric_line_shows_idle_not_blank() {
    lyricmsg::In in = base(Status::Synced);
    in.firstLineMs = 0;
    in.posMs = 83000;
    in.line = "";
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING(idle(), out.text);
    TEST_ASSERT_TRUE(out.notes);
    in.status = Status::Plain;
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING(idle(), out.text);
    TEST_ASSERT_FALSE(out.notes);
}
// The note icons dance only while a real timed lyric line is shown.
void test_notes_dance_only_on_timed_lyric_lines() {
    lyricmsg::In in = base(Status::Synced);
    in.firstLineMs = 1000;
    in.posMs = 5000;
    in.statusAgeMs = 9000;
    in.line = "a verse";
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(out.dance);
    in.line = "";                                 // empty timed line -> IDLE: still
    lyricmsg::compose(in, out);
    TEST_ASSERT_FALSE(out.dance);
    in.line = "a verse";
    in.posMs = 500;
    in.statusAgeMs = 0;                           // FOUND flash: still
    lyricmsg::compose(in, out);
    TEST_ASSERT_FALSE(out.dance);
    in.status = Status::Plain;                    // plain: no notes, no dance
    in.posMs = 5000;
    in.statusAgeMs = 9000;
    lyricmsg::compose(in, out);
    TEST_ASSERT_FALSE(out.dance);
    for (Status s : {Status::Searching, Status::Retrying, Status::Instrumental, Status::None}) {
        in.status = s;
        lyricmsg::compose(in, out);
        TEST_ASSERT_FALSE(out.dance);
    }
}
void test_plain_has_no_notes() {
    lyricmsg::In in = base(Status::Plain);
    in.firstLineMs = 20000;
    in.posMs = 1000;
    in.line = "words";
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::FOUND, lyricmsg::FOUND_N, out.text, "Pikachu"));
    TEST_ASSERT_FALSE(out.notes);
    in.statusAgeMs = lyricmsg::FOUND_SHOW_MS;
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
    TEST_ASSERT_EQUAL_STRING(idle(), out.text);
    TEST_ASSERT_TRUE(out.notes);
}
void test_instrumental_shows_its_line_then_idle() {
    lyricmsg::In in = base(Status::Instrumental);
    lyricmsg::compose(in, out);
    TEST_ASSERT_TRUE(inPool(lyricmsg::INSTRUMENTAL, lyricmsg::INSTRUMENTAL_N, out.text, "Pikachu"));
    in.statusAgeMs = lyricmsg::FAIL_SHOW_MS;
    lyricmsg::compose(in, out);
    TEST_ASSERT_EQUAL_STRING(idle(), out.text);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fill_replaces_every_P_with_capitals);
    RUN_TEST(test_fill_keeps_accents);
    RUN_TEST(test_fill_never_cuts_a_utf8_character);
    RUN_TEST(test_fill_long_name_stays_in_bounds);
    RUN_TEST(test_empty_name_uses_pokemon);
    RUN_TEST(test_all_messages_fit_two_lines);
    RUN_TEST(test_searching_uses_search_pool_with_notes);
    RUN_TEST(test_retrying_uses_retry_pool);
    RUN_TEST(test_pick_is_stable_per_song_and_varies_between_songs);
    RUN_TEST(test_synced_intro_flashes_found_then_shows_first_line);
    RUN_TEST(test_lyrics_arriving_mid_song_skip_found);
    RUN_TEST(test_empty_lyric_line_shows_idle_not_blank);
    RUN_TEST(test_notes_dance_only_on_timed_lyric_lines);
    RUN_TEST(test_plain_has_no_notes);
    RUN_TEST(test_none_shows_fail_then_idle);
    RUN_TEST(test_instrumental_shows_its_line_then_idle);
    return UNITY_END();
}
