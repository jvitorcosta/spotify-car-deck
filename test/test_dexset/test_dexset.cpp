#include <unity.h>
#include "../../src/util/dexset.h"

void setUp() {}
void tearDown() {}

void test_empty_has_nothing() {
    dexset::Set s;
    TEST_ASSERT_FALSE(s.has(1));
    TEST_ASSERT_FALSE(s.has(0));       // 0 is "no Pokemon", never a member
}
void test_add_then_has() {
    dexset::Set s;
    s.add(25);
    s.add(151);
    TEST_ASSERT_TRUE(s.has(25));
    TEST_ASSERT_TRUE(s.has(151));
    TEST_ASSERT_FALSE(s.has(26));
}
void test_adding_twice_keeps_one_entry() {
    dexset::Set s;
    for (int i = 0; i < dexset::Set::N + 5; ++i) s.add(7);   // must not evict others
    s.add(8);
    TEST_ASSERT_TRUE(s.has(7));
    TEST_ASSERT_TRUE(s.has(8));
}
void test_full_set_evicts_oldest() {
    dexset::Set s;
    for (int i = 1; i <= dexset::Set::N; ++i) s.add(i);
    TEST_ASSERT_TRUE(s.has(1));
    s.add(1000);
    TEST_ASSERT_FALSE(s.has(1));
    TEST_ASSERT_TRUE(s.has(2));
    TEST_ASSERT_TRUE(s.has(1000));
}
void test_ignores_non_positive() {
    dexset::Set s;
    s.add(0);
    s.add(-3);
    TEST_ASSERT_FALSE(s.has(0));
    TEST_ASSERT_FALSE(s.has(-3));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_has_nothing);
    RUN_TEST(test_add_then_has);
    RUN_TEST(test_adding_twice_keeps_one_entry);
    RUN_TEST(test_full_set_evicts_oldest);
    RUN_TEST(test_ignores_non_positive);
    return UNITY_END();
}
