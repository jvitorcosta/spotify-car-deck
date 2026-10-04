#pragma once
#include <TFT_eSPI.h>
namespace ui {
void panel(TFT_eSPI& t, int x, int y, int w, int h);                 // cream w/ navy+gold border
void hpBar(TFT_eSPI& t, int x, int y, int w, int h, float frac);     // green fill, dark bg
struct Button { int x, y, w, h; const char* label; };
void drawButton(TFT_eSPI& t, const Button& b, bool active);
bool hit(const Button& b, int px, int py);
}
