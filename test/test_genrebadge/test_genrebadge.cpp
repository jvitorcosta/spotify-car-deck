#include <unity.h>
#include <cstdio>
#include <cstring>
#include "../../src/ui/genrebadge.h"

void setUp() {}
void tearDown() {}

static const char* L(uint32_t id) { return genrebadge::at(genrebadge::forGenreId(id)).label; }

void test_genre_id_to_badge() {
    const struct { uint32_t id; const char* label; } cases[] = {
        // top-level genres
        {14, "POP"}, {18, "RAP"}, {15, "R&B"}, {21, "ROCK"}, {20, "ALT"}, {7, "ELECTRO"},
        {17, "DANCE"}, {1122, "BRASIL"}, {12, "LATIN"}, {100024, "LATIN"}, {24, "REGGAE"},
        {11, "JAZZ"}, {2, "JAZZ"}, {22, "GOSPEL"}, {5, "CLASSIC"}, {16, "OST"}, {6, "COUNTRY"},
        {10, "FOLK"}, {19, "WORLD"},
        // subgenres take the parent's badge
        {1004, "ALT"},       // Indie Rock
        {1146, "ROCK"},      // Arena rock
        {1073, "RAP"},       // Hip-hop
        {1050, "DANCE"},     // Techno
        {1263, "WORLD"},     // Bollywood: other regions stay WORLD
        {100026, "LATIN"},   // Cha-cha-cha
        {1224, "BRASIL"},    // Frevo
        // Africana and its subgenres get their own badge (Wizkid is Afrobeats 100051), not WORLD
        {1203, "AFRO"},      // Africana
        {100051, "AFRO"},    // Afrobeats
        {42, "AFRO"},        // Amapiano
        {1178, "AFRO"},      // Afro-pop
        // Chinese music gets one badge wherever Apple files it (own group, or under Pop/Rap/Rock/Alt)
        {1232, "CHINESE"},   // Chinês
        {1235, "CHINESE"},   // Chinese opera (subgenre)
        {1250, "CHINESE"},   // C-Pop (under Pop)
        {1251, "CHINESE"},   // Cantopop (under Pop)
        {1241, "CHINESE"},   // Chinese hip-hop (under Rap)
        {1248, "CHINESE"},   // Chinese rock (under Rock)
        {1230, "CHINESE"},   // Chinese alt (under Alternativo)
        {1243, "WORLD"},     // Coreano (traditional Korean): a neighbour, unchanged
        // overrides beat the parent's badge
        {51, "K-POP"}, {1139, "FUNK"}, {1153, "METAL"},
        {1152, "METAL"},     // Hard rock
        {1229, "FUNK BR"},   // Baile Funk, not BRASIL
        {1228, "SERTANEJO"}, {1225, "MPB"},
        {1221, "MPB"},       // Bossa nova
        {1227, "SAMBA"},
        {1222, "SAMBA"},     // Choro
        {1226, "PAGODE"}, {1223, "FORRO"}, {1220, "AXE"}, {27, "J-POP"},
        {30, "J-POP"},       // Kayokyoku
        {29, "OST"},         // Anime
    };
    for (const auto& c : cases) {
        char msg[16];
        snprintf(msg, sizeof msg, "id %u", (unsigned)c.id);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c.label, L(c.id), msg);
    }
}
void test_unknown_is_question_marks() {
    TEST_ASSERT_EQUAL_STRING("???", L(999999));
    TEST_ASSERT_EQUAL_STRING("???", L(0));
    TEST_ASSERT_EQUAL_STRING("???", L(3));         // Comedia: not a music badge
    TEST_ASSERT_EQUAL_INT(genrebadge::count() - 1, genrebadge::forGenreId(999999));
}
void test_badges_fit_and_colours_are_distinct() {
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
    RUN_TEST(test_genre_id_to_badge);
    RUN_TEST(test_unknown_is_question_marks);
    RUN_TEST(test_badges_fit_and_colours_are_distinct);
    RUN_TEST(test_table_is_sorted_and_complete);
    return UNITY_END();
}
