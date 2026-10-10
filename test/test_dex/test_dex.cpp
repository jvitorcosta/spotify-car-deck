#include <unity.h>
#include <cstring>
#include "../../src/pokemon/dex.h"

void setUp() {}
void tearDown() {}

void test_spot_check_names_and_types() {
    const struct { int n; const char* name; const char* type; } cases[] = {
        {1, "Bulbasaur", "grass"},
        {25, "Pikachu", "electric"},
        {1025, "Pecharunt", "poison"},   // the last entry
    };
    for (const auto& c : cases) {
        TEST_ASSERT_EQUAL_STRING(c.name, dex::name(c.n));
        TEST_ASSERT_EQUAL_STRING(c.type, dex::type(c.n));
    }
}
void test_names_are_folded_to_ascii() {
    TEST_ASSERT_EQUAL_STRING("Nidoran F", dex::name(29));
    TEST_ASSERT_EQUAL_STRING("Nidoran M", dex::name(32));
    TEST_ASSERT_EQUAL_STRING("Farfetch'd", dex::name(83));
    TEST_ASSERT_EQUAL_STRING("Mr. Mime", dex::name(122));
    TEST_ASSERT_EQUAL_STRING("Flabebe", dex::name(669));
}
void test_out_of_range_is_empty() {
    TEST_ASSERT_EQUAL_STRING("", dex::name(0));
    TEST_ASSERT_EQUAL_STRING("", dex::name(1026));
    TEST_ASSERT_EQUAL_STRING("", dex::type(-5));
}
void test_every_entry_is_printable_ascii_and_fits_state() {
    for (int n = 1; n <= dex::COUNT; ++n) {
        const char* s = dex::name(n);
        TEST_ASSERT_TRUE_MESSAGE(s[0] != 0, "empty name");
        TEST_ASSERT_TRUE_MESSAGE(strlen(s) < 24, "longer than AppState::pokeName");
        for (const char* p = s; *p; ++p) TEST_ASSERT_TRUE(*p >= 0x20 && *p <= 0x7E);
        TEST_ASSERT_TRUE(dex::type(n)[0] != 0);
    }
}
void test_from_random_covers_range() {
    TEST_ASSERT_EQUAL_INT(1, dex::fromRandom(0));
    TEST_ASSERT_EQUAL_INT(1025, dex::fromRandom(1024));
    TEST_ASSERT_EQUAL_INT(1, dex::fromRandom(1025));
    int n = dex::fromRandom(0xFFFFFFFFu);
    TEST_ASSERT_TRUE(n >= 1 && n <= dex::COUNT);
}
void test_sprite_url() {
    char url[160];
    dex::spriteUrl(25, url, sizeof(url));
    TEST_ASSERT_EQUAL_STRING(
        "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/25.png", url);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_spot_check_names_and_types);
    RUN_TEST(test_names_are_folded_to_ascii);
    RUN_TEST(test_out_of_range_is_empty);
    RUN_TEST(test_every_entry_is_printable_ascii_and_fits_state);
    RUN_TEST(test_from_random_covers_range);
    RUN_TEST(test_sprite_url);
    return UNITY_END();
}
