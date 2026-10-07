#include <unity.h>
#include "../../src/ui/labels.h"

void setUp() {}
void tearDown() {}

void test_top_title() {
    TEST_ASSERT_EQUAL_STRING("NOW PLAYING", ui::topTitle(true));
    TEST_ASSERT_EQUAL_STRING("PAUSED", ui::topTitle(false));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_top_title);
    return UNITY_END();
}
