#include <unity.h>
#include "../../src/util/interp.h"

void setUp() {}
void tearDown() {}

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
    RUN_TEST(test_walk_start);
    RUN_TEST(test_walk_mid);
    RUN_TEST(test_walk_end_clamps);
    return UNITY_END();
}
