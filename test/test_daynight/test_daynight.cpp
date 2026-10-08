#include <unity.h>
#include "../../src/util/daynight.h"

void setUp() {}
void tearDown() {}

static int hm(int h, int m) { return h * 60 + m; }

void test_night_starts_at_six_pm() {
    TEST_ASSERT_FALSE(daynight::isNight(hm(17, 59)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(18, 0)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(23, 59)));
}
void test_night_runs_past_midnight_until_six_am() {
    TEST_ASSERT_TRUE(daynight::isNight(hm(0, 0)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(5, 59)));
    TEST_ASSERT_FALSE(daynight::isNight(hm(6, 0)));
    TEST_ASSERT_FALSE(daynight::isNight(hm(12, 0)));
}
// 2026-10-08 22:30:00 UTC = 18:30 in Manaus.
void test_minute_of_day_applies_the_manaus_offset() {
    const int64_t t = 1791498600;
    TEST_ASSERT_EQUAL_INT(hm(22, 30), daynight::minuteOfDay(t, 0));
    TEST_ASSERT_EQUAL_INT(hm(18, 30), daynight::minuteOfDay(t, daynight::UTC_OFFSET_S));
}
// 02:00 UTC is 22:00 the previous day in Manaus: the negative offset must wrap, not go < 0.
void test_minute_of_day_wraps_to_the_previous_day() {
    const int64_t t = 1791511200;   // 2026-10-09 02:00:00 UTC
    TEST_ASSERT_EQUAL_INT(hm(2, 0), daynight::minuteOfDay(t, 0));
    TEST_ASSERT_EQUAL_INT(hm(22, 0), daynight::minuteOfDay(t, daynight::UTC_OFFSET_S));
}
void test_clock_counts_as_synced_only_after_2024() {
    TEST_ASSERT_FALSE(daynight::synced(0));            // boot: 1970
    TEST_ASSERT_FALSE(daynight::synced(86400 * 3));
    TEST_ASSERT_TRUE(daynight::synced(1791498600));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_night_starts_at_six_pm);
    RUN_TEST(test_night_runs_past_midnight_until_six_am);
    RUN_TEST(test_minute_of_day_applies_the_manaus_offset);
    RUN_TEST(test_minute_of_day_wraps_to_the_previous_day);
    RUN_TEST(test_clock_counts_as_synced_only_after_2024);
    return UNITY_END();
}
