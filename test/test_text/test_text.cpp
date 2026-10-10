#include <unity.h>
#include <cstring>
#include "../../src/util/text.h"

void setUp() {}
void tearDown() {}

// asciiFold, and foldMarks without a marks array, give the same ASCII.
void test_ascii_fold() {
    const struct { const char* in; const char* out; } cases[] = {
        {"Hello World", "Hello World"},                   // ASCII passes through
        {"Cora\xC3\xA7\xC3\xA3o", "Coracao"},             // "Coração"
        {"N\xC3\xA3o \xC3\xA9", "Nao e"},                 // "Não é"
        {"\xC3\x89" "POCA", "EPOCA"},                     // "ÉPOCA"
    };
    for (const auto& c : cases) {
        char a[32], b[32];
        txt::asciiFold(c.in, a, sizeof(a));
        TEST_ASSERT_EQUAL_STRING(c.out, a);
        txt::foldMarks(c.in, b, nullptr, sizeof(b));
        TEST_ASSERT_EQUAL_STRING(c.out, b);
    }
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
// Text cut mid-character at a buffer edge (lyric lines, txt::copy) must stop at the terminator,
// never skip past it. The bytes after the NUL must not show up.
void test_fold_marks_stops_at_a_cut_character() {
    const char cut3[] = "ab\xE3" "\0" "XYZ";       // a 3-byte character cut after its lead byte
    const char cut4[] = "ab\xF0\x9F" "\0" "XYZ";   // a 4-byte character cut after 2 bytes
    char out[16];
    txt::foldMarks(cut3, out, nullptr, sizeof out);
    TEST_ASSERT_EQUAL_STRING("ab", out);
    txt::foldMarks(cut4, out, nullptr, sizeof out);
    TEST_ASSERT_EQUAL_STRING("ab", out);
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
void test_copy_truncates_and_terminates() {
    char b[4] = "zzz";
    txt::copy(b, "abcdef", sizeof(b));
    TEST_ASSERT_EQUAL_STRING("abc", b);
    txt::copy(b, "ok", sizeof(b));
    TEST_ASSERT_EQUAL_STRING("ok", b);
    txt::copy(b, nullptr, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("", b);
    char one[1] = {'x'};
    txt::copy(one, "abc", 1);                 // room for the NUL only
    TEST_ASSERT_EQUAL_CHAR('\0', one[0]);
}
void test_equals_ignore_case() {
    TEST_ASSERT_TRUE(txt::equalsIgnoreCase("Smartphone", "smartphone"));
    TEST_ASSERT_TRUE(txt::equalsIgnoreCase("TV", "tv"));
    TEST_ASSERT_FALSE(txt::equalsIgnoreCase("tv", "tvs"));
    TEST_ASSERT_FALSE(txt::equalsIgnoreCase("tvs", "tv"));
    TEST_ASSERT_FALSE(txt::equalsIgnoreCase(nullptr, "tv"));
    TEST_ASSERT_FALSE(txt::equalsIgnoreCase("tv", nullptr));
    TEST_ASSERT_TRUE(txt::equalsIgnoreCase("", ""));
}
void test_concat_all_or_nothing() {
    char b[12] = "old";
    TEST_ASSERT_TRUE(txt::concat(b, sizeof(b), "Bearer ", "abcd"));
    TEST_ASSERT_EQUAL_STRING("Bearer abcd", b);              // 11 chars + NUL: exactly fits
    TEST_ASSERT_FALSE(txt::concat(b, sizeof(b), "Bearer ", "abcde"));
    TEST_ASSERT_EQUAL_STRING("", b);                         // too long: empty, never truncated
    TEST_ASSERT_FALSE(txt::concat(b, sizeof(b), "Bearer ", nullptr));
    TEST_ASSERT_EQUAL_STRING("", b);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_copy_truncates_and_terminates);
    RUN_TEST(test_equals_ignore_case);
    RUN_TEST(test_concat_all_or_nothing);
    RUN_TEST(test_url_encode_reserved_and_utf8);
    RUN_TEST(test_url_encode_truncates_whole_escapes);
    RUN_TEST(test_copy_id_short_is_copied_verbatim);
    RUN_TEST(test_copy_id_long_ids_stay_distinct);
    RUN_TEST(test_copy_id_null_is_empty);
    RUN_TEST(test_ascii_fold);
    RUN_TEST(test_fold_marks_portuguese);
    RUN_TEST(test_fold_marks_upper_and_others);
    RUN_TEST(test_fold_marks_two_letter_folds_have_no_mark);
    RUN_TEST(test_fold_marks_stops_at_a_cut_character);
    return UNITY_END();
}
