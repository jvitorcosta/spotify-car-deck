#include <unity.h>
#include <cstring>
#include "../../src/ui/typebadge.h"

void setUp() {}
void tearDown() {}

static const char* TYPES[] = {"normal", "fire", "water", "electric", "grass", "ice", "fighting",
                              "poison", "ground", "flying", "psychic", "bug", "rock", "ghost",
                              "dragon", "dark", "steel", "fairy"};

void test_label_is_uppercase_type_name() {
    char b[16];
    TEST_ASSERT_TRUE(typebadge::label("grass", b, sizeof(b)));
    TEST_ASSERT_EQUAL_STRING("GRASS", b);
    TEST_ASSERT_TRUE(typebadge::label("Psychic", b, sizeof(b)));
    TEST_ASSERT_EQUAL_STRING("PSYCHIC", b);
}
void test_unknown_or_empty_type_has_no_badge() {
    char b[16] = "x";
    TEST_ASSERT_FALSE(typebadge::label("", b, sizeof(b)));
    TEST_ASSERT_FALSE(typebadge::label(nullptr, b, sizeof(b)));
    TEST_ASSERT_FALSE(typebadge::label("shadow", b, sizeof(b)));   // not one of the 18
    TEST_ASSERT_EQUAL_STRING("", b);
}
void test_every_label_fits_inside_the_badge() {
    char b[16];
    for (const char* t : TYPES) {
        TEST_ASSERT_TRUE(typebadge::label(t, b, sizeof(b)));
        int textW = (int)strlen(b) * typebadge::CHAR_W - 1;   // font 1: 6 px advance, last gap dropped
        TEST_ASSERT_TRUE(textW <= typebadge::W - 4);          // 2 px padding each side
    }
}
void test_small_buffer_is_rejected_not_overflowed() {
    char b[4] = "zz";
    TEST_ASSERT_FALSE(typebadge::label("electric", b, sizeof(b)));
    TEST_ASSERT_EQUAL_STRING("", b);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_label_is_uppercase_type_name);
    RUN_TEST(test_unknown_or_empty_type_has_no_badge);
    RUN_TEST(test_every_label_fits_inside_the_badge);
    RUN_TEST(test_small_buffer_is_rejected_not_overflowed);
    return UNITY_END();
}
