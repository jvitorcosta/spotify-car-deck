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
// walkX (moved from the former test_walk suite, which also only tested util/interp)
void test_walk_start() {   // at 0% the sprite sits at the left edge (+ half sprite)
    TEST_ASSERT_EQUAL_INT(100 + 20, interp::walkX(0.0f, 100, 200, 40));
}
void test_walk_mid() {
    TEST_ASSERT_EQUAL_INT(100 + 100, interp::walkX(0.5f, 100, 200, 40));
}
void test_walk_end_clamps() {  // at 100% stays inside the bar (right edge - half sprite)
    TEST_ASSERT_EQUAL_INT(100 + 200 - 20, interp::walkX(1.0f, 100, 200, 40));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_playing_advances);
    RUN_TEST(test_paused_holds);
    RUN_TEST(test_clamps_to_duration);
    RUN_TEST(test_walk_start);
    RUN_TEST(test_walk_mid);
    RUN_TEST(test_walk_end_clamps);
    return UNITY_END();
}
