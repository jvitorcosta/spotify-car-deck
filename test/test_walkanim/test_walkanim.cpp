#include <unity.h>
#include "../../src/util/walkanim.h"

using namespace walkanim;
void setUp() {}
void tearDown() {}

void test_box_union() {
    Box b = emptyBox();
    TEST_ASSERT_TRUE(isEmpty(b));
    include(b, 5, 7); include(b, 2, 9); include(b, 4, 3);
    TEST_ASSERT_FALSE(isEmpty(b));
    TEST_ASSERT_EQUAL_INT(2, b.x0); TEST_ASSERT_EQUAL_INT(3, b.y0);
    TEST_ASSERT_EQUAL_INT(5, b.x1); TEST_ASSERT_EQUAL_INT(9, b.y1);
}
void test_fit_keeps_native_when_it_fits() {
    Fit f = fitBand(24, 30, 32, 64);
    TEST_ASSERT_EQUAL_INT(24, f.w); TEST_ASSERT_EQUAL_INT(30, f.h);
}
void test_fit_downscales_tall_keeping_aspect() {
    Fit f = fitBand(40, 64, 32, 64);
    TEST_ASSERT_EQUAL_INT(20, f.w); TEST_ASSERT_EQUAL_INT(32, f.h);
}
void test_fit_downscales_wide() {
    Fit f = fitBand(128, 32, 32, 64);
    TEST_ASSERT_EQUAL_INT(64, f.w); TEST_ASSERT_EQUAL_INT(16, f.h);
}
void test_fit_never_zero() {
    Fit f = fitBand(1, 200, 32, 64);
    TEST_ASSERT_EQUAL_INT(1, f.w); TEST_ASSERT_EQUAL_INT(32, f.h);
}
void test_src_index_maps_nearest() {
    TEST_ASSERT_EQUAL_INT(10, srcIndex(0, 16, 10, 32));
    TEST_ASSERT_EQUAL_INT(40, srcIndex(15, 16, 10, 32));
    TEST_ASSERT_EQUAL_INT(13, srcIndex(3, 8, 10, 8));   // 1:1
}
void test_keep_every_fits_cap() {
    TEST_ASSERT_EQUAL_INT(1, keepEvery(4, 28, 32, 16384));    // 4*28*32*3 = 10752
    TEST_ASSERT_EQUAL_INT(2, keepEvery(10, 32, 32, 16384));   // 10 frames = 30720 -> 5 = 15360
    TEST_ASSERT_EQUAL_INT(0, keepEvery(4, 80, 80, 16384));    // one frame = 19200
}
void test_kept_count_rounds_up() {
    TEST_ASSERT_EQUAL_INT(5, keptCount(10, 2));
    TEST_ASSERT_EQUAL_INT(4, keptCount(7, 2));
    TEST_ASSERT_EQUAL_INT(4, keptCount(4, 1));
}
void test_merged_durations() {
    const uint16_t ticks[5] = {6, 6, 12, 12, 30};
    uint16_t ms[3];
    mergedDurationsMs(ticks, 5, 2, ms);
    TEST_ASSERT_EQUAL_UINT16(200, ms[0]);   // (6+6) ticks
    TEST_ASSERT_EQUAL_UINT16(400, ms[1]);   // (12+12)
    TEST_ASSERT_EQUAL_UINT16(500, ms[2]);   // 30
}
void test_frame_at_cycles() {
    const uint16_t d[3] = {100, 200, 100};
    TEST_ASSERT_EQUAL_INT(0, frameAt(0, d, 3));
    TEST_ASSERT_EQUAL_INT(0, frameAt(99, d, 3));
    TEST_ASSERT_EQUAL_INT(1, frameAt(100, d, 3));
    TEST_ASSERT_EQUAL_INT(2, frameAt(399, d, 3));
    TEST_ASSERT_EQUAL_INT(0, frameAt(400, d, 3));   // wraps
    TEST_ASSERT_EQUAL_INT(1, frameAt(400 * 1000 + 150, d, 3));
}
void test_frame_at_degenerate() {
    const uint16_t z[2] = {0, 0};
    TEST_ASSERT_EQUAL_INT(0, frameAt(1234, z, 2));
    TEST_ASSERT_EQUAL_INT(0, frameAt(1234, z, 0));
}
// PNGdec 1.1.6 keeps the current and previous line (+16 B alignment each) in a 2562-byte buffer.
void test_png_fits_rgba_up_to_316px() {
    TEST_ASSERT_TRUE(pngFits(128, 6, 8));    // pitch 512
    TEST_ASSERT_TRUE(pngFits(316, 6, 8));    // pitch 1264 -> 2560
    TEST_ASSERT_FALSE(pngFits(317, 6, 8));   // pitch 1268 -> 2568
    TEST_ASSERT_FALSE(pngFits(320, 6, 8));
}
void test_png_fits_indexed_wide() {
    TEST_ASSERT_TRUE(pngFits(512, 3, 8));    // pitch 512
    TEST_ASSERT_TRUE(pngFits(512, 3, 4));    // pitch 256
}
void test_png_fits_truecolor_and_gray_alpha() {
    TEST_ASSERT_TRUE(pngFits(400, 2, 8));    // RGB pitch 1200 -> 2432
    TEST_ASSERT_FALSE(pngFits(500, 2, 8));
    TEST_ASSERT_TRUE(pngFits(600, 4, 8));    // gray+alpha pitch 1200
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_box_union);
    RUN_TEST(test_fit_keeps_native_when_it_fits);
    RUN_TEST(test_fit_downscales_tall_keeping_aspect);
    RUN_TEST(test_fit_downscales_wide);
    RUN_TEST(test_fit_never_zero);
    RUN_TEST(test_src_index_maps_nearest);
    RUN_TEST(test_keep_every_fits_cap);
    RUN_TEST(test_kept_count_rounds_up);
    RUN_TEST(test_merged_durations);
    RUN_TEST(test_frame_at_cycles);
    RUN_TEST(test_frame_at_degenerate);
    RUN_TEST(test_png_fits_rgba_up_to_316px);
    RUN_TEST(test_png_fits_indexed_wide);
    RUN_TEST(test_png_fits_truecolor_and_gray_alpha);
    return UNITY_END();
}
