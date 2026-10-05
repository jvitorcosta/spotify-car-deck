#include "widgets.h"
#include "theme.h"
namespace ui {
void panel(TFT_eSPI& t, int x, int y, int w, int h) {
    t.fillRoundRect(x, y, w, h, 3, theme::GBA_CREAM);
    t.drawRoundRect(x, y, w, h, 3, theme::GBA_NAVY);
    t.drawRoundRect(x+1, y+1, w-2, h-2, 3, theme::GBA_GOLD);
}
void hpBar(TFT_eSPI& t, int x, int y, int w, int h, float frac) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    t.fillRect(x, y, w, h, 0x2124);                 // dark bg (depleted portion)
    t.drawRect(x, y, w, h, theme::GBA_NAVY);
    // Drain from the LEFT: remaining fill stays anchored on the right edge.
    int fw = (int)((w - 4) * frac);
    t.fillRect(x + 2 + ((w - 4) - fw), y + 2, fw, h - 4, theme::hpColor(frac));
}
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
