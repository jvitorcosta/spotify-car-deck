#include <unity.h>
#include "../../src/util/interp.h"

void setUp() {}
void tearDown() {}

void test_current_progress() {
    const struct { uint32_t last, duration; bool playing; uint32_t since, expected; } cases[] = {
        {3000, 200000, true, 2000, 5000},        // playing advances
        {3000, 200000, false, 2000, 3000},       // paused holds
        {199000, 200000, true, 5000, 200000},    // clamps to the duration
    };
    for (const auto& c : cases)
        TEST_ASSERT_EQUAL_UINT32(c.expected, interp::currentProgressMs(c.last, c.duration, c.playing, c.since));
}
// walkX: a 40 px sprite on a bar at x 100, 200 px wide; it stays wholly inside the bar.
void test_walk_x() {
    const struct { float frac; int expected; } cases[] = {
        {0.0f, 100 + 20},          // left edge + half sprite
        {0.5f, 100 + 100},
        {1.0f, 100 + 200 - 20},    // right edge - half sprite
        {-0.5f, 100 + 20},         // out-of-range fractions clamp
        {1.5f, 100 + 200 - 20},
    };
    for (const auto& c : cases) TEST_ASSERT_EQUAL_INT(c.expected, interp::walkX(c.frac, 100, 200, 40));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_current_progress);
    RUN_TEST(test_walk_x);
    return UNITY_END();
}
