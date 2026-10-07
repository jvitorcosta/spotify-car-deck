#include "textdraw.h"
#include <ctype.h>
#include <string.h>
#include "accents.h"
#include "battle.h"

namespace ui {

static void drawMark(TFT_eSPI& t, txt::Mark m, char base, int cx, int top, uint16_t col) {
    int r0 = top + accents::topRow(m, base);
    for (int y = 0; y < accents::H; ++y) {
        const char* r = accents::row(m, y);
        if (!r) return;
        for (int x = 0; x < accents::W; ++x)
            if (r[x] == '#') t.drawPixel(cx - accents::W / 2 + x, r0 + y, col);
    }
}

int drawFolded(TFT_eSPI& t, const char* text, const txt::Mark* marks, size_t nMarks, int x,
               int y, uint8_t font, uint16_t fg, uint16_t shadow, uint8_t datum) {
    int w = t.textWidth(text, font), h = t.fontHeight(font);
    int left = x, top = y;
    switch (datum) {
        case TC_DATUM: left = x - w / 2; break;
        case TR_DATUM: left = x - w; break;
        case ML_DATUM: top = y - h / 2; break;
        case MC_DATUM: left = x - w / 2; top = y - h / 2; break;
        default: break;   // TL_DATUM
    }
    shadowText(t, text, left, top, font, fg, shadow, TL_DATUM);
    if (font != 2 || !marks) return w;
    char one[2] = {0, 0};
    int px = left;
    for (size_t i = 0; text[i]; ++i) {
        one[0] = text[i];
        int cw = t.textWidth(one, font);
        txt::Mark m = i < nMarks ? marks[i] : txt::Mark::None;
        if (m != txt::Mark::None) {
            int cx = px + (cw - 1) / 2;
            drawMark(t, m, text[i], cx + 1, top + 1, shadow);   // shadow first, like the glyph
            drawMark(t, m, text[i], cx, top, fg);
        }
        px += cw;
    }
    return w;
}

int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint8_t font, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper) {
    char buf[128];
    txt::Mark marks[128];
    size_t n = txt::foldMarks(utf8 ? utf8 : "", buf, marks, sizeof(buf));
    if (upper)
        for (size_t i = 0; i < n; ++i) buf[i] = (char)toupper((unsigned char)buf[i]);
    if (t.textWidth(buf, font) > maxW) {   // truncate with "..." (marks follow the kept chars)
        while (n > 1) {
            buf[--n] = '\0';
            char tmp[132];
            snprintf(tmp, sizeof(tmp), "%s...", buf);
            if (t.textWidth(tmp, font) <= maxW) break;
        }
        strncat(buf, "...", sizeof(buf) - strlen(buf) - 1);
    }
    return drawFolded(t, buf, marks, n, x, y, font, fg, shadow, datum);
}

}
