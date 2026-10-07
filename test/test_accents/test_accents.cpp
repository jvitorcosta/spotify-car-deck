#include <unity.h>
#include <cstring>
#include "../../src/ui/accents.h"

using txt::Mark;
void setUp() {}
void tearDown() {}

void test_every_mark_is_5x2() {
    const Mark all[] = {Mark::Acute, Mark::Grave, Mark::Circumflex, Mark::Tilde,
                        Mark::Diaeresis, Mark::Cedilla, Mark::Ring};
    for (Mark m : all)
        for (int y = 0; y < accents::H; ++y) {
            TEST_ASSERT_NOT_NULL(accents::row(m, y));
            TEST_ASSERT_EQUAL_INT(accents::W, (int)strlen(accents::row(m, y)));
        }
}
void test_none_and_out_of_range() {
    TEST_ASSERT_NULL(accents::row(Mark::None, 0));
    TEST_ASSERT_NULL(accents::row(Mark::Acute, 2));
    TEST_ASSERT_NULL(accents::row(Mark::Acute, -1));
}
void test_top_row_by_case_and_cedilla() {
    TEST_ASSERT_EQUAL_INT(0, accents::topRow(Mark::Acute, 'E'));    // capitals start at row 3
    TEST_ASSERT_EQUAL_INT(3, accents::topRow(Mark::Tilde, 'a'));    // lowercase start at row 6
    TEST_ASSERT_EQUAL_INT(13, accents::topRow(Mark::Cedilla, 'c')); // under the baseline (row 12)
    TEST_ASSERT_EQUAL_INT(13, accents::topRow(Mark::Cedilla, 'C'));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_mark_is_5x2);
    RUN_TEST(test_none_and_out_of_range);
    RUN_TEST(test_top_row_by_case_and_cedilla);
    return UNITY_END();
}
