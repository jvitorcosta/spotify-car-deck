#include <unity.h>
#include <cstring>
#include "../../src/util/text.h"

void setUp() {}
void tearDown() {}

void test_ascii_passthrough() {
    char o[32];
    txt::asciiFold("Hello World", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("Hello World", o);
}
void test_portuguese_lower() {
    char o[32];
    txt::asciiFold("Cora\xC3\xA7\xC3\xA3o", o, sizeof(o));   // "Coração"
    TEST_ASSERT_EQUAL_STRING("Coracao", o);
}
void test_portuguese_mixed() {
    char o[32];
    txt::asciiFold("N\xC3\xA3o \xC3\xA9", o, sizeof(o));      // "Não é"
    TEST_ASSERT_EQUAL_STRING("Nao e", o);
}
void test_uppercase_accent() {
    char o[32];
    txt::asciiFold("\xC3\x89""POCA", o, sizeof(o));           // "ÉPOCA"
    TEST_ASSERT_EQUAL_STRING("EPOCA", o);
}
void test_fold_marks_portuguese() {
    char o[32];
    txt::Mark m[32];
    size_t n = txt::foldMarks("Cora\xC3\xA7\xC3\xA3o", o, m, sizeof(o));   // "Coração"
    TEST_ASSERT_EQUAL_STRING("Coracao", o);
    TEST_ASSERT_EQUAL_INT(7, (int)n);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::None, (int)m[3]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Cedilla, (int)m[4]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Tilde, (int)m[5]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::None, (int)m[6]);
}
void test_fold_marks_upper_and_others() {
    char o[32];
    txt::Mark m[32];
    txt::foldMarks("\xC3\x89""POCA voc\xC3\xAA \xC3\xA0 n\xC3\xA3o", o, m, sizeof(o));   // "ÉPOCA você à não"
    TEST_ASSERT_EQUAL_STRING("EPOCA voce a nao", o);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Acute, (int)m[0]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Circumflex, (int)m[9]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Grave, (int)m[11]);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Tilde, (int)m[14]);
}
void test_fold_marks_two_letter_folds_have_no_mark() {
    char o[8];
    txt::Mark m[8];
    txt::foldMarks("\xC3\x86\xC3\x9F", o, m, sizeof(o));   // "Æß"
    TEST_ASSERT_EQUAL_STRING("AEss", o);
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT((int)txt::Mark::None, (int)m[i]);
}
void test_fold_marks_null_marks_matches_ascii_fold() {
    char a[32], b[32];
    txt::asciiFold("N\xC3\xA3o \xC3\xA9", a, sizeof(a));
    txt::foldMarks("N\xC3\xA3o \xC3\xA9", b, nullptr, sizeof(b));
    TEST_ASSERT_EQUAL_STRING(a, b);
}
void test_copy_id_short_is_copied_verbatim() {
    char o[64];
    txt::copyId("spotify:track:4uLU6hMCjMI75M1A2tKUQC", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("spotify:track:4uLU6hMCjMI75M1A2tKUQC", o);
}
void test_copy_id_long_ids_stay_distinct() {
    // Local files: spotify:local:<artist>:<album>:<title>:<seconds>; same album -> same prefix.
    const char* a = "spotify:local:Some+Long+Artist+Name:Some+Long+Album+Name:Track+One:215";
    const char* b = "spotify:local:Some+Long+Artist+Name:Some+Long+Album+Name:Track+Two:198";
    char oa[64], ob[64];
    txt::copyId(a, oa, sizeof(oa));
    txt::copyId(b, ob, sizeof(ob));
    TEST_ASSERT_TRUE(strlen(oa) < sizeof(oa));
    TEST_ASSERT_TRUE(strcmp(oa, ob) != 0);
    char again[64];
    txt::copyId(a, again, sizeof(again));
    TEST_ASSERT_EQUAL_STRING(oa, again);   // stable: same id -> same key
}
void test_copy_id_null_is_empty() {
    char o[8] = "x";
    txt::copyId(nullptr, o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("", o);
}
void test_url_encode_reserved_and_utf8() {
    char o[128];
    TEST_ASSERT_EQUAL_INT(35, (int)txt::urlEncode("S. Kiyotaka & Omega Tribe", o, sizeof(o)));
    TEST_ASSERT_EQUAL_STRING("S.%20Kiyotaka%20%26%20Omega%20Tribe", o);
    txt::urlEncode("Racionais MC's", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("Racionais%20MC%27s", o);
    txt::urlEncode("Mar\xC3\xADlia", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("Mar%C3%ADlia", o);
    txt::urlEncode("a-b_c.d~e", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("a-b_c.d~e", o);         // unreserved kept
}
void test_url_encode_truncates_whole_escapes() {
    char o[6];                                       // room for 5 chars
    txt::urlEncode("ab&cd", o, sizeof(o));
    TEST_ASSERT_EQUAL_STRING("ab%26", o);
    char p[5];                                       // room for 4: "%26" no longer fits after "ab"
    txt::urlEncode("ab&cd", p, sizeof(p));
    TEST_ASSERT_EQUAL_STRING("ab", p);
    txt::urlEncode(nullptr, p, sizeof(p));
    TEST_ASSERT_EQUAL_STRING("", p);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_url_encode_reserved_and_utf8);
    RUN_TEST(test_url_encode_truncates_whole_escapes);
    RUN_TEST(test_copy_id_short_is_copied_verbatim);
    RUN_TEST(test_copy_id_long_ids_stay_distinct);
    RUN_TEST(test_copy_id_null_is_empty);
    RUN_TEST(test_ascii_passthrough);
    RUN_TEST(test_portuguese_lower);
    RUN_TEST(test_portuguese_mixed);
    RUN_TEST(test_uppercase_accent);
    RUN_TEST(test_fold_marks_portuguese);
    RUN_TEST(test_fold_marks_upper_and_others);
    RUN_TEST(test_fold_marks_two_letter_folds_have_no_mark);
    RUN_TEST(test_fold_marks_null_marks_matches_ascii_fold);
    return UNITY_END();
}
