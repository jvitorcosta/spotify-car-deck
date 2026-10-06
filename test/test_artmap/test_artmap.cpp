#include <unity.h>
#include <cstring>
#include "../../src/util/artmap.h"

void setUp() {}
void tearDown() {}

void test_cover_square_maps_full_range() {
    artmap::Map m = artmap::cover(150, 150, 92, 92);
    TEST_ASSERT_EQUAL_INT(0, artmap::srcX(m, 0));
    TEST_ASSERT_EQUAL_INT(148, artmap::srcX(m, 91));
    TEST_ASSERT_EQUAL_INT(148, artmap::srcY(m, 91));
}
void test_cover_landscape_crops_center() {
    artmap::Map m = artmap::cover(200, 150, 92, 92);
    TEST_ASSERT_EQUAL_INT(25, artmap::srcX(m, 0));
    TEST_ASSERT_EQUAL_INT(0, artmap::srcY(m, 0));
    TEST_ASSERT_EQUAL_INT(25 + 91 * 150 / 92, artmap::srcX(m, 91));
}
void test_pick_scale_keeps_short_side_at_least_box() {
    TEST_ASSERT_EQUAL_INT(1, artmap::pickScale(300, 300, 92));   // 150 px
    TEST_ASSERT_EQUAL_INT(2, artmap::pickScale(640, 640, 92));   // 160 px
    TEST_ASSERT_EQUAL_INT(0, artmap::pickScale(64, 64, 92));     // smaller than the box
    TEST_ASSERT_EQUAL_INT(3, artmap::pickScale(2000, 1000, 92)); // 125 px short side
}
void test_plain_http_url() {
    char out[128];
    artmap::plainHttpUrl("https://i.scdn.co/image/abc", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("http://i.scdn.co/image/abc", out);
    artmap::plainHttpUrl("https://example.com/x.jpg", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://example.com/x.jpg", out);
    char tiny[8];
    artmap::plainHttpUrl("https://i.scdn.co/image/abc", tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_INT(7, (int)strlen(tiny));   // truncated, terminated
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cover_square_maps_full_range);
    RUN_TEST(test_cover_landscape_crops_center);
    RUN_TEST(test_pick_scale_keeps_short_side_at_least_box);
    RUN_TEST(test_plain_http_url);
    return UNITY_END();
}
