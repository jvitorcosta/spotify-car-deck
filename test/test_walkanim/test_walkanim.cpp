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
void test_fit_band() {
    const struct { int bw, bh, w, h; } cases[] = {   // in a 32-high, 64-wide slot
        {24, 30, 24, 30},    // fits: kept native
        {40, 64, 20, 32},    // tall: downscaled keeping aspect
        {128, 32, 64, 16},   // wide
        {1, 200, 1, 32},     // a side never rounds to zero
    };
    for (const auto& c : cases) {
        const Fit f = fitBand(c.bw, c.bh, 32, 64);
        TEST_ASSERT_EQUAL_INT(c.w, f.w);
        TEST_ASSERT_EQUAL_INT(c.h, f.h);
    }
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
// Long frames (from the XML) saturate at the uint16 limit instead of wrapping to a short one.
void test_merged_durations_saturate() {
    const uint16_t ticks[2] = {60000, 60000};
    uint16_t ms[1];
    mergedDurationsMs(ticks, 2, 2, ms);
    TEST_ASSERT_EQUAL_UINT16(65535, ms[0]);
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
void test_png_fits() {
    const struct { int width, pixelType, bpp; bool fits; } cases[] = {
        {128, 6, 8, true},    // RGBA pitch 512
        {316, 6, 8, true},    // pitch 1264 -> 2560
        {317, 6, 8, false},   // pitch 1268 -> 2568
        {320, 6, 8, false},
        {512, 3, 8, true},    // indexed pitch 512
        {512, 3, 4, true},    // pitch 256
        {400, 2, 8, true},    // RGB pitch 1200 -> 2432
        {500, 2, 8, false},
        {600, 4, 8, true},    // gray+alpha pitch 1200
    };
    for (const auto& c : cases) TEST_ASSERT_EQUAL(c.fits, pngFits(c.width, c.pixelType, c.bpp));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_box_union);
    RUN_TEST(test_fit_band);
    RUN_TEST(test_src_index_maps_nearest);
    RUN_TEST(test_keep_every_fits_cap);
    RUN_TEST(test_kept_count_rounds_up);
    RUN_TEST(test_merged_durations);
    RUN_TEST(test_merged_durations_saturate);
    RUN_TEST(test_frame_at_cycles);
    RUN_TEST(test_frame_at_degenerate);
    RUN_TEST(test_png_fits);
    return UNITY_END();
}
