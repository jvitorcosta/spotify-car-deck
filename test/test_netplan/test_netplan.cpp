#include <unity.h>
#include "../../src/util/netplan.h"

using netplan::Step;
void setUp() {}
void tearDown() {}

void test_first_track_bumps_generation() {
    netplan::TrackGen g;
    TEST_ASSERT_EQUAL_UINT32(0, g.gen());
    TEST_ASSERT_TRUE(g.update("Song A"));
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_same_track_does_not_bump() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_FALSE(g.update("Song A"));
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_new_track_bumps() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_TRUE(g.update("Song B"));
    TEST_ASSERT_EQUAL_UINT32(2, g.gen());
}
void test_empty_name_is_ignored_and_keeps_last() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_FALSE(g.update(""));
    TEST_ASSERT_FALSE(g.update(nullptr));
    TEST_ASSERT_FALSE(g.update("Song A"));   // resumed same song: no new Pokemon
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_order_without_prefetched_walker() {
    // Art and lyrics first: they are what the user waits for; the walker is usually
    // prefetched, and when it isn't it can follow.
    netplan::Work w = netplan::freshWork(false);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
    netplan::done(w, Step::Art);
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w));
    netplan::done(w, Step::Lyrics);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w));
    netplan::done(w, Step::Walk);
    TEST_ASSERT_EQUAL_INT((int)Step::Prefetch, (int)netplan::next(w));
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w));
}
void test_prefetched_walker_skips_walk_step() {
    netplan::Work w = netplan::freshWork(true);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
}
void test_link_stays_up_through_isolated_failures() {
    netplan::LinkGate g;
    TEST_ASSERT_FALSE(g.update(false));   // one failed poll is not "no signal"
    TEST_ASSERT_FALSE(g.update(false));
    TEST_ASSERT_FALSE(g.update(true));    // success resets the count
    TEST_ASSERT_FALSE(g.update(false));
    TEST_ASSERT_FALSE(g.update(false));
}
void test_link_down_after_three_consecutive_failures() {
    netplan::LinkGate g;
    g.update(false); g.update(false);
    TEST_ASSERT_TRUE(g.update(false));
    TEST_ASSERT_TRUE(g.update(false));
    TEST_ASSERT_FALSE(g.update(true));    // first good poll clears it
}
void test_idle_work_is_none() {
    netplan::Work w{};
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w));
}
void test_can_run_gates_on_largest_block() {
    using netplan::canRun;
    TEST_ASSERT_TRUE(canRun(Step::Art, 6000));          // plain HTTP: small
    TEST_ASSERT_FALSE(canRun(Step::Art, 5999));
    TEST_ASSERT_TRUE(canRun(Step::Lyrics, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Lyrics, netplan::TLS_NEED - 1));
    // Walker downloads use the fixed scratch buffer (no malloc): they need only TLS.
    TEST_ASSERT_TRUE(canRun(Step::Walk, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Walk, netplan::TLS_NEED - 1));
    TEST_ASSERT_TRUE(canRun(Step::Prefetch, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Prefetch, netplan::TLS_NEED - 1));
    TEST_ASSERT_TRUE(canRun(Step::None, 0));
}
using Act = netplan::Health::Action;
void test_health_success_is_none() {
    netplan::Health h;
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(true, true, 1000));
    TEST_ASSERT_FALSE(h.optionalPaused());
}
void test_health_two_failures_pause_optional_until_success() {
    netplan::Health h;
    h.onPoll(true, true, 1000);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(false, true, 5000));
    TEST_ASSERT_EQUAL_INT((int)Act::PauseOptional, (int)h.onPoll(false, true, 9000));
    TEST_ASSERT_TRUE(h.optionalPaused());
    h.onPoll(true, true, 13000);
    TEST_ASSERT_FALSE(h.optionalPaused());
}
void test_health_restart_after_180s_with_wifi_up() {
    netplan::Health h;
    h.onPoll(true, true, 1000);
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, 1000 + 179000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, 1000 + 180000));
}
void test_health_wifi_down_never_restarts_and_resets_timer() {
    netplan::Health h;
    h.onPoll(true, true, 1000);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(false, false, 500000));
    // WiFi back at 500 s: the 180 s clock starts again from there
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, 504000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, 500000 + 180000));
}
void test_health_first_call_failing_starts_clock() {
    netplan::Health h;
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, 900000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_track_bumps_generation);
    RUN_TEST(test_same_track_does_not_bump);
    RUN_TEST(test_new_track_bumps);
    RUN_TEST(test_empty_name_is_ignored_and_keeps_last);
    RUN_TEST(test_order_without_prefetched_walker);
    RUN_TEST(test_prefetched_walker_skips_walk_step);
    RUN_TEST(test_link_stays_up_through_isolated_failures);
    RUN_TEST(test_link_down_after_three_consecutive_failures);
    RUN_TEST(test_idle_work_is_none);
    RUN_TEST(test_can_run_gates_on_largest_block);
    RUN_TEST(test_health_success_is_none);
    RUN_TEST(test_health_two_failures_pause_optional_until_success);
    RUN_TEST(test_health_restart_after_180s_with_wifi_up);
    RUN_TEST(test_health_wifi_down_never_restarts_and_resets_timer);
    RUN_TEST(test_health_first_call_failing_starts_clock);
    return UNITY_END();
}
