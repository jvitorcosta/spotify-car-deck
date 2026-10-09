#include <unity.h>
#include "../../src/ui/car_sprite.h"

void setUp() {}
void tearDown() {}

static uint8_t kindAt(int x, int y, uint16_t* c = nullptr) {
    uint8_t k = 99;
    car_sprite::pixel(x, y, c, &k);
    return k;
}

void test_size() {
    TEST_ASSERT_EQUAL_INT(200, car_sprite::width());
    TEST_ASSERT_EQUAL_INT(66, car_sprite::height());
}
void test_transparent_around_the_body_and_outside() {
    TEST_ASSERT_FALSE(car_sprite::pixel(0, 0, nullptr, nullptr));       // above the trunk
    TEST_ASSERT_FALSE(car_sprite::pixel(199, 5, nullptr, nullptr));     // in front of the windshield
    TEST_ASSERT_FALSE(car_sprite::pixel(30, 60, nullptr, nullptr));     // where the rear tyre goes
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(-1, 10));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(200, 10));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::NONE, kindAt(10, 66));
}
void test_body_paint() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::LOWER, kindAt(110, 40, &c));
    TEST_ASSERT_EQUAL_HEX16(0xBE19, c);                                 // #BCC3CB silver
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(120, 24, &c));
    TEST_ASSERT_EQUAL_HEX16(0xD6DC, c);                                 // #D5DBE1 above the shoulder
}
// v2: the white DRL runs along the TOP edge of the headlight (owner's photos), LEDs under it.
void test_lamp_kinds() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(180, 28, &c));    // DRL strip, top edge
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(180, 29, &c));    // LED projector
    TEST_ASSERT_EQUAL_HEX16(0xEF9F, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(190, 32));       // smoked housing, no bottom DRL
    TEST_ASSERT_EQUAL_UINT8(car_sprite::TAIL, kindAt(8, 26));          // taillight LED strip
}
void test_fog_lamp_is_lit() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::HEAD, kindAt(192, 46, &c));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, c);
}
void test_near_black_tint() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(110, 10, &c));
    TEST_ASSERT_EQUAL_HEX16(0x0882, c);                                 // #0C1016
}
void test_silver_door_handles() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(99, 26, &c));     // front door, near its rear edge
    TEST_ASSERT_EQUAL_HEX16(0xEF9E, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(99, 27, &c));
    TEST_ASSERT_EQUAL_HEX16(0x7C31, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(53, 26, &c));     // rear door
    TEST_ASSERT_EQUAL_HEX16(0xEF9E, c);
}
// The owner's trunk lip spoiler stands above the trunk line.
void test_trunk_lip_spoiler() {
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(5, 16));      // spoiler on the higher trunk edge
    TEST_ASSERT_TRUE(car_sprite::pixel(5, 15, nullptr, nullptr));
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(5, 14));      // bright top edge of the lip
    TEST_ASSERT_FALSE(car_sprite::pixel(5, 13, nullptr, nullptr));
}
void test_wheel_arch_liner_is_dark() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(46, 45, &c));
    TEST_ASSERT_EQUAL_HEX16(0x0882, c);                                 // #0E1016
}

// v3 geometry from the side-profile reference: B-pillar at x 86-89, fuel door above the shoulder.
void test_side_profile_landmarks() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(87, 10, &c));     // black B-pillar
    TEST_ASSERT_EQUAL_HEX16(0x2987, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::LOWER, kindAt(32, 21, &c));     // fuel door outline
    TEST_ASSERT_EQUAL_HEX16(0x7C31, c);
}

// Aero details from the owner's side photo: side skirt, front air curtain and lip, rear diffuser.
void test_aero_details() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::LOWER, kindAt(100, 51, &c));   // skirt: light top edge
    TEST_ASSERT_EQUAL_HEX16(0xD6DC, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(100, 55, &c));   // skirt lip 1 px proud of the sill
    TEST_ASSERT_EQUAL_HEX16(0x2987, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(197, 40));       // front air-curtain slot
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(185, 54));       // dark front lip
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(10, 50));        // rear diffuser
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(8, 49));         // diffuser fin
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(5, 18, &c));     // shadow under the spoiler
    TEST_ASSERT_EQUAL_HEX16(0x5B2D, c);
}

// Domed hood (owner's side photo): level and high to x ~175, then curving down to the nose.
void test_domed_hood() {
    TEST_ASSERT_TRUE(car_sprite::pixel(170, 20, nullptr, nullptr));    // above the old flat hood line
    TEST_ASSERT_FALSE(car_sprite::pixel(170, 18, nullptr, nullptr));   // but not higher than the windshield base
    TEST_ASSERT_TRUE(car_sprite::pixel(186, 23, nullptr, nullptr));    // still high mid-curve
    TEST_ASSERT_FALSE(car_sprite::pixel(197, 26, nullptr, nullptr));   // and dropping steeply at the nose
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_size);
    RUN_TEST(test_transparent_around_the_body_and_outside);
    RUN_TEST(test_body_paint);
    RUN_TEST(test_lamp_kinds);
    RUN_TEST(test_trunk_lip_spoiler);
    RUN_TEST(test_wheel_arch_liner_is_dark);
    RUN_TEST(test_fog_lamp_is_lit);
    RUN_TEST(test_near_black_tint);
    RUN_TEST(test_silver_door_handles);
    RUN_TEST(test_side_profile_landmarks);
    RUN_TEST(test_aero_details);
    RUN_TEST(test_domed_hood);
    return UNITY_END();
}
