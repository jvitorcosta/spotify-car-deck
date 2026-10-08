#include <unity.h>
#include <cstring>
#include "../../src/ui/genrebadge.h"
#include "../../src/ui/theme.h"

void setUp() {}
void tearDown() {}

static const char* L(uint32_t id) { return genrebadge::at(genrebadge::forGenreId(id)).label; }

void test_top_level_genres() {
    TEST_ASSERT_EQUAL_STRING("POP", L(14));
    TEST_ASSERT_EQUAL_STRING("RAP", L(18));
    TEST_ASSERT_EQUAL_STRING("R&B", L(15));
    TEST_ASSERT_EQUAL_STRING("ROCK", L(21));
    TEST_ASSERT_EQUAL_STRING("ALT", L(20));
    TEST_ASSERT_EQUAL_STRING("ELECTRO", L(7));
    TEST_ASSERT_EQUAL_STRING("DANCE", L(17));
    TEST_ASSERT_EQUAL_STRING("BRASIL", L(1122));
    TEST_ASSERT_EQUAL_STRING("LATIN", L(12));
    TEST_ASSERT_EQUAL_STRING("LATIN", L(100024));
    TEST_ASSERT_EQUAL_STRING("REGGAE", L(24));
    TEST_ASSERT_EQUAL_STRING("JAZZ", L(11));
    TEST_ASSERT_EQUAL_STRING("JAZZ", L(2));
    TEST_ASSERT_EQUAL_STRING("GOSPEL", L(22));
    TEST_ASSERT_EQUAL_STRING("CLASSIC", L(5));
    TEST_ASSERT_EQUAL_STRING("OST", L(16));
    TEST_ASSERT_EQUAL_STRING("COUNTRY", L(6));
    TEST_ASSERT_EQUAL_STRING("FOLK", L(10));
    TEST_ASSERT_EQUAL_STRING("WORLD", L(19));
}
void test_subgenres_take_parent_badge() {
    TEST_ASSERT_EQUAL_STRING("ALT", L(1004));      // Indie Rock
    TEST_ASSERT_EQUAL_STRING("ROCK", L(1146));     // Arena rock
    TEST_ASSERT_EQUAL_STRING("RAP", L(1073));      // Hip-hop
    TEST_ASSERT_EQUAL_STRING("DANCE", L(1050));    // Techno
    TEST_ASSERT_EQUAL_STRING("WORLD", L(1263));    // Bollywood
    TEST_ASSERT_EQUAL_STRING("LATIN", L(100026));  // Cha-cha-cha
    TEST_ASSERT_EQUAL_STRING("BRASIL", L(1224));   // Frevo
}
void test_overrides_beat_parent() {
    TEST_ASSERT_EQUAL_STRING("K-POP", L(51));
    TEST_ASSERT_EQUAL_STRING("FUNK", L(1139));
    TEST_ASSERT_EQUAL_STRING("METAL", L(1153));
    TEST_ASSERT_EQUAL_STRING("METAL", L(1152));    // Hard rock
    TEST_ASSERT_EQUAL_STRING("FUNK BR", L(1229));  // Baile Funk, not BRASIL
    TEST_ASSERT_EQUAL_STRING("SERTANEJO", L(1228));
    TEST_ASSERT_EQUAL_STRING("MPB", L(1225));
    TEST_ASSERT_EQUAL_STRING("MPB", L(1221));      // Bossa nova
    TEST_ASSERT_EQUAL_STRING("SAMBA", L(1227));
    TEST_ASSERT_EQUAL_STRING("SAMBA", L(1222));    // Choro
    TEST_ASSERT_EQUAL_STRING("PAGODE", L(1226));
    TEST_ASSERT_EQUAL_STRING("FORRO", L(1223));
    TEST_ASSERT_EQUAL_STRING("AXE", L(1220));
    TEST_ASSERT_EQUAL_STRING("J-POP", L(27));
    TEST_ASSERT_EQUAL_STRING("J-POP", L(30));      // Kayokyoku
    TEST_ASSERT_EQUAL_STRING("OST", L(29));        // Anime
}
void test_unknown_is_question_marks() {
    TEST_ASSERT_EQUAL_STRING("???", L(999999));
    TEST_ASSERT_EQUAL_STRING("???", L(0));
    TEST_ASSERT_EQUAL_STRING("???", L(3));         // Comedia: not a music badge
    TEST_ASSERT_EQUAL_INT(genrebadge::count() - 1, genrebadge::forGenreId(999999));
    TEST_ASSERT_EQUAL_HEX16(theme::rgb(0x88, 0x90, 0xA0), genrebadge::at(genrebadge::count() - 1).color);
}
void test_badges_fit_and_colours_are_distinct() {
    TEST_ASSERT_EQUAL_INT(29, genrebadge::count());
    for (int i = 0; i < genrebadge::count(); ++i) {
        const genrebadge::Badge& b = genrebadge::at(i);
        TEST_ASSERT_TRUE_MESSAGE(strlen(b.label) <= 9, b.label);
        TEST_ASSERT_TRUE((int)strlen(b.label) * 6 - 1 <= genrebadge::W - 4);
        for (int j = 0; j < i; ++j) TEST_ASSERT_NOT_EQUAL(genrebadge::at(j).color, b.color);
    }
}
void test_table_is_sorted_and_complete() {
    TEST_ASSERT_TRUE(genrebadge::idCount() > 400);
    for (int i = 1; i < genrebadge::idCount(); ++i)
        TEST_ASSERT_TRUE(genrebadge::idAt(i - 1) < genrebadge::idAt(i));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_top_level_genres);
    RUN_TEST(test_subgenres_take_parent_badge);
    RUN_TEST(test_overrides_beat_parent);
    RUN_TEST(test_unknown_is_question_marks);
    RUN_TEST(test_badges_fit_and_colours_are_distinct);
    RUN_TEST(test_table_is_sorted_and_complete);
    return UNITY_END();
}
