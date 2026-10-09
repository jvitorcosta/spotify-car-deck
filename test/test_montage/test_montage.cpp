#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/ui/montage.h"

void setUp() {}
void tearDown() {}

static void u16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static void u32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }

// "MTG1", count, fps, index, payloads
static std::vector<uint8_t> blob(const std::vector<std::vector<uint8_t>>& frames, uint16_t fps = 10) {
    std::vector<uint8_t> b = {'M', 'T', 'G', '1'};
    u16(b, (uint16_t)frames.size());
    u16(b, fps);
    uint32_t off = 8 + 8 * (uint32_t)frames.size();
    for (auto& f : frames) { u32(b, off); u32(b, (uint32_t)f.size()); off += (uint32_t)f.size(); }
    for (auto& f : frames) b.insert(b.end(), f.begin(), f.end());
    return b;
}

void test_parses_two_frames() {
    auto b = blob({{1, 2, 3}, {9, 8}});
    TEST_ASSERT_TRUE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(2, montage::count());
    TEST_ASSERT_EQUAL_INT(10, montage::fps());
    const uint8_t* j = nullptr;
    size_t n = 0;
    TEST_ASSERT_TRUE(montage::frame(1, &j, &n));
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)n);
    TEST_ASSERT_EQUAL_UINT8(9, j[0]);
    TEST_ASSERT_EQUAL_UINT8(8, j[1]);
}
void test_out_of_range_index() {
    auto b = blob({{1, 2, 3}});
    TEST_ASSERT_TRUE(montage::parse(b.data(), b.size()));
    const uint8_t* j;
    size_t n;
    TEST_ASSERT_FALSE(montage::frame(-1, &j, &n));
    TEST_ASSERT_FALSE(montage::frame(1, &j, &n));
}
// Review Focus 1: the build fallback writes an empty montage.
void test_empty_montage() {
    auto b = blob({});
    TEST_ASSERT_TRUE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
// Review Focus 2: corrupt files count as no montage.
void test_bad_magic_is_rejected() {
    auto b = blob({{1}});
    b[0] = 'X';
    TEST_ASSERT_FALSE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
void test_offset_past_end_is_rejected() {
    auto b = blob({{1, 2, 3}});
    b.pop_back();                                        // payload one byte short
    TEST_ASSERT_FALSE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
void test_truncated_header() {
    auto b = blob({{1}, {2}});
    TEST_ASSERT_FALSE(montage::parse(b.data(), 12));    // index cut off
    TEST_ASSERT_FALSE(montage::parse(b.data(), 3));
    TEST_ASSERT_FALSE(montage::parse(nullptr, 0));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
void test_zero_fps_is_rejected() {
    auto b = blob({{1}}, 0);
    TEST_ASSERT_FALSE(montage::parse(b.data(), b.size()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_two_frames);
    RUN_TEST(test_out_of_range_index);
    RUN_TEST(test_empty_montage);
    RUN_TEST(test_bad_magic_is_rejected);
    RUN_TEST(test_offset_past_end_is_rejected);
    RUN_TEST(test_truncated_header);
    RUN_TEST(test_zero_fps_is_rejected);
    return UNITY_END();
}
