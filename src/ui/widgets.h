#pragma once
#include <TFT_eSPI.h>
namespace ui {
struct Button { int x, y, w, h; const char* label; };
void drawButton(TFT_eSPI& t, const Button& b, bool active);
bool hit(const Button& b, int px, int py);
}
