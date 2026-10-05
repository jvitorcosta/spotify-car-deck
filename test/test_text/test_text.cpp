#include <unity.h>
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
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_passthrough);
    RUN_TEST(test_portuguese_lower);
    RUN_TEST(test_portuguese_mixed);
    RUN_TEST(test_uppercase_accent);
    return UNITY_END();
}
