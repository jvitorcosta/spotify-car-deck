#include <unity.h>
#include <math.h>
#include <initializer_list>
#include "../../src/ui/theme.h"

void setUp() {}
void tearDown() {}

// Classic (Gen 3-era) type colours. The first table was hand-encoded wrong: grass came out
// rust red (0xA9A5), water grey (0x6B0D), ground pink.
void test_known_type_returns_type_color() {
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x68, 0x90, 0xF0), theme::typeColor("water"));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x78, 0xC8, 0x50), theme::typeColor("grass"));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF0, 0x80, 0x30), theme::typeColor("fire"));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xE0, 0xC0, 0x68), theme::typeColor("ground"));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0x58, 0x88), theme::typeColor("psychic"));
}
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
// DAY must be today's look exactly: night mode may not change the day screen.
void test_day_palette_is_todays_colours() {
    const theme::Palette& d = theme::DAY;
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x28, 0x30, 0x38), d.top);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xD8), d.topText);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x80, 0x88, 0x90), d.iconOff);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xA8, 0xD8, 0xF8), d.sky);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x60, 0xA8, 0x58), d.horizon);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x88, 0xC8, 0x78), d.grass);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xD8), d.boxFill);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x40, 0x48, 0x48), d.boxBorder);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x58, 0x70, 0x60), d.boxShadow);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x40, 0x40, 0x40), d.text);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xD8, 0xD0, 0xB0), d.textShadow);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x28, 0x48, 0x60), d.dlgFrame);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x68, 0xA0, 0xB8), d.dlgLine);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xF8, 0xF8, 0xF8), d.dlgFill);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xD0, 0xD0, 0xD0), d.dlgShadow);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x28, 0x48, 0x60), d.note);
}
void test_night_palette_is_moonlit() {
    const theme::Palette& n = theme::NIGHT;
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x18, 0x28, 0x4A), n.sky);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x2A, 0x32, 0x40), n.boxFill);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0xE0, 0xDC, 0xC8), n.text);
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x1C, 0x24, 0x30), n.dlgFill);
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
    RUN_TEST(test_day_palette_is_todays_colours);
    RUN_TEST(test_night_palette_is_moonlit);
    RUN_TEST(test_text_is_high_contrast_in_both_palettes);
    RUN_TEST(test_off_icons_visible_but_distinct_from_on_in_both_palettes);
    RUN_TEST(test_note_icons_visible_in_both_palettes);
    RUN_TEST(test_be_swaps_bytes_for_push_image);
    RUN_TEST(test_type_names_are_the_18_types);
    RUN_TEST(test_hp_and_volume_bars_stand_out_from_the_empty_track);
    RUN_TEST(test_rgb565_primaries);
    RUN_TEST(test_hp_color_thresholds);
    RUN_TEST(test_hp_shine_follows_color);
    RUN_TEST(test_known_type_returns_type_color);
    RUN_TEST(test_all_18_types_have_distinct_colors);
    RUN_TEST(test_darken_halves_each_channel);
    RUN_TEST(test_type_is_case_insensitive);
    RUN_TEST(test_unknown_type_returns_navy);
    RUN_TEST(test_null_returns_navy);
    return UNITY_END();
}
