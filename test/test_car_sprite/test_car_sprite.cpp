#include <unity.h>
#include "../../src/ui/car_sprite.h"
#include "../../src/util/bootanim.h"

void setUp() {}
void tearDown() {}

static uint8_t kindAt(int x, int y, uint16_t* c = nullptr) {
    uint8_t k = 99;
    car_sprite::pixel(x, y, c, &k);
    return k;
}

// The boot scene starts with the car wholly off-screen left and ends with it wholly off right.
void test_car_starts_and_ends_off_screen() {
    TEST_ASSERT_TRUE(bootanim::START_X + car_sprite::width() <= 0);
    TEST_ASSERT_TRUE(bootanim::END_X >= 320);
}
void test_transparent_and_outside_is_none() {
    TEST_ASSERT_FALSE(car_sprite::pixel(0, 0, nullptr, nullptr));       // the box corner above the trunk
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(-1, 10));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(car_sprite::width(), 10));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(10, -1));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(10, car_sprite::height()));
    TEST_ASSERT_FALSE(car_sprite::pixel(car_sprite::width(), 10, nullptr, nullptr));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_car_starts_and_ends_off_screen);
    RUN_TEST(test_transparent_and_outside_is_none);
    return UNITY_END();
}
