#include <unity.h>
#include <cstdio>
#include <cstring>
#include "../../src/util/genre.h"

void setUp() {}
void tearDown() {}

// Real shape of an iTunes Search API artist answer (country=BR).
static const char* DANGELO =
    "{\n \"resultCount\":1,\n \"results\": [\n{\"wrapperType\":\"artist\", \"artistType\":\"Artist\", "
    "\"artistName\":\"D'Angelo\", \"artistLinkUrl\":\"https://music.apple.com/br/artist/dangelo/185580?uo=4\", "
    "\"artistId\":185580, \"amgArtistId\":127412, \"primaryGenreName\":\"R&B/soul\", \"primaryGenreId\":15}]\n}\n";

void test_parse_real_answer() {
    uint32_t id = 0;
    TEST_ASSERT_TRUE(genre::parsePrimaryGenreId(DANGELO, &id));
    TEST_ASSERT_EQUAL_UINT32(15, id);
}
void test_parse_large_ids_and_spacing() {
    uint32_t id = 0;
    TEST_ASSERT_TRUE(genre::parsePrimaryGenreId("{\"primaryGenreId\" : 50000063 }", &id));
    TEST_ASSERT_EQUAL_UINT32(50000063, id);
    TEST_ASSERT_TRUE(genre::parsePrimaryGenreId("{\"primaryGenreId\":1122,\"x\":1}", &id));
    TEST_ASSERT_EQUAL_UINT32(1122, id);
}
void test_parse_no_result_is_false() {
    uint32_t id = 7;
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\n \"resultCount\":0,\n \"results\": []\n}\n", &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("", &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId(nullptr, &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\"primaryGenreId\":\"x\"}", &id));
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\"primaryGenreId\":null}", &id));
    TEST_ASSERT_EQUAL_UINT32(7, id);                 // untouched on failure
}
void test_parse_rejects_cut_off_number() {
    uint32_t id = 0;
    // The 1 KB buffer could end inside the number: "11" of "1122" is not a genre.
    TEST_ASSERT_FALSE(genre::parsePrimaryGenreId("{\"primaryGenreName\":\"Brasileira\", \"primaryGenreId\":11", &id));
}
void test_cache_miss_then_hit() {
    genre::Cache c;
    uint8_t b = 0;
    TEST_ASSERT_FALSE(c.find("D'Angelo", &b));
    c.store("D'Angelo", 4);
    TEST_ASSERT_TRUE(c.find("D'Angelo", &b));
    TEST_ASSERT_EQUAL_UINT8(4, b);
    c.store("D'Angelo", 5);                          // re-store updates, no duplicate
    TEST_ASSERT_TRUE(c.find("D'Angelo", &b));
    TEST_ASSERT_EQUAL_UINT8(5, b);
}
void test_cache_remembers_none() {
    genre::Cache c;
    uint8_t b = 0;
    c.store("Unknown Band", genre::NONE);
    TEST_ASSERT_TRUE(c.find("Unknown Band", &b));
    TEST_ASSERT_EQUAL_UINT8(genre::NONE, b);
}
void test_cache_empty_artist_is_known_none() {
    genre::Cache c;
    uint8_t b = 0;
    TEST_ASSERT_TRUE(c.find("", &b));
    TEST_ASSERT_EQUAL_UINT8(genre::NONE, b);
    TEST_ASSERT_TRUE(c.find(nullptr, &b));
    TEST_ASSERT_EQUAL_UINT8(genre::NONE, b);
}
void test_cache_evicts_oldest_when_full() {
    genre::Cache c;
    char name[16];
    for (int i = 0; i < genre::Cache::N; ++i) {
        snprintf(name, sizeof(name), "artist %d", i);
        c.store(name, (uint8_t)(i % 29));
    }
    uint8_t b = 0;
    TEST_ASSERT_TRUE(c.find("artist 0", &b));
    c.store("newcomer", 3);
    TEST_ASSERT_FALSE(c.find("artist 0", &b));       // oldest replaced
    TEST_ASSERT_TRUE(c.find("artist 1", &b));
    TEST_ASSERT_TRUE(c.find("newcomer", &b));
    TEST_ASSERT_EQUAL_UINT8(3, b);
}
// Final review (re-graded Important): only a complete answer may say "no genre" (cached for
// the session); a body cut off by a dropped connection is a network error (not cached).
void test_answer_found() {
    uint32_t id = 0;
    TEST_ASSERT_EQUAL_INT((int)genre::Answer::Found, (int)genre::readAnswer(DANGELO, &id));
    TEST_ASSERT_EQUAL_UINT32(15, id);
}
void test_answer_complete_without_genre_is_no_match() {
    uint32_t id = 0;
    TEST_ASSERT_EQUAL_INT((int)genre::Answer::NoMatch,
                          (int)genre::readAnswer("{\n \"resultCount\":0,\n \"results\": []\n}\n", &id));
    TEST_ASSERT_EQUAL_INT((int)genre::Answer::NoMatch,
                          (int)genre::readAnswer("{\"resultCount\":1,\"results\":[{\"artistName\":\"X\"}]}", &id));
}
void test_answer_cut_off_is_incomplete() {
    uint32_t id = 0;
    // Connection dropped before primaryGenreId (the last key in Apple's object).
    TEST_ASSERT_EQUAL_INT((int)genre::Answer::Incomplete,
                          (int)genre::readAnswer("{\n \"resultCount\":1,\n \"results\": [\n{\"wrapperType\":\"artist\", \"artistName\":\"D'Ang", &id));
    TEST_ASSERT_EQUAL_INT((int)genre::Answer::Incomplete, (int)genre::readAnswer("", &id));
    TEST_ASSERT_EQUAL_INT((int)genre::Answer::Incomplete, (int)genre::readAnswer(nullptr, &id));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_real_answer);
    RUN_TEST(test_parse_large_ids_and_spacing);
    RUN_TEST(test_parse_no_result_is_false);
    RUN_TEST(test_parse_rejects_cut_off_number);
    RUN_TEST(test_cache_miss_then_hit);
    RUN_TEST(test_cache_remembers_none);
    RUN_TEST(test_cache_empty_artist_is_known_none);
    RUN_TEST(test_cache_evicts_oldest_when_full);
    RUN_TEST(test_answer_found);
    RUN_TEST(test_answer_complete_without_genre_is_no_match);
    RUN_TEST(test_answer_cut_off_is_incomplete);
    return UNITY_END();
}
