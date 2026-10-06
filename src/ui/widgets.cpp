#include "widgets.h"
#include "theme.h"
namespace ui {
void drawButton(TFT_eSPI& t, const Button& b, bool active) {
    uint16_t bg = active ? theme::POKE_RED : theme::GBA_CREAM;
    uint16_t fg = active ? TFT_WHITE : theme::GBA_NAVY;
    t.fillRoundRect(b.x, b.y, b.w, b.h, 3, bg);
    t.drawRoundRect(b.x, b.y, b.w, b.h, 3, theme::GBA_NAVY);
    t.setTextColor(fg, bg);
    t.setTextDatum(MC_DATUM);
    t.drawString(b.label, b.x + b.w/2, b.y + b.h/2, 2);
    t.setTextDatum(TL_DATUM);
}
bool hit(const Button& b, int px, int py) {
    return px >= b.x && px <= b.x+b.w && py >= b.y && py <= b.y+b.h;
}
}
