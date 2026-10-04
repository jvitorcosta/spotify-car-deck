#include <unity.h>
#include "../../src/util/lrc.h"

void setUp() {}
void tearDown() {}

void test_parses_timestamped_lines() {
    auto v = lrc::parse("[00:01.00]hello\n[00:03.50]world\n");
    TEST_ASSERT_EQUAL_INT(2, (int)v.size());
    TEST_ASSERT_EQUAL_UINT32(1000, v[0].tMs);
    TEST_ASSERT_EQUAL_STRING("hello", v[0].text.c_str());
    TEST_ASSERT_EQUAL_UINT32(3500, v[1].tMs);
}
void test_skips_malformed() {
    auto v = lrc::parse("garbage\n[00:02.00]ok\n[bad]x\n");
    TEST_ASSERT_EQUAL_INT(1, (int)v.size());
    TEST_ASSERT_EQUAL_STRING("ok", v[0].text.c_str());
}
void test_current_index() {
    auto v = lrc::parse("[00:01.00]a\n[00:03.00]b\n[00:05.00]c\n");
    TEST_ASSERT_EQUAL_INT(-1, lrc::currentIndex(v, 500));
    TEST_ASSERT_EQUAL_INT(0, lrc::currentIndex(v, 2000));
    TEST_ASSERT_EQUAL_INT(1, lrc::currentIndex(v, 3000));
    TEST_ASSERT_EQUAL_INT(2, lrc::currentIndex(v, 9000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_timestamped_lines);
    RUN_TEST(test_skips_malformed);
    RUN_TEST(test_current_index);
    return UNITY_END();
}
