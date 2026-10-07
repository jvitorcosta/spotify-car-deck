#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/lrcstream.h"

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
void test_null_or_missing_key_is_not_found() {
    lrcstream::Extractor a(L);
    TEST_ASSERT_EQUAL_INT(0, feedAll(a, "{\"plainLyrics\":\"x\",\"syncedLyrics\":null}"));
    TEST_ASSERT_FALSE(a.found());
    lrcstream::Extractor b(L);
    TEST_ASSERT_EQUAL_INT(0, feedAll(b, "{\"plainLyrics\":\"[00:01.00]no\"}"));
    TEST_ASSERT_FALSE(b.found());
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
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_extracts_synced_lines);
    RUN_TEST(test_unescapes_quotes_backslash_and_unicode);
    RUN_TEST(test_raw_utf8_passes_through);
    RUN_TEST(test_null_or_missing_key_is_not_found);
    RUN_TEST(test_whitespace_around_colon_and_done_after_string);
    RUN_TEST(test_arena_full_sets_truncated);
    RUN_TEST(test_overlong_line_is_cut_not_overflowed);
    return UNITY_END();
}
