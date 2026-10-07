#include <unity.h>
#include "../../src/util/ctxcache.h"

void setUp() {}
void tearDown() {}

void test_new_uri_needs_lookup() {
    ctxcache::Cache c;
    TEST_ASSERT_TRUE(c.needsLookup("spotify:playlist:abc", 0));
}
void test_successful_lookup_is_kept() {
    ctxcache::Cache c;
    c.store("spotify:playlist:abc", true, 1000);
    TEST_ASSERT_FALSE(c.needsLookup("spotify:playlist:abc", 1000 + 10 * ctxcache::RETRY_MS));
    TEST_ASSERT_TRUE(c.needsLookup("spotify:album:xyz", 2000));
}
void test_failed_lookup_retries_after_backoff() {
    ctxcache::Cache c;
    c.store("spotify:playlist:abc", false, 1000);
    TEST_ASSERT_FALSE(c.needsLookup("spotify:playlist:abc", 1000 + ctxcache::RETRY_MS - 1));
    TEST_ASSERT_TRUE(c.needsLookup("spotify:playlist:abc", 1000 + ctxcache::RETRY_MS));
}
void test_failed_lookup_retry_survives_millis_wrap() {
    ctxcache::Cache c;
    c.store("spotify:playlist:abc", false, 0xFFFFFF00u);
    TEST_ASSERT_FALSE(c.needsLookup("spotify:playlist:abc", 0x00000010u));
    TEST_ASSERT_TRUE(c.needsLookup("spotify:playlist:abc", 0xFFFFFF00u + ctxcache::RETRY_MS));
}
void test_null_uri_is_treated_as_empty() {
    ctxcache::Cache c;
    c.store(nullptr, true, 0);
    TEST_ASSERT_FALSE(c.needsLookup("", 5));
    TEST_ASSERT_FALSE(c.needsLookup(nullptr, 5));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_new_uri_needs_lookup);
    RUN_TEST(test_successful_lookup_is_kept);
    RUN_TEST(test_failed_lookup_retries_after_backoff);
    RUN_TEST(test_failed_lookup_retry_survives_millis_wrap);
    RUN_TEST(test_null_uri_is_treated_as_empty);
    return UNITY_END();
}
