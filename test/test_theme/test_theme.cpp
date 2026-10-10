#include <unity.h>
#include <math.h>
#include <initializer_list>
#include "../../src/ui/theme.h"

void setUp() {}
void tearDown() {}

void test_all_18_types_have_distinct_colors() {
    static const char* T[] = {"normal", "fire", "water", "electric", "grass", "ice", "fighting",
                              "poison", "ground", "flying", "psychic", "bug", "rock", "ghost",
                              "dragon", "dark", "steel", "fairy"};
    for (int i = 0; i < 18; ++i) {
        TEST_ASSERT_NOT_EQUAL(theme::GBA_NAVY, theme::typeColor(T[i]));
        for (int j = 0; j < i; ++j)
            TEST_ASSERT_NOT_EQUAL(theme::typeColor(T[j]), theme::typeColor(T[i]));
    }
}
void test_darken_halves_each_channel() {
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x78, 0x60, 0x40), theme::darken(theme::rgb(0xF0, 0xC0, 0x80)));
    TEST_ASSERT_EQUAL_HEX16(0x0000, theme::darken(0x0000));
}
void test_type_lookup_ignores_case_and_falls_back_to_navy() {
    TEST_ASSERT_EQUAL_UINT16(theme::typeColor("water"), theme::typeColor("WATER"));
    TEST_ASSERT_EQUAL_UINT16(theme::GBA_NAVY, theme::typeColor("nonsense"));
    TEST_ASSERT_EQUAL_UINT16(theme::GBA_NAVY, theme::typeColor(nullptr));
}
void test_rgb565_primaries() {
    TEST_ASSERT_EQUAL_HEX16(0xF800, theme::rgb(0xFF, 0x00, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, theme::rgb(0x00, 0xFF, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x001F, theme::rgb(0x00, 0x00, 0xFF));
    TEST_ASSERT_EQUAL_HEX16(0xFFDB, theme::rgb(0xF8, 0xF8, 0xD8));   // battle box cream
}
void test_hp_color_thresholds() {   // the shine follows the bar colour
    const struct { float frac; uint16_t color, shine; } cases[] = {
        {0.51f, theme::HP_GREEN, theme::HP_GREEN_SHINE},
        {0.50f, theme::HP_YELLOW, theme::HP_YELLOW_SHINE},
        {0.21f, theme::HP_YELLOW, theme::HP_YELLOW_SHINE},
        {0.20f, theme::HP_RED, theme::HP_RED_SHINE},
    };
    for (const auto& c : cases) {
        TEST_ASSERT_EQUAL_HEX16(c.color, theme::hpColor(c.frac));
        TEST_ASSERT_EQUAL_HEX16(c.shine, theme::hpShine(c.frac));
    }
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
// Read at a glance in a car, day and night.
void test_text_is_high_contrast_in_both_palettes() {
    for (const theme::Palette* p : {&theme::DAY, &theme::NIGHT}) {
        TEST_ASSERT_TRUE(contrast(p->text, p->boxFill) >= 7.0);
        TEST_ASSERT_TRUE(contrast(p->text, p->dlgFill) >= 7.0);
        TEST_ASSERT_TRUE(contrast(p->topText, p->top) >= 7.0);
    }
}
void test_off_icons_visible_but_distinct_from_on_in_both_palettes() {
    for (const theme::Palette* p : {&theme::DAY, &theme::NIGHT}) {
        TEST_ASSERT_TRUE(contrast(p->iconOff, p->top) >= 3.0);       // still visible
        TEST_ASSERT_TRUE(contrast(p->topText, p->iconOff) >= 3.0);   // on vs off
    }
}
void test_note_icons_visible_in_both_palettes() {
    for (const theme::Palette* p : {&theme::DAY, &theme::NIGHT})
        TEST_ASSERT_TRUE(contrast(p->note, p->dlgFill) >= 3.0);
}
void test_set_night_switches_the_active_palette() {
    TEST_ASSERT_EQUAL_PTR(&theme::DAY, &theme::pal());           // boot: day
    TEST_ASSERT_FALSE(theme::isNightActive());
    theme::setNight(true);
    TEST_ASSERT_EQUAL_PTR(&theme::NIGHT, &theme::pal());
    TEST_ASSERT_TRUE(theme::isNightActive());
    theme::setNight(false);
    TEST_ASSERT_EQUAL_PTR(&theme::DAY, &theme::pal());
}
void test_be_swaps_bytes_for_push_image() {
    static_assert(theme::be(0x1234) == 0x3412, "constexpr");
    TEST_ASSERT_EQUAL_HEX16(0x3412, theme::be(0x1234));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xD8), theme::be(theme::be(theme::rgb(0xF8, 0xF8, 0xD8))));
}
void test_type_names_are_the_18_types() {
    TEST_ASSERT_EQUAL_INT(18, theme::typeCount());
    TEST_ASSERT_EQUAL_STRING("normal", theme::typeName(0));
    TEST_ASSERT_EQUAL_STRING("fairy", theme::typeName(17));
    TEST_ASSERT_EQUAL_STRING("", theme::typeName(18));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_set_night_switches_the_active_palette);
    RUN_TEST(test_text_is_high_contrast_in_both_palettes);
    RUN_TEST(test_off_icons_visible_but_distinct_from_on_in_both_palettes);
    RUN_TEST(test_note_icons_visible_in_both_palettes);
    RUN_TEST(test_be_swaps_bytes_for_push_image);
    RUN_TEST(test_type_names_are_the_18_types);
    RUN_TEST(test_hp_and_volume_bars_stand_out_from_the_empty_track);
    RUN_TEST(test_rgb565_primaries);
    RUN_TEST(test_hp_color_thresholds);
    RUN_TEST(test_all_18_types_have_distinct_colors);
    RUN_TEST(test_darken_halves_each_channel);
    RUN_TEST(test_type_lookup_ignores_case_and_falls_back_to_navy);
    return UNITY_END();
}
