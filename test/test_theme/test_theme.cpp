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
void test_rgb565_primaries() {
    TEST_ASSERT_EQUAL_HEX16(0xF800, theme::rgb(0xFF, 0x00, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, theme::rgb(0x00, 0xFF, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x001F, theme::rgb(0x00, 0x00, 0xFF));
    TEST_ASSERT_EQUAL_HEX16(0xFFDB, theme::rgb(0xF8, 0xF8, 0xD8));   // battle box cream
}
void test_hp_color_thresholds() {
    TEST_ASSERT_EQUAL_HEX16(theme::HP_GREEN,  theme::hpColor(0.51f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_YELLOW, theme::hpColor(0.50f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_YELLOW, theme::hpColor(0.21f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_RED,    theme::hpColor(0.20f));
}
void test_hp_shine_follows_color() {
    TEST_ASSERT_EQUAL_HEX16(theme::HP_GREEN_SHINE,  theme::hpShine(0.9f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_YELLOW_SHINE, theme::hpShine(0.4f));
    TEST_ASSERT_EQUAL_HEX16(theme::HP_RED_SHINE,    theme::hpShine(0.1f));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rgb565_primaries);
    RUN_TEST(test_hp_color_thresholds);
    RUN_TEST(test_hp_shine_follows_color);
    RUN_TEST(test_known_type_returns_type_color);
    RUN_TEST(test_type_is_case_insensitive);
    RUN_TEST(test_unknown_type_returns_navy);
    RUN_TEST(test_null_returns_navy);
    return UNITY_END();
}
