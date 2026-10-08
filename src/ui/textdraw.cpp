#include "textdraw.h"
#include "accents.h"
#include "battle.h"
#include "cjkdata.h"

namespace ui {

int asciiWidth2(char c, void* ctx) {
    char one[2] = {c, 0};
    return ((TFT_eSPI*)ctx)->textWidth(one, 2);
}

int wideWidth(uint32_t cp, void*) {
    int w = 0;
    return cjk().glyph(cp, &w) ? w : 0;
}

static void drawMark(TFT_eSPI& t, txt::Mark m, char base, int cx, int top, uint16_t col) {
    int r0 = top + accents::topRow(m, base);
    for (int y = 0; y < accents::H; ++y) {
        const char* r = accents::row(m, y);
        if (!r) return;
        for (int x = 0; x < accents::W; ++x)
            if (r[x] == '#') t.drawPixel(cx - accents::W / 2 + x, r0 + y, col);
    }
}

static void drawWide(TFT_eSPI& t, uint32_t cp, int x, int top, uint16_t col) {
    int w = 0;
    const uint8_t* g = cjk().glyph(cp, &w);
    if (!g) return;
    int bpr = w / 8;                                     // bytes per row
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < w; ++c)
            if ((g[r * bpr + c / 8] >> (7 - (c & 7))) & 1) t.drawPixel(x + c, top + CJK_DY + r, col);
}

int drawRun(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
            uint16_t shadow, uint8_t datum) {
    int w = glyphrun::width(it, n), h = t.fontHeight(2);
    int left = x, top = y;
    switch (datum) {
        case TC_DATUM: left = x - w / 2; break;
        case TR_DATUM: left = x - w; break;
        case ML_DATUM: top = y - h / 2; break;
        case MC_DATUM: left = x - w / 2; top = y - h / 2; break;
        default: break;   // TL_DATUM
    }
    int px = left;
    char one[2] = {0, 0};
    for (size_t i = 0; i < n; ++i) {
        const glyphrun::Item& g = it[i];
        if (g.kind == glyphrun::Kind::Wide) {
            drawWide(t, g.cp, px + 1, top + 1, shadow);   // shadow first, like shadowText
            drawWide(t, g.cp, px, top, fg);
        } else {
            one[0] = g.ch;
            shadowText(t, one, px, top, 2, fg, shadow, TL_DATUM);
            if (g.mark != txt::Mark::None) {
                int cx = px + (g.w - 1) / 2;
                drawMark(t, g.mark, g.ch, cx + 1, top + 1, shadow);
                drawMark(t, g.mark, g.ch, cx, top, fg);
            }
        }
        px += g.w;
    }
    return w;
}

int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper) {
    glyphrun::Item items[96];
    size_t n = glyphrun::decode(utf8 ? utf8 : "", items, 96, asciiWidth2, wideWidth, &t, upper);
    n = glyphrun::fit(items, n, 96, maxW, asciiWidth2, &t);
    return drawRun(t, items, n, x, y, fg, shadow, datum);
}

}
