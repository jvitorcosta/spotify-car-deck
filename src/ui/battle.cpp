#include "battle.h"
#include "theme.h"

namespace ui {

void background(TFT_eSPI& t) {
    t.fillRect(0, 20, 320, 98, theme::pal().sky);
    t.fillRect(0, 118, 320, 4, theme::pal().horizon);
    t.fillRect(0, 122, 320, 118, theme::pal().grass);
}

void topStrip(TFT_eSPI& t) { t.fillRect(0, 0, 320, 20, theme::pal().top); }

// Box silhouette: rectangle with one side slanted by c px over its height.
static void shape(TFT_eSPI& t, int x, int y, int w, int h, Tab tab, int c, uint16_t col) {
    if (tab == Tab::Left) {
        t.fillRect(x + c, y, w - c, h, col);
        t.fillTriangle(x, y, x + c, y, x + c, y + h - 1, col);
    } else if (tab == Tab::Right) {
        t.fillRect(x, y, w - c, h, col);
        t.fillTriangle(x + w - c, y, x + w - 1, y, x + w - c, y + h - 1, col);
    } else {
        t.fillRect(x, y, w, h, col);
    }
}

void battleBox(TFT_eSPI& t, int x, int y, int w, int h, Tab tab) {
    const int c = (tab == Tab::None) ? 0 : 10;
    shape(t, x + 3, y + 3, w, h, tab, c, theme::pal().boxShadow);
    shape(t, x, y, w, h, tab, c, theme::pal().boxBorder);
    shape(t, x + 2, y + 2, w - 4, h - 4, tab, c > 2 ? c - 2 : 0, theme::pal().boxFill);
}

void shadowText(TFT_eSPI& t, const char* s, int x, int y, uint8_t font,
                uint16_t fg, uint16_t shadow, uint8_t datum) {
    t.setTextDatum(datum);
    t.setTextColor(shadow);          // single-arg colour = transparent background
    t.drawString(s, x + 1, y + 1, font);
    t.setTextColor(fg);
    t.drawString(s, x, y, font);
    t.setTextDatum(TL_DATUM);
}

void hpBarBattle(TFT_eSPI& t, int x, int y, int w, int h, float frac) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    t.fillRect(x, y, HP_TAG_W, h, theme::HP_TAG);
    t.setTextColor(theme::HP_TAG_TEXT);
    t.setTextDatum(ML_DATUM);
    t.drawString("HP", x + 4, y + h / 2 + 1, 1);
    t.setTextDatum(TL_DATUM);
    int fx = x + HP_TAG_W, fw = w - HP_TAG_W;
    t.fillRect(fx, y, fw, h, theme::HP_EMPTY);
    int inner = fw - 2;
    int filled = (int)(inner * frac);
    int x0 = fx + 1 + (inner - filled);               // drains from the left
    t.fillRect(x0, y + 1, filled, h - 2, theme::hpColor(frac));
    t.fillRect(x0, y + 1, filled, 2, theme::hpShine(frac));
}

void expBar(TFT_eSPI& t, int x, int y, int w, float frac) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    t.fillRect(x, y, w, 3, theme::HP_EMPTY);
    t.fillRect(x, y, (int)(w * frac), 3, theme::EXP_BLUE);
}

void dialogueBox(TFT_eSPI& t, int x, int y, int w, int h) {
    t.fillRoundRect(x, y, w, h, 4, theme::pal().dlgFrame);
    t.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 3, theme::pal().dlgLine);
    t.fillRoundRect(x + 4, y + 4, w - 8, h - 8, 3, theme::pal().dlgFill);
}

}
