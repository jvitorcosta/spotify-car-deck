#include <unity.h>
#include <initializer_list>
#include "../../src/util/bootanim.h"

using namespace bootanim;

void setUp() {}
void tearDown() {}

void test_enter_eases_from_off_screen_to_cruise() {
    TEST_ASSERT_EQUAL_INT(-202, carX(0, NO_EXIT));
    TEST_ASSERT_EQUAL_INT(60, carX(900, NO_EXIT));
}
// The sway starts at phase 0 when the enter phase ends: no jump at the hand-over.
void test_cruise_sways_around_the_centre() {
    TEST_ASSERT_EQUAL_INT(60, carX(900, NO_EXIT));
    TEST_ASSERT_EQUAL_INT(66, carX(900 + 1000, NO_EXIT));
    TEST_ASSERT_EQUAL_INT(54, carX(900 + 3000, NO_EXIT));
    for (uint32_t t = 900; t < 60000; t += 37) {
        int x = carX(t, NO_EXIT);
        TEST_ASSERT_TRUE(x >= 54 && x <= 66);
    }
}
void test_exit_accelerates_off_screen_from_the_cruise_x() {
    const uint32_t tExit = 900 + 1000;                   // cruise x = 66 here
    TEST_ASSERT_EQUAL_INT(66, carX(tExit, tExit));
    TEST_ASSERT_EQUAL_INT(132, carX(tExit + 450, tExit));   // 66 + 264 * 0.25
    TEST_ASSERT_EQUAL_INT(330, carX(tExit + 900, tExit));
    TEST_ASSERT_EQUAL_INT(330, carX(tExit + 5000, tExit));
    TEST_ASSERT_FALSE(exitDone(tExit + 899, tExit));
    TEST_ASSERT_TRUE(exitDone(tExit + 900, tExit));
    TEST_ASSERT_FALSE(exitDone(tExit + 900, NO_EXIT));
}
// Exit only after 5 s, once the sound has ended, and while online (Review Focus 4).
void test_exit_gating() {
    TEST_ASSERT_FALSE(mayExit(4999, true, true));
    TEST_ASSERT_TRUE(mayExit(5000, true, true));
    TEST_ASSERT_FALSE(mayExit(6000, false, true));
    TEST_ASSERT_FALSE(mayExit(6000, true, false));
    TEST_ASSERT_TRUE(mayExit(600000, true, true));
}
void test_scroll_wraps_at_its_period() {
    TEST_ASSERT_EQUAL_INT(90, scroll(1000, 90, 160));
    TEST_ASSERT_EQUAL_INT(20, scroll(2000, 90, 160));
    TEST_ASSERT_EQUAL_INT(0, scroll(0, 14, 720));
}
void test_rgb565_helpers() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(255, 255, 255));
    TEST_ASSERT_EQUAL_HEX16(0xBE19, hex565(0xBCC3CB));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, blend565(0xFFFF, 0x0000, 255));
    TEST_ASSERT_EQUAL_HEX16(0x0000, blend565(0xFFFF, 0x0000, 0));
    TEST_ASSERT_EQUAL_HEX16(0x8410, blend565(0xFFFF, 0x0000, 128));
    TEST_ASSERT_EQUAL_HEX16(0x8410, scale565(0xFFFF, 128));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, scale565(0xFFFF, 255));
}
// One full hue cycle every 3 s.
void test_underglow_hue_cycles() {
    TEST_ASSERT_EQUAL_HEX16(hue565(0), hue565(3000));
}
// Lights brighten with loudness, never dim; the streetlight reflection fades with distance
// and is gone past its reach.
void test_lights_follow_loudness_and_reflection_falls_off() {
    for (int e = 1; e <= 255; ++e) {
        const uint8_t a = (uint8_t)(e - 1), b = (uint8_t)e;
        TEST_ASSERT_TRUE(headColor(b) >= headColor(a));
        TEST_ASSERT_TRUE(tailLevel(b) >= tailLevel(a));
        TEST_ASSERT_TRUE(tailGlowAlpha(b) >= tailGlowAlpha(a));
        TEST_ASSERT_TRUE(beamAlpha(b) >= beamAlpha(a));
        TEST_ASSERT_TRUE(glowAlpha(b) >= glowAlpha(a));
    }
    for (int e = 0; e <= 12; ++e) TEST_ASSERT_EQUAL_UINT8(0, tailGlowAlpha((uint8_t)e));
    TEST_ASSERT_TRUE(tailGlowAlpha(255) > 0);
    for (bool upper : {true, false}) {
        TEST_ASSERT_TRUE(reflectAlpha(0, upper) > 0);
        for (int d = 1; d <= 320; ++d) {
            TEST_ASSERT_TRUE(reflectAlpha(d, upper) <= reflectAlpha(d - 1, upper));
            TEST_ASSERT_EQUAL_UINT8(reflectAlpha(d, upper), reflectAlpha(-d, upper));
        }
        TEST_ASSERT_EQUAL_UINT8(0, reflectAlpha(320, upper));   // non-increasing, so 0 from its reach on
    }
}
// Partly off-screen spans are trimmed, fully off-screen ones rejected (Review Focus 2).
void test_clip() {
    int a = -202, b = -2;
    TEST_ASSERT_FALSE(clip(a, b, 0, 320));
    a = -10; b = 190;
    TEST_ASSERT_TRUE(clip(a, b, 0, 320));
    TEST_ASSERT_EQUAL_INT(0, a);
    TEST_ASSERT_EQUAL_INT(190, b);
    a = 300; b = 500;
    TEST_ASSERT_TRUE(clip(a, b, 0, 320));
    TEST_ASSERT_EQUAL_INT(320, b);
    a = 330; b = 530;
    TEST_ASSERT_FALSE(clip(a, b, 0, 320));
}

// Review Focus 1: no jump at u = 0 from any cruise x, stops 40 px further.
void test_brake_starts_where_cruise_left_off() {
    for (int xc : {54, 60, 66}) {
        TEST_ASSERT_EQUAL_INT(xc, brakeX(xc, 0));
        TEST_ASSERT_EQUAL_INT(xc + 30, brakeX(xc, 300));
        TEST_ASSERT_EQUAL_INT(xc + 40, brakeX(xc, 600));
        TEST_ASSERT_EQUAL_INT(xc + 40, brakeX(xc, 5000));
    }
}
// The world and the wheels slow with the car: the braking clock advances 300 ms over the 600 ms brake.
void test_brake_time_slows_down() {
    TEST_ASSERT_EQUAL_UINT32(7000u, brakeTime(7000, 0));
    TEST_ASSERT_EQUAL_UINT32(7225u, brakeTime(7000, 300));
    TEST_ASSERT_EQUAL_UINT32(7300u, brakeTime(7000, 600));
    TEST_ASSERT_EQUAL_UINT32(7300u, brakeTime(7000, 3000));
}
void test_brake_fades_to_black() {
    TEST_ASSERT_EQUAL_UINT8(255, brakeLevel(0));
    TEST_ASSERT_EQUAL_UINT8(128, brakeLevel(300));
    TEST_ASSERT_EQUAL_UINT8(0, brakeLevel(600));
    TEST_ASSERT_EQUAL_UINT8(0, brakeLevel(2000));
}

// ---- v3: photo montage after the brake ----
void test_montage_frame_index() {
    TEST_ASSERT_EQUAL_INT(-1, montageFrame(599, 10, 67));          // still braking
    TEST_ASSERT_EQUAL_INT(0, montageFrame(600, 10, 67));
    TEST_ASSERT_EQUAL_INT(10, montageFrame(1600, 10, 67));
    TEST_ASSERT_EQUAL_INT(66, montageFrame(600 + 6699, 10, 67));
    TEST_ASSERT_EQUAL_INT(67, montageFrame(600 + 6700, 10, 67));   // finished
    TEST_ASSERT_EQUAL_INT(67, montageFrame(999999, 10, 67));
}
// Review Focus 1: no montage -> finished as soon as the brake is over.
void test_no_frames_finishes_at_once() {
    TEST_ASSERT_EQUAL_INT(-1, montageFrame(100, 10, 0));
    TEST_ASSERT_EQUAL_INT(0, montageFrame(600, 10, 0));
}

// Finale sound: starts so it ends with the montage's last frame (the fade to black).
void test_finale_start_frame() {
    TEST_ASSERT_EQUAL_INT(48, finaleStartFrame(67, 10, 1840));   // 1.84 s = 19 frames before the end
    TEST_ASSERT_EQUAL_INT(57, finaleStartFrame(67, 10, 1000));
    TEST_ASSERT_EQUAL_INT(0, finaleStartFrame(67, 10, 9000));    // longer than the montage: from the start
    TEST_ASSERT_EQUAL_INT(-1, finaleStartFrame(67, 10, 0));      // no finale clip
    TEST_ASSERT_EQUAL_INT(-1, finaleStartFrame(0, 10, 1840));    // no montage
}

void test_opener_fits() {
    TEST_ASSERT_TRUE(openerFits(61, 10, 900));      // 0.9 s opener, finale from frame 61
    TEST_ASSERT_FALSE(openerFits(9, 10, 900));      // ends exactly where the finale starts: no margin
    TEST_ASSERT_FALSE(openerFits(11, 10, 900));     // 0.9 s + 0.3 s margin needs 12 frames
    TEST_ASSERT_TRUE(openerFits(12, 10, 900));
    TEST_ASSERT_FALSE(openerFits(8, 10, 900));      // would overlap: one speaker, one clip
    TEST_ASSERT_FALSE(openerFits(0, 10, 900));      // finale from the first frame
    TEST_ASSERT_TRUE(openerFits(-1, 10, 900));      // no finale
    TEST_ASSERT_FALSE(openerFits(61, 10, 0));       // no opener clip
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_enter_eases_from_off_screen_to_cruise);
    RUN_TEST(test_cruise_sways_around_the_centre);
    RUN_TEST(test_exit_accelerates_off_screen_from_the_cruise_x);
    RUN_TEST(test_exit_gating);
    RUN_TEST(test_scroll_wraps_at_its_period);
    RUN_TEST(test_rgb565_helpers);
    RUN_TEST(test_underglow_hue_cycles);
    RUN_TEST(test_lights_follow_loudness_and_reflection_falls_off);
    RUN_TEST(test_clip);
    RUN_TEST(test_brake_starts_where_cruise_left_off);
    RUN_TEST(test_brake_time_slows_down);
    RUN_TEST(test_brake_fades_to_black);
    RUN_TEST(test_montage_frame_index);
    RUN_TEST(test_no_frames_finishes_at_once);
    RUN_TEST(test_finale_start_frame);
    RUN_TEST(test_opener_fits);
    return UNITY_END();
}
