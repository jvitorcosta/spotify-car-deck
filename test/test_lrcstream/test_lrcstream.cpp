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
// Final review Important #1: "" counts as absent, so it must not use up the plain slot.
void test_search_empty_plain_does_not_block_a_later_match() {
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    feedAll(ex, "[{\"duration\":200,\"plainLyrics\":\"\",\"syncedLyrics\":null},"
                "{\"duration\":200,\"plainLyrics\":\"\\n\\n\",\"syncedLyrics\":null},"
                "{\"duration\":201,\"plainLyrics\":\"real words\",\"syncedLyrics\":null}]");
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
    TEST_ASSERT_EQUAL_STRING("real words", lyricbuf::lineText(L, 0));
}
// Final review (re-graded Important): a complete answer must end the read; waiting for the
// server to close the connection turned complete answers into "stalled" temp errors.
void test_get_done_at_end_of_document() {
    lrcstream::Extractor ex(L);
    const char* json = "{\"plainLyrics\":\"only plain\",\"syncedLyrics\":null}";
    for (const char* p = json; *p; ++p) ex.feed(*p);
    TEST_ASSERT_TRUE(ex.done());
    ex.finish();
    TEST_ASSERT_EQUAL_INT((int)Kind::Plain, (int)ex.kind());
}
void test_search_done_at_end_of_array() {
    lrcstream::Extractor ex(L, Mode::Search, 200000);
    const char* json = "[{\"duration\":10,\"plainLyrics\":\"no\"}]";
    for (const char* p = json; *p; ++p) ex.feed(*p);
    TEST_ASSERT_TRUE(ex.done());
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
    RUN_TEST(test_search_empty_plain_does_not_block_a_later_match);
    RUN_TEST(test_get_done_at_end_of_document);
    RUN_TEST(test_search_done_at_end_of_array);
    return UNITY_END();
}
