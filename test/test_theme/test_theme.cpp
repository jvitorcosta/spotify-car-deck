#include <unity.h>
#include "../../src/ui/theme.h"

void setUp() {}
void tearDown() {}

void test_known_type_returns_type_color() {
    TEST_ASSERT_EQUAL_UINT16(0x6B0D, theme::typeColor("water")); // GBA blue
    TEST_ASSERT_EQUAL_UINT16(0xA9A5, theme::typeColor("grass")); // GBA green
}
void test_type_is_case_insensitive() {
    TEST_ASSERT_EQUAL_UINT16(theme::typeColor("water"), theme::typeColor("WATER"));
}
void test_unknown_type_returns_navy() {
    TEST_ASSERT_EQUAL_UINT16(theme::GBA_NAVY, theme::typeColor("nonsense"));
}
void test_null_returns_navy() {
    TEST_ASSERT_EQUAL_UINT16(theme::GBA_NAVY, theme::typeColor(nullptr));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_known_type_returns_type_color);
    RUN_TEST(test_type_is_case_insensitive);
    RUN_TEST(test_unknown_type_returns_navy);
    RUN_TEST(test_null_returns_navy);
    return UNITY_END();
}
