#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/lyricbuf.h"

static lyricbuf::Lyrics L;   // ~5.6 KB: keep it off the stack
void setUp() {}
void tearDown() {}

void test_parses_timestamped_lines() {
    TEST_ASSERT_EQUAL_INT(2, lyricbuf::parse("[00:01.00]hello\n[00:03.50]world\n", L));
    TEST_ASSERT_EQUAL_UINT32(1000, L.lines[0].tMs);
    TEST_ASSERT_EQUAL_STRING("hello", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_UINT32(3500, L.lines[1].tMs);
    TEST_ASSERT_EQUAL_STRING("world", lyricbuf::lineText(L, 1));
}
void test_skips_malformed() {
    TEST_ASSERT_EQUAL_INT(1, lyricbuf::parse("garbage\n[00:02.00]ok\n[bad]x\n", L));
    TEST_ASSERT_EQUAL_STRING("ok", lyricbuf::lineText(L, 0));
}
void test_crlf_and_empty_text() {
    TEST_ASSERT_EQUAL_INT(2, lyricbuf::parse("[00:01.00]a\r\n[00:02.00]\r\n", L));
    TEST_ASSERT_EQUAL_STRING("a", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_STRING("", lyricbuf::lineText(L, 1));
}
void test_sorts_by_time() {
    lyricbuf::parse("[00:05.00]c\n[00:01.00]a\n[00:03.00]b\n", L);
    TEST_ASSERT_EQUAL_STRING("a", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_STRING("b", lyricbuf::lineText(L, 1));
    TEST_ASSERT_EQUAL_STRING("c", lyricbuf::lineText(L, 2));
}
void test_current_index() {
    lyricbuf::parse("[00:01.00]a\n[00:03.00]b\n[00:05.00]c\n", L);
    TEST_ASSERT_EQUAL_INT(-1, lyricbuf::currentIndex(L, 500));
    TEST_ASSERT_EQUAL_INT(0, lyricbuf::currentIndex(L, 2000));
    TEST_ASSERT_EQUAL_INT(1, lyricbuf::currentIndex(L, 3000));
    TEST_ASSERT_EQUAL_INT(2, lyricbuf::currentIndex(L, 9000));
}
void test_text_overflow_drops_the_rest() {
    std::string s;
    for (int i = 0; i < 200; ++i) {   // 200 lines x 50 chars > 4096 B of text
        char tag[16];
        snprintf(tag, sizeof(tag), "[%02d:%02d.00]", i / 60, i % 60);
        s += tag + std::string(50, 'x') + "\n";
    }
    int n = lyricbuf::parse(s.c_str(), L);
    TEST_ASSERT_TRUE(n > 50 && n < 200);   // ~80 lines of 51 B fit in 4096
    TEST_ASSERT_EQUAL_INT(50, (int)strlen(lyricbuf::lineText(L, n - 1)));   // last kept line intact
}
void test_null_and_out_of_range() {
    TEST_ASSERT_EQUAL_INT(0, lyricbuf::parse(nullptr, L));
    TEST_ASSERT_EQUAL_STRING("", lyricbuf::lineText(L, 0));
    TEST_ASSERT_EQUAL_INT(-1, lyricbuf::currentIndex(L, 1000));
}
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
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_timestamped_lines);
    RUN_TEST(test_skips_malformed);
    RUN_TEST(test_crlf_and_empty_text);
    RUN_TEST(test_sorts_by_time);
    RUN_TEST(test_current_index);
    RUN_TEST(test_text_overflow_drops_the_rest);
    RUN_TEST(test_null_and_out_of_range);
    RUN_TEST(test_has_tag);
    RUN_TEST(test_plain_lines_keep_order_and_skip_blanks);
    RUN_TEST(test_plain_overflow_still_counts_lines);
    RUN_TEST(test_spread_plain_over_10_to_90_percent);
    RUN_TEST(test_spread_plain_keeps_share_of_lines_that_did_not_fit);
    RUN_TEST(test_spread_plain_without_duration_is_4s_apart);
    RUN_TEST(test_reset_clears_plain_total);
    return UNITY_END();
}
