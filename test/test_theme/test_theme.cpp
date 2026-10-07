#include <unity.h>
#include <math.h>
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
// WCAG contrast ratio of two RGB565 colours, as the panel shows them (5/6/5 bits expanded).
static double luminance(uint16_t c) {
    double ch[3] = {(double)(((c >> 11) & 0x1F) * 255 / 31), (double)(((c >> 5) & 0x3F) * 255 / 63),
                    (double)((c & 0x1F) * 255 / 31)};
    for (double& v : ch) {
        v /= 255.0;
        v = v <= 0.03928 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
}
static double contrast(uint16_t a, uint16_t b) {
    double la = luminance(a), lb = luminance(b);
    if (la < lb) { double t = la; la = lb; lb = t; }
    return (la + 0.05) / (lb + 0.05);
}

// In a car the deck is read at a glance, at an angle, sometimes in sunlight.
void test_hp_and_volume_bars_stand_out_from_the_empty_track() {
    TEST_ASSERT_TRUE(contrast(theme::HP_GREEN, theme::HP_EMPTY) >= 4.0);
    TEST_ASSERT_TRUE(contrast(theme::HP_YELLOW, theme::HP_EMPTY) >= 4.0);
    TEST_ASSERT_TRUE(contrast(theme::HP_RED, theme::HP_EMPTY) >= 4.0);   // last 20% of the song
    TEST_ASSERT_TRUE(contrast(theme::EXP_BLUE, theme::HP_EMPTY) >= 4.0);
}
void test_off_icons_visible_but_distinct_from_on() {
    TEST_ASSERT_TRUE(contrast(theme::ICON_OFF, theme::TOP_DARK) >= 3.0);  // still visible
    TEST_ASSERT_TRUE(contrast(theme::BOX_FILL, theme::ICON_OFF) >= 3.0);  // on vs off
}
void test_text_is_high_contrast() {
    TEST_ASSERT_TRUE(contrast(theme::TEXT, theme::BOX_FILL) >= 7.0);
    TEST_ASSERT_TRUE(contrast(theme::TEXT, theme::DLG_FILL) >= 7.0);
    TEST_ASSERT_TRUE(contrast(theme::BOX_FILL, theme::TOP_DARK) >= 7.0);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hp_and_volume_bars_stand_out_from_the_empty_track);
    RUN_TEST(test_off_icons_visible_but_distinct_from_on);
    RUN_TEST(test_text_is_high_contrast);
    RUN_TEST(test_rgb565_primaries);
    RUN_TEST(test_hp_color_thresholds);
    RUN_TEST(test_hp_shine_follows_color);
    RUN_TEST(test_known_type_returns_type_color);
    RUN_TEST(test_type_is_case_insensitive);
    RUN_TEST(test_unknown_type_returns_navy);
    RUN_TEST(test_null_returns_navy);
    return UNITY_END();
}
