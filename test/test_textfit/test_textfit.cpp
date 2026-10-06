#include <unity.h>
#include <string>
#include "../../src/util/textfit.h"

void setUp() {}
void tearDown() {}

// Fixed-width fake font: 6 px per character.
static int w6(const std::string& s, void*) { return (int)s.size() * 6; }

void test_short_line_stays_single() {
    textfit::TwoLines t = textfit::wrapTwo("hello world", 120, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("hello world", t.a.c_str());
    TEST_ASSERT_EQUAL_STRING("", t.b.c_str());
}
void test_breaks_at_last_fitting_space() {
    // 20 chars fit per line (120 px)
    textfit::TwoLines t = textfit::wrapTwo("and I said hey what is going on", 120, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("and I said hey what", t.a.c_str());
    TEST_ASSERT_EQUAL_STRING("is going on", t.b.c_str());
}
void test_overlong_second_line_gets_ellipsis() {
    textfit::TwoLines t = textfit::wrapTwo(
        "one two three four five six seven eight nine ten eleven", 60, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("one two", t.a.c_str());
    TEST_ASSERT_TRUE(t.b.size() * 6 <= 60);
    TEST_ASSERT_EQUAL_STRING("...", t.b.substr(t.b.size() - 3).c_str());
}
void test_no_space_hard_splits() {
    textfit::TwoLines t = textfit::wrapTwo("aaaaaaaaaaaaaaaaaaaaaaaaa", 60, w6, nullptr);   // 25 chars, 10 fit
    TEST_ASSERT_EQUAL_INT(10, (int)t.a.size());
    TEST_ASSERT_TRUE(t.b.size() * 6 <= 60);
}
void test_empty_is_empty() {
    textfit::TwoLines t = textfit::wrapTwo("", 60, w6, nullptr);
    TEST_ASSERT_EQUAL_STRING("", t.a.c_str());
    TEST_ASSERT_EQUAL_STRING("", t.b.c_str());
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_short_line_stays_single);
    RUN_TEST(test_breaks_at_last_fitting_space);
    RUN_TEST(test_overlong_second_line_gets_ellipsis);
    RUN_TEST(test_no_space_hard_splits);
    RUN_TEST(test_empty_is_empty);
    return UNITY_END();
}
