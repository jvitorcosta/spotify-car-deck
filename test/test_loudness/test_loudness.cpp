#include <unity.h>
#include <cstring>
#include "../../src/util/loudness.h"

void setUp() {}
void tearDown() {}

// Little-endian 16-bit samples into a byte buffer starting at `out` (may be unaligned).
static void put(uint8_t* out, const int16_t* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = (uint8_t)(s[i] & 0xFF);
        out[2 * i + 1] = (uint8_t)((uint16_t)s[i] >> 8);
    }
}

void test_silence_is_zero() {
    const int16_t s[4] = {0, 0, 0, 0};
    uint8_t b[8];
    put(b, s, 4);
    TEST_ASSERT_EQUAL_UINT8(0, loudness::loudness(b, 4, 0, 4));
}
void test_full_scale_is_255_both_polarities() {
    const int16_t pos[2] = {0, 32767}, neg[2] = {-32768, 0};
    uint8_t b[4];
    put(b, pos, 2);
    TEST_ASSERT_EQUAL_UINT8(255, loudness::loudness(b, 2, 0, 2));
    put(b, neg, 2);
    TEST_ASSERT_EQUAL_UINT8(255, loudness::loudness(b, 2, 0, 2));
}
void test_half_scale_is_128() {
    const int16_t s[1] = {16384};
    uint8_t b[2];
    put(b, s, 1);
    TEST_ASSERT_EQUAL_UINT8(128, loudness::loudness(b, 1, 0, 1));
}
// Only [at, at + window) counts: the loud sample outside the window is ignored.
void test_window_selects_samples() {
    const int16_t s[6] = {32767, 0, 1000, 0, 0, 0};
    uint8_t b[12];
    put(b, s, 6);
    TEST_ASSERT_EQUAL_UINT8(8, loudness::loudness(b, 6, 1, 3));   // peak 1000 -> 8
}
// A window running past the end is clamped; a start past the end gives 0.
void test_window_clamped_at_the_clip_end() {
    const int16_t s[3] = {0, 0, -32767};
    uint8_t b[6];
    put(b, s, 3);
    TEST_ASSERT_EQUAL_UINT8(255, loudness::loudness(b, 3, 2, 320));
    TEST_ASSERT_EQUAL_UINT8(0, loudness::loudness(b, 3, 3, 320));
    TEST_ASSERT_EQUAL_UINT8(0, loudness::loudness(b, 3, 99, 320));
}
// Embedded data has no alignment guarantee: an odd start address must read the same.
void test_odd_alignment() {
    const int16_t s[2] = {0, 16384};
    uint8_t raw[5];
    put(raw + 1, s, 2);
    TEST_ASSERT_EQUAL_UINT8(128, loudness::loudness(raw + 1, 2, 0, 2));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_silence_is_zero);
    RUN_TEST(test_full_scale_is_255_both_polarities);
    RUN_TEST(test_half_scale_is_128);
    RUN_TEST(test_window_selects_samples);
    RUN_TEST(test_window_clamped_at_the_clip_end);
    RUN_TEST(test_odd_alignment);
    return UNITY_END();
}
