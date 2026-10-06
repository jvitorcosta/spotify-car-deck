#include <unity.h>
#include "../../src/util/walkrect.h"

void setUp() {}
void tearDown() {}

// Creeping right 1px per frame for a whole bar must never clear (blink) and the
// pushed rect must only span one frame of motion.
void test_creep_never_clears_or_grows() {
    walkrect::Rect prev{0, 0, 0, 0};
    for (int x = 128; x < 300; ++x) {
        walkrect::Rect cur{x, 141, 34, 35};
        walkrect::Plan p = walkrect::plan(prev, cur, 8);
        TEST_ASSERT_FALSE(p.clearPrev);
        TEST_ASSERT_LESS_OR_EQUAL_INT(35, p.push.w);
        prev = p.next;
    }
}
void test_first_frame_pushes_sprite_rect() {
    walkrect::Plan p = walkrect::plan({0, 0, 0, 0}, {150, 141, 34, 35}, 8);
    TEST_ASSERT_FALSE(p.clearPrev);
    TEST_ASSERT_EQUAL_INT(150, p.push.x);
    TEST_ASSERT_EQUAL_INT(34, p.push.w);
}
void test_jump_clears_old_rect() {   // seek / new song far away
    walkrect::Plan p = walkrect::plan({130, 141, 34, 35}, {250, 141, 34, 35}, 8);
    TEST_ASSERT_TRUE(p.clearPrev);
    TEST_ASSERT_EQUAL_INT(250, p.push.x);
    TEST_ASSERT_EQUAL_INT(34, p.push.w);
}
void test_small_step_unions() {
    walkrect::Plan p = walkrect::plan({200, 141, 34, 35}, {203, 141, 34, 35}, 8);
    TEST_ASSERT_FALSE(p.clearPrev);
    TEST_ASSERT_EQUAL_INT(200, p.push.x);
    TEST_ASSERT_EQUAL_INT(37, p.push.w);
    TEST_ASSERT_EQUAL_INT(203, p.next.x);   // remembers the sprite, not the union
    TEST_ASSERT_EQUAL_INT(34, p.next.w);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_creep_never_clears_or_grows);
    RUN_TEST(test_first_frame_pushes_sprite_rect);
    RUN_TEST(test_jump_clears_old_rect);
    RUN_TEST(test_small_step_unions);
    return UNITY_END();
}
