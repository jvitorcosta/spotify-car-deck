#include <unity.h>
#include <cstring>
#include "../../src/util/glyphrun.h"

using glyphrun::Item;
using glyphrun::Kind;
void setUp() {}
void tearDown() {}

// Fake fonts: ASCII 6 px; kana/hanzi 16 px; half-width katakana (U+FF61..FF9F) 8 px.
static int aw(char, void*) { return 6; }
static int ww(uint32_t cp, void*) {
    if (cp >= 0xFF61 && cp <= 0xFF9F) return 8;
    if ((cp >= 0x3000 && cp <= 0x30FF) || (cp >= 0x4E00 && cp <= 0x9FFF)) return 16;
    return 0;
}
static Item buf[128];

void test_decode_mixed_ascii_accent_cjk() {
    // "Aç あ愛" : A, c(cedilla), space, あ, 愛
    size_t n = glyphrun::decode("A\xC3\xA7 \xE3\x81\x82\xE6\x84\x9B", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(5, (int)n);
    TEST_ASSERT_EQUAL_INT((int)Kind::Ascii, (int)buf[1].kind);
    TEST_ASSERT_EQUAL_INT('c', buf[1].ch);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Cedilla, (int)buf[1].mark);
    TEST_ASSERT_EQUAL_INT((int)Kind::Wide, (int)buf[3].kind);
    TEST_ASSERT_EQUAL_HEX32(0x3042, buf[3].cp);
    TEST_ASSERT_EQUAL_HEX32(0x611B, buf[4].cp);
    TEST_ASSERT_EQUAL_INT(6 * 3 + 16 * 2, glyphrun::width(buf, n));
}
void test_decode_drops_unsupported() {
    // emoji (4-byte), Hangul (not covered), stray continuation byte
    size_t n = glyphrun::decode("a\xF0\x9F\x98\x80" "b\xEA\xB0\x80" "c\x80" "d", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(4, (int)n);
    TEST_ASSERT_EQUAL_INT('a', buf[0].ch);
    TEST_ASSERT_EQUAL_INT('d', buf[3].ch);
}
void test_decode_half_width_and_upper() {
    size_t n = glyphrun::decode("ab\xEF\xBD\xB1", buf, 128, aw, ww, nullptr, true);   // "abｱ"
    TEST_ASSERT_EQUAL_INT(3, (int)n);
    TEST_ASSERT_EQUAL_INT('A', buf[0].ch);
    TEST_ASSERT_EQUAL_INT(8, buf[2].w);
}
void test_decode_respects_capacity() {
    size_t n = glyphrun::decode("abcdef", buf, 4, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(4, (int)n);
}
void test_fit_appends_ellipsis() {
    size_t n = glyphrun::decode("\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82", buf, 128, aw, ww, nullptr);   // 4 x 16 px
    n = glyphrun::fit(buf, n, 128, 50, aw, nullptr);   // 50 px: 1 kana (16) + "..." (18) = 34; 2 kana = 50
    TEST_ASSERT_TRUE(glyphrun::width(buf, n) <= 50);
    TEST_ASSERT_EQUAL_INT('.', buf[n - 1].ch);
    TEST_ASSERT_EQUAL_INT((int)Kind::Wide, (int)buf[0].kind);
}
void test_fit_short_is_untouched() {
    size_t n = glyphrun::decode("hi", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(2, (int)glyphrun::fit(buf, n, 128, 100, aw, nullptr));
}
void test_wrap_ascii_at_space() {
    size_t n = glyphrun::decode("aaaa bbbb cccc", buf, 128, aw, ww, nullptr);   // 6 px each
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 60, 18);   // 10 chars per line
    TEST_ASSERT_EQUAL_INT(9, (int)w.aEnd);                  // "aaaa bbbb"
    TEST_ASSERT_EQUAL_INT(10, (int)w.bStart);               // "cccc"
    TEST_ASSERT_EQUAL_INT(14, (int)w.bEnd);
    TEST_ASSERT_FALSE(w.bEllipsis);
}
void test_wrap_cjk_between_characters() {
    // 10 kana, no spaces; 64 px per line = 4 kana
    size_t n = glyphrun::decode("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A"
                                "\xE3\x81\x8B\xE3\x81\x8D\xE3\x81\x8F\xE3\x81\x91\xE3\x81\x93",
                                buf, 128, aw, ww, nullptr);
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 64, 18);
    TEST_ASSERT_EQUAL_INT(4, (int)w.aEnd);
    TEST_ASSERT_EQUAL_INT(4, (int)w.bStart);
    TEST_ASSERT_TRUE(w.bEllipsis);                           // 6 left, only fits 2 + "..."
    TEST_ASSERT_TRUE(16 * (int)(w.bEnd - w.bStart) + 18 <= 64);
}
void test_wrap_single_line() {
    size_t n = glyphrun::decode("short", buf, 128, aw, ww, nullptr);
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 60, 18);
    TEST_ASSERT_EQUAL_INT(5, (int)w.aEnd);
    TEST_ASSERT_EQUAL_INT((int)w.bStart, (int)w.bEnd);       // empty line b
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_mixed_ascii_accent_cjk);
    RUN_TEST(test_decode_drops_unsupported);
    RUN_TEST(test_decode_half_width_and_upper);
    RUN_TEST(test_decode_respects_capacity);
    RUN_TEST(test_fit_appends_ellipsis);
    RUN_TEST(test_fit_short_is_untouched);
    RUN_TEST(test_wrap_ascii_at_space);
    RUN_TEST(test_wrap_cjk_between_characters);
    RUN_TEST(test_wrap_single_line);
    return UNITY_END();
}
