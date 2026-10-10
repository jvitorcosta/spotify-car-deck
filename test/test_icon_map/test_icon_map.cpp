#include <unity.h>
#include <cstring>
#include "../../src/ui/icon_map.h"

using icons::Icon;
void setUp() {}
void tearDown() {}

void test_device_types_map_to_icons() {
    const struct { const char* type; Icon icon; } cases[] = {
        {"Smartphone", Icon::Phone},   {"Tablet", Icon::Phone},
        {"Computer", Icon::Laptop},
        {"Speaker", Icon::Speaker},    {"CastAudio", Icon::Speaker},
        {"TV", Icon::Tv},              {"GameConsole", Icon::Tv},
        {"Automobile", Icon::Car},
        {"smartphone", Icon::Phone},   // case-insensitive
        {"Unknown", Icon::Speaker},    // unknown, empty and null fall back to the speaker
        {"", Icon::Speaker},
        {nullptr, Icon::Speaker},
    };
    for (const auto& c : cases)
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)c.icon, (int)icons::forDevice(c.type), c.type ? c.type : "(null)");
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
    RUN_TEST(test_every_icon_is_12x12);
    RUN_TEST(test_pixel_reads_art_and_bounds);
    return UNITY_END();
}
