#include <unity.h>
#include <cstring>
#include "../../src/util/animdata.h"

void setUp() {}
void tearDown() {}

static const char* XML_BASIC =
    "<AnimData><ShadowSize>1</ShadowSize><Anims>"
    "<Anim><Name>Idle</Name><Index>7</Index><FrameWidth>24</FrameWidth><FrameHeight>32</FrameHeight>"
    "<Durations><Duration>40</Duration><Duration>20</Duration></Durations></Anim>"
    "<Anim><Name>WalkAlt</Name><Index>9</Index><FrameWidth>99</FrameWidth><FrameHeight>99</FrameHeight>"
    "<Durations><Duration>1</Duration></Durations></Anim>"
    "<Anim>\n\t<Name>Walk</Name>\n\t<Index>0</Index>\n\t<FrameWidth>32</FrameWidth>\n"
    "\t<FrameHeight>40</FrameHeight>\n\t<Durations>\n\t\t<Duration>8</Duration>\n"
    "\t\t<Duration>10</Duration>\n\t\t<Duration>8</Duration>\n\t\t<Duration>10</Duration>\n"
    "\t</Durations>\n</Anim>"
    "</Anims></AnimData>";

void test_parses_walk_block() {
    animdata::WalkAnim w = animdata::parseWalk(XML_BASIC);
    TEST_ASSERT_TRUE(w.ok);
    TEST_ASSERT_EQUAL_INT(32, w.frameW);
    TEST_ASSERT_EQUAL_INT(40, w.frameH);
    TEST_ASSERT_EQUAL_INT(4, w.frames);
    TEST_ASSERT_EQUAL_UINT16(8, w.ticks[0]);
    TEST_ASSERT_EQUAL_UINT16(10, w.ticks[3]);
}
void test_follows_copy_of() {
    const char* xml =
        "<Anims><Anim><Name>Idle</Name><FrameWidth>24</FrameWidth><FrameHeight>32</FrameHeight>"
        "<Durations><Duration>40</Duration><Duration>20</Duration></Durations></Anim>"
        "<Anim><Name>Walk</Name><Index>0</Index><CopyOf>Idle</CopyOf></Anim></Anims>";
    animdata::WalkAnim w = animdata::parseWalk(xml);
    TEST_ASSERT_TRUE(w.ok);
    TEST_ASSERT_EQUAL_INT(24, w.frameW);
    TEST_ASSERT_EQUAL_INT(2, w.frames);
    TEST_ASSERT_EQUAL_UINT16(40, w.ticks[0]);
}
void test_missing_walk_is_not_ok() {
    const char* xml = "<Anims><Anim><Name>Idle</Name><FrameWidth>24</FrameWidth>"
                      "<FrameHeight>32</FrameHeight><Durations><Duration>4</Duration>"
                      "</Durations></Anim></Anims>";
    TEST_ASSERT_FALSE(animdata::parseWalk(xml).ok);
}
void test_prefix_name_does_not_match() {
    const char* xml = "<Anims><Anim><Name>WalkAlt</Name><FrameWidth>9</FrameWidth>"
                      "<FrameHeight>9</FrameHeight><Durations><Duration>4</Duration>"
                      "</Durations></Anim></Anims>";
    TEST_ASSERT_FALSE(animdata::parseWalk(xml).ok);
}
void test_copy_of_cycle_is_not_ok() {
    const char* xml = "<Anims><Anim><Name>Walk</Name><CopyOf>Hop</CopyOf></Anim>"
                      "<Anim><Name>Hop</Name><CopyOf>Walk</CopyOf></Anim></Anims>";
    TEST_ASSERT_FALSE(animdata::parseWalk(xml).ok);
}
void test_null_and_garbage_are_not_ok() {
    TEST_ASSERT_FALSE(animdata::parseWalk(nullptr).ok);
    TEST_ASSERT_FALSE(animdata::parseWalk("404: Not Found").ok);
}
void test_more_durations_than_max_are_capped() {
    char xml[1200] = "<Anims><Anim><Name>Walk</Name><FrameWidth>32</FrameWidth>"
                     "<FrameHeight>32</FrameHeight><Durations>";
    for (int i = 0; i < 20; ++i) strcat(xml, "<Duration>5</Duration>");
    strcat(xml, "</Durations></Anim></Anims>");
    animdata::WalkAnim w = animdata::parseWalk(xml);
    TEST_ASSERT_TRUE(w.ok);
    TEST_ASSERT_EQUAL_INT(animdata::MAX_FRAMES, w.frames);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_walk_block);
    RUN_TEST(test_follows_copy_of);
    RUN_TEST(test_missing_walk_is_not_ok);
    RUN_TEST(test_prefix_name_does_not_match);
    RUN_TEST(test_copy_of_cycle_is_not_ok);
    RUN_TEST(test_null_and_garbage_are_not_ok);
    RUN_TEST(test_more_durations_than_max_are_capped);
    return UNITY_END();
}
