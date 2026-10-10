#include <unity.h>
#include <cstdint>
#include "../../src/ui/status_sprite.h"

void setUp() {}
void tearDown() {}

static bool rowHasOpaque(int y) {
    for (int x = 0; x < status_sprite::width(); ++x)
        if (status_sprite::pixel(x, y, nullptr)) return true;
    return false;
}
static bool colHasOpaque(int x) {
    for (int y = 0; y < status_sprite::height(); ++y)
        if (status_sprite::pixel(x, y, nullptr)) return true;
    return false;
}
void test_cropped_to_opaque_bounds() {   // every edge of the crop touches the creature
    TEST_ASSERT_TRUE(rowHasOpaque(0));
    TEST_ASSERT_TRUE(rowHasOpaque(status_sprite::height() - 1));
    TEST_ASSERT_TRUE(colHasOpaque(0));
    TEST_ASSERT_TRUE(colHasOpaque(status_sprite::width() - 1));
}
void test_out_of_range_is_transparent() {
    uint16_t c = 0x1234;
    TEST_ASSERT_FALSE(status_sprite::pixel(-1, 0, &c));
    TEST_ASSERT_FALSE(status_sprite::pixel(0, status_sprite::height(), &c));
    TEST_ASSERT_EQUAL_HEX16(0x1234, c);   // untouched
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cropped_to_opaque_bounds);
    RUN_TEST(test_out_of_range_is_transparent);
    return UNITY_END();
}
