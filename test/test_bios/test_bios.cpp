#include <unity.h>
#include <cstring>
#include "../../src/util/bios.h"

using namespace bios;

void setUp() {}
void tearDown() {}

static int find(const char* label) {
    for (int i = 0; i < count(); ++i)
        if (strstr(line(i).label, label)) return i;
    return -1;
}

void test_script_is_in_time_order_and_ends_after_the_last_line() {
    for (int i = 1; i < count(); ++i) TEST_ASSERT_TRUE(line(i).at >= line(i - 1).at);
    TEST_ASSERT_TRUE(TOTAL_MS > (uint32_t)line(count() - 1).at + DOTS_MS);
}

void test_format_right_aligns_the_value_with_a_dot_leader() {
    char out[COLS + 1];
    const int v = format("CUP HOLDERS", "2/2 [OK]", out, sizeof out);
    TEST_ASSERT_EQUAL_INT(COLS, (int)strlen(out));
    TEST_ASSERT_EQUAL_STRING("2/2 [OK]", out + v);
    TEST_ASSERT_EQUAL_INT(0, strncmp(out, "CUP HOLDERS ...", 15));
    TEST_ASSERT_EQUAL_CHAR(' ', out[v - 1]);
}

void test_format_without_value_is_the_label() {
    char out[COLS + 1];
    TEST_ASSERT_EQUAL_INT(-1, format("Detecting occupants...", nullptr, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("Detecting occupants...", out);
}

void test_format_truncates_a_long_value_keeping_one_dot() {
    char out[COLS + 1];
    const int v = format("WI-FI", "A-VERY-LONG-NETWORK-NAME-THAT-DOES-NOT-FIT-AT-ALL [OK]", out, sizeof out);
    TEST_ASSERT_EQUAL_INT(COLS, (int)strlen(out));
    TEST_ASSERT_EQUAL_INT(0, strncmp(out, "WI-FI . ", 8));
    TEST_ASSERT_EQUAL_INT(8, v);
}

void test_format_to_a_narrower_width() {
    char out[TERM_COLS + 1];
    const int v = format("WI-FI", "Cecilia [OK]", out, sizeof out, TERM_COLS);
    TEST_ASSERT_EQUAL_INT(TERM_COLS, (int)strlen(out));
    TEST_ASSERT_EQUAL_STRING("Cecilia [OK]", out + v);
}

void test_term_appends_new_labels() {
    Term term = {};
    TEST_ASSERT_TRUE(termSet(term, "IGNITION", "START", OK, 0));
    TEST_ASSERT_TRUE(termSet(term, "ENGINE", "RUNNING", OK, 600));
    TEST_ASSERT_EQUAL_INT(2, term.n);
    TEST_ASSERT_EQUAL_STRING("ENGINE", term.rows[1].label);
    TEST_ASSERT_EQUAL_UINT32(600, term.rows[1].at);
}

void test_term_updates_a_label_in_place() {
    Term term = {};
    termSet(term, "WI-FI", "CONNECTING...", WARN, 100);
    const uint32_t gen = term.gen;
    TEST_ASSERT_FALSE(termSet(term, "WI-FI", "CONNECTING...", WARN, 200));   // nothing new
    TEST_ASSERT_EQUAL_UINT32(gen, term.gen);
    TEST_ASSERT_TRUE(termSet(term, "WI-FI", "Cecilia [OK]", OK, 900));
    TEST_ASSERT_EQUAL_INT(1, term.n);
    TEST_ASSERT_EQUAL_STRING("Cecilia [OK]", term.rows[0].value);
    TEST_ASSERT_EQUAL_UINT8(OK, term.rows[0].tone);
    TEST_ASSERT_EQUAL_UINT32(100, term.rows[0].at);                    // not typed again
    TEST_ASSERT_TRUE(term.gen != gen);
}

void test_term_scrolls_the_oldest_line_off() {
    Term term = {};
    const char* labels[] = {"A", "B", "C", "D", "E", "F", "G", "H"};
    static_assert(TERM_ROWS + 1 <= 8, "test labels");
    for (int i = 0; i <= TERM_ROWS; ++i) termSet(term, labels[i], "OK", OK, i * 100);
    TEST_ASSERT_EQUAL_INT(TERM_ROWS, term.n);
    TEST_ASSERT_EQUAL_STRING("B", term.rows[0].label);
    TEST_ASSERT_EQUAL_STRING(labels[TERM_ROWS], term.rows[TERM_ROWS - 1].label);
}

void test_script_lines_fit_the_terminal() {
    char out[COLS + 1];
    for (int i = 0; i < count(); ++i) {
        const Line& l = line(i);
        TEST_ASSERT_TRUE_MESSAGE(l.label[0] != 0, "no blank lines in the terminal script");
        if (l.live) continue;
        const int v = format(l.label, l.value, out, sizeof out, TERM_COLS);
        TEST_ASSERT_TRUE_MESSAGE((int)strlen(l.label) <= TERM_COLS, l.label);
        if (l.value) TEST_ASSERT_EQUAL_STRING_MESSAGE(l.value, out + v, l.label);   // not truncated
    }
}

void test_term_script_feeds_due_lines_once() {
    Term term = {};
    termScript(term, 0);
    TEST_ASSERT_EQUAL_STRING(line(0).label, term.rows[0].label);
    const int n0 = term.n;
    termScript(term, 0);
    TEST_ASSERT_EQUAL_INT(n0, term.n);                       // not added twice
    termExpire(term, 100000, 1000);                           // everything expires
    TEST_ASSERT_EQUAL_INT(0, term.n);
    termScript(term, 0);
    TEST_ASSERT_EQUAL_INT(0, term.n);                         // and doesn't come back
}

void test_term_script_skips_the_live_line() {
    const int wifi = find("WI-FI");
    TEST_ASSERT_TRUE(wifi >= 0);
    TEST_ASSERT_TRUE(line(wifi).live);                       // its value comes from the radio
    TEST_ASSERT_NULL(line(wifi).value);
    Term term = {};
    for (uint32_t t = 0; t <= TOTAL_MS; t += 50) {
        termScript(term, t);
        for (int i = 0; i < term.n; ++i) TEST_ASSERT_TRUE(strcmp(term.rows[i].label, "WI-FI") != 0);
    }
    TEST_ASSERT_TRUE(liveAt() > 0 && liveAt() < TOTAL_MS);
}

void test_plain_script_line_has_no_value() {
    Term term = {};
    termScript(term, 0);
    TEST_ASSERT_EQUAL_STRING("", term.rows[0].value);       // the header: plain text
    TEST_ASSERT_EQUAL_UINT8(HEADER, term.rows[0].tone);
}

void test_term_expire_keeps_fresh_and_pending_lines() {
    Term term = {};
    termSet(term, "OLD", "OK", OK, 0);
    termSet(term, "WAIT", "CONNECTING...", WARN, 0, true);
    termSet(term, "NEW", "OK", OK, 2500);
    const uint32_t gen = term.gen;
    termExpire(term, 3000, 3000);
    TEST_ASSERT_EQUAL_INT(2, term.n);
    TEST_ASSERT_EQUAL_STRING("WAIT", term.rows[0].label);
    TEST_ASSERT_EQUAL_STRING("NEW", term.rows[1].label);
    TEST_ASSERT_TRUE(term.gen != gen);
    termExpire(term, 3000, 3000);                             // nothing more to drop
    termSet(term, "WAIT", "Cecilia [OK]", OK, 4000);         // resolved: no longer pending
    termExpire(term, 7000, 3000);
    TEST_ASSERT_EQUAL_INT(0, term.n);
}

void test_term_update_restarts_the_expiry_clock() {
    Term term = {};
    termSet(term, "GREETING", "PLAYING", PURPLE, 0);
    termSet(term, "GREETING", "DONE", OK, 2000);
    termExpire(term, 4000, 3000);
    TEST_ASSERT_EQUAL_INT(1, term.n);
}

void test_term_value_is_truncated_to_fit() {
    Term term = {};
    termSet(term, "WI-FI", "A-NETWORK-NAME-LONGER-THAN-THE-BUFFER [OK]", OK, 0);
    TEST_ASSERT_EQUAL_INT(TERM_VAL - 1, (int)strlen(term.rows[0].value));
}

void test_typed_reveals_label_then_dots() {
    TEST_ASSERT_EQUAL_INT(5, typed(0, 5, 20));                // label at once
    TEST_ASSERT_EQUAL_INT(12, typed(DOTS_MS / 2, 5, 20));     // dots filling in: 5 + 15 / 2
    TEST_ASSERT_EQUAL_INT(20, typed(DOTS_MS, 5, 20));         // value shown
    TEST_ASSERT_EQUAL_INT(20, typed(DOTS_MS + 1000, 5, 20));  // and stays
    TEST_ASSERT_EQUAL_INT(9, typed(0, 9, 9));                 // plain text: whole at once
    // shown() is typed() from the script line's start time
    const int i = find("CUP HOLDERS");
    TEST_ASSERT_TRUE(i >= 0);
    const uint32_t at = line(i).at;
    TEST_ASSERT_EQUAL_INT(0, shown(i, at - 1, COLS));         // not yet
    TEST_ASSERT_EQUAL_INT((int)strlen(line(i).label), shown(i, at, COLS));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_script_is_in_time_order_and_ends_after_the_last_line);
    RUN_TEST(test_format_right_aligns_the_value_with_a_dot_leader);
    RUN_TEST(test_format_without_value_is_the_label);
    RUN_TEST(test_format_truncates_a_long_value_keeping_one_dot);
    RUN_TEST(test_format_to_a_narrower_width);
    RUN_TEST(test_term_appends_new_labels);
    RUN_TEST(test_term_updates_a_label_in_place);
    RUN_TEST(test_term_scrolls_the_oldest_line_off);
    RUN_TEST(test_term_value_is_truncated_to_fit);
    RUN_TEST(test_typed_reveals_label_then_dots);
    RUN_TEST(test_script_lines_fit_the_terminal);
    RUN_TEST(test_term_script_feeds_due_lines_once);
    RUN_TEST(test_term_script_skips_the_live_line);
    RUN_TEST(test_plain_script_line_has_no_value);
    RUN_TEST(test_term_expire_keeps_fresh_and_pending_lines);
    RUN_TEST(test_term_update_restarts_the_expiry_clock);
    return UNITY_END();
}
