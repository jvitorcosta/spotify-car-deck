#include <unity.h>
#include <cstring>
#include "../../src/ui/icon_map.h"

using icons::Icon;
void setUp() {}
void tearDown() {}

void test_device_types_map_to_icons() {
    TEST_ASSERT_EQUAL_INT((int)Icon::Phone,   (int)icons::forDevice("Smartphone"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Phone,   (int)icons::forDevice("Tablet"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Laptop,  (int)icons::forDevice("Computer"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice("Speaker"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice("CastAudio"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Tv,      (int)icons::forDevice("TV"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Tv,      (int)icons::forDevice("GameConsole"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Car,     (int)icons::forDevice("Automobile"));
}
void test_device_type_case_insensitive() {
    TEST_ASSERT_EQUAL_INT((int)Icon::Phone, (int)icons::forDevice("smartphone"));
}
void test_unknown_empty_null_device_is_speaker() {
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice("Unknown"));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice(""));
    TEST_ASSERT_EQUAL_INT((int)Icon::Speaker, (int)icons::forDevice(nullptr));
}
void test_every_icon_is_12x12() {
    for (int i = 0; i <= (int)Icon::Note; ++i) {
        for (int y = 0; y < icons::SIZE; ++y) {
            const char* r = icons::row((Icon)i, y);
            TEST_ASSERT_NOT_NULL(r);
            TEST_ASSERT_EQUAL_INT(icons::SIZE, (int)strlen(r));
        }
        TEST_ASSERT_NULL(icons::row((Icon)i, icons::SIZE));
    }
}
void test_pixel_reads_art_and_bounds() {
    TEST_ASSERT_TRUE(icons::pixel(Icon::Phone, 3, 0));    // "...######..." top edge
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, 0, 0));
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, -1, 0));
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, 12, 0));
    TEST_ASSERT_FALSE(icons::pixel(Icon::Phone, 0, 12));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_device_types_map_to_icons);
    RUN_TEST(test_device_type_case_insensitive);
    RUN_TEST(test_unknown_empty_null_device_is_speaker);
    RUN_TEST(test_every_icon_is_12x12);
    RUN_TEST(test_pixel_reads_art_and_bounds);
    return UNITY_END();
}
