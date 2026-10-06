#pragma once
#include <TFT_eSPI.h>
#include "icon_map.h"
namespace ui {
// 12x12 pixel icon at (x,y), composed in RAM and pushed in one go.
void drawIcon(TFT_eSPI& t, icons::Icon i, int x, int y, uint16_t fg, uint16_t bg);
// 13x13 spinning CD centred at (cx,cy); frame 0..3 rotates the sheen 45 deg each.
void drawCd(TFT_eSPI& t, int cx, int cy, int frame, uint16_t bg);
}
