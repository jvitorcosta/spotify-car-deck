#pragma once
#include <TFT_eSPI.h>
// Gen-3 battle-screen drawing primitives.
namespace ui {
enum class Tab { None, Left, Right };
constexpr int HP_TAG_W = 22;
void background(TFT_eSPI& t);                                    // sky / horizon / grass (y >= 20)
void topStrip(TFT_eSPI& t);                                      // dark strip y 0..19
void battleBox(TFT_eSPI& t, int x, int y, int w, int h, Tab tab);  // cream box, border, shadow, slanted tab
void shadowText(TFT_eSPI& t, const char* s, int x, int y, uint8_t font,
                uint16_t fg, uint16_t shadow, uint8_t datum);   // transparent text + 1 px shadow
void hpBarBattle(TFT_eSPI& t, int x, int y, int w, int h, float frac);  // x = start of "HP" tag
void expBar(TFT_eSPI& t, int x, int y, int w, float frac);      // 3 px blue bar
void dialogueBox(TFT_eSPI& t, int x, int y, int w, int h);      // teal frame, white interior
}
