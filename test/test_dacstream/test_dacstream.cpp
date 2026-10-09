#include <unity.h>
#include <cstdint>
#include <vector>
#include "../../src/util/dacstream.h"

using namespace dacstream;

void setUp() {}
void tearDown() {}

// The pre-refactor loop from src/audio/greeting.cpp (commit 19b19a1), kept verbatim as the reference.
static int32_t refSample(size_t i, const int16_t* clip, size_t len, int volume, int ramp) {
    if (i < (size_t)ramp) return MID * (int32_t)i / ramp;
    i -= ramp;
    if (i < len) return MID + (int32_t)clip[i] * volume / 100;
    i -= len;
    if (i < (size_t)ramp) return MID * (int32_t)(ramp - i) / ramp;
    return 0;
}

static std::vector<uint16_t> reference(const int16_t* clip, size_t len, int volume, int ramp) {
    std::vector<uint16_t> out;
    const size_t total = (ramp + len + ramp) * 2;
    int32_t err = 0;
    for (size_t j = 0; j < total; ++j) {
        const size_t i = j / 2;
        int32_t v = refSample(i, clip, len, volume, ramp);
        if (j & 1) v = (v + refSample(i + 1, clip, len, volume, ramp)) / 2;
        int32_t want = v + err;
        int32_t q = (want < 0 ? 0 : want > 65535 ? 65535 : want) & 0xFF00;
        err = want - q;
        out.push_back((uint16_t)q);
    }
    return out;
}

static std::vector<uint16_t> encoded(const int16_t* clip, size_t len, int volume, int ramp) {
    std::vector<uint16_t> out;
    Encoder e;
    uint16_t w[2];
    auto feed = [&](int32_t v) { int n = e.push(v, w); for (int k = 0; k < n; ++k) out.push_back(w[k]); };
    for (int i = 0; i < ramp; ++i) feed(rampUp(i, ramp));
    for (size_t k = 0; k < len; ++k) feed(MID + (int32_t)clip[k] * volume / 100);
    for (int i = 0; i < ramp; ++i) feed(rampDown(i, ramp));
    int n = e.finish(w);
    for (int k = 0; k < n; ++k) out.push_back(w[k]);
    return out;
}

static std::vector<int16_t> testClip(size_t len) {
    std::vector<int16_t> c(len);
    uint32_t x = 12345;
    for (size_t i = 0; i < len; ++i) { x = x * 1103515245u + 12345u; c[i] = (int16_t)(x >> 16); }
    if (len > 2) { c[0] = 32767; c[1] = -32768; }          // extremes clip in the shaper
    return c;
}

void test_matches_the_old_loop_at_each_volume() {
    const std::vector<int16_t> clip = testClip(3000);
    for (int volume : {30, 12, 100}) {
        const auto a = reference(clip.data(), clip.size(), volume, 320);
        const auto b = encoded(clip.data(), clip.size(), volume, 320);
        TEST_ASSERT_EQUAL_size_t(a.size(), b.size());
        TEST_ASSERT_EQUAL_UINT16_ARRAY(a.data(), b.data(), a.size());
    }
}

void test_empty_clip_is_just_the_ramps() {
    const auto a = reference(nullptr, 0, 30, 320);
    const auto b = encoded(nullptr, 0, 30, 320);
    TEST_ASSERT_EQUAL_size_t(a.size(), b.size());
    TEST_ASSERT_EQUAL_UINT16_ARRAY(a.data(), b.data(), a.size());
}

void test_first_push_waits_for_the_next_sample() {
    Encoder e;
    uint16_t w[2];
    TEST_ASSERT_EQUAL_INT(0, e.push(MID, w));
    TEST_ASSERT_EQUAL_INT(2, e.push(MID, w));
}

void test_finish_without_samples_writes_nothing() {
    Encoder e;
    uint16_t w[2];
    TEST_ASSERT_EQUAL_INT(0, e.finish(w));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_matches_the_old_loop_at_each_volume);
    RUN_TEST(test_empty_clip_is_just_the_ramps);
    RUN_TEST(test_first_push_waits_for_the_next_sample);
    RUN_TEST(test_finish_without_samples_writes_nothing);
    return UNITY_END();
}
