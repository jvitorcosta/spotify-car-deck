#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/ui/cjkfont.h"

void setUp() {}
void tearDown() {}

// Synthetic blob: [0x3042..0x3043] 16 px, [0xFF71..0xFF71] 8 px.
static std::vector<uint8_t> makeBlob() {
    std::vector<uint8_t> b = {'U', 'F', 'N', 'T', 2, 0};
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    const uint32_t hdr = 6 + 13 * 2;
    u32(0x3042); u32(0x3043); b.push_back(16); u32(hdr);
    u32(0xFF71); u32(0xFF71); b.push_back(8);  u32(hdr + 64);
    for (int i = 0; i < 64; ++i) b.push_back((uint8_t)(0xA0 + i));   // two 16-px glyphs
    for (int i = 0; i < 16; ++i) b.push_back((uint8_t)(0x10 + i));   // one 8-px glyph
    return b;
}

void test_open_and_lookup() {
    std::vector<uint8_t> b = makeBlob();
    cjkfont::Font f;
    TEST_ASSERT_TRUE(f.open(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(2, f.rangeCount());
    int w = 0;
    const uint8_t* g = f.glyph(0x3042, &w);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_INT(16, w);
    TEST_ASSERT_EQUAL_HEX8(0xA0, g[0]);
    g = f.glyph(0x3043, &w);
    TEST_ASSERT_EQUAL_HEX8(0xA0 + 32, g[0]);
    g = f.glyph(0xFF71, &w);
    TEST_ASSERT_EQUAL_INT(8, w);
    TEST_ASSERT_EQUAL_HEX8(0x10, g[0]);
}
void test_missing_codepoints() {
    std::vector<uint8_t> b = makeBlob();
    cjkfont::Font f;
    f.open(b.data(), b.size());
    int w = 99;
    TEST_ASSERT_NULL(f.glyph(0x3044, &w));
    TEST_ASSERT_NULL(f.glyph(0x41, &w));
    TEST_ASSERT_NULL(f.glyph(0x1F600, &w));
}
void test_rejects_bad_blobs() {
    std::vector<uint8_t> b = makeBlob();
    cjkfont::Font f;
    std::vector<uint8_t> bad = b; bad[0] = 'X';
    TEST_ASSERT_FALSE(f.open(bad.data(), bad.size()));                 // magic
    TEST_ASSERT_FALSE(f.open(b.data(), b.size() - 1));                 // truncated bitmaps
    TEST_ASSERT_FALSE(f.open(b.data(), 10));                           // truncated header
    std::vector<uint8_t> unsorted = b;
    unsorted[6] = 0x72; unsorted[7] = 0xFF; unsorted[10] = 0x72; unsorted[11] = 0xFF;   // r0 = FF72
    TEST_ASSERT_FALSE(f.open(unsorted.data(), unsorted.size()));
    TEST_ASSERT_FALSE(f.open(nullptr, 0));
    int w;
    TEST_ASSERT_NULL(f.glyph(0x3042, &w));                             // closed font: no glyphs
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_open_and_lookup);
    RUN_TEST(test_missing_codepoints);
    RUN_TEST(test_rejects_bad_blobs);
    return UNITY_END();
}
