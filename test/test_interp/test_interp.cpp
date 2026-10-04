#include <unity.h>
#include "../../src/util/interp.h"

void setUp() {}
void tearDown() {}

void test_playing_advances() {
    TEST_ASSERT_EQUAL_UINT32(5000, interp::currentProgressMs(3000, 200000, true, 2000));
}
void test_paused_holds() {
    TEST_ASSERT_EQUAL_UINT32(3000, interp::currentProgressMs(3000, 200000, false, 2000));
}
void test_clamps_to_duration() {
    TEST_ASSERT_EQUAL_UINT32(200000, interp::currentProgressMs(199000, 200000, true, 5000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_playing_advances);
    RUN_TEST(test_paused_holds);
    RUN_TEST(test_clamps_to_duration);
    return UNITY_END();
}
