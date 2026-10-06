#include "screen_now.h"
#include "widgets.h"
#include "theme.h"
#include "../util/text.h"
#include "../util/interp.h"
#include "../util/walkrect.h"
#include "../images/png.h"

namespace ui {

// Button layout kept for when touch controls are wired back on; not drawn now.
NowButtons nowButtons() {
    NowButtons b;
    int y = 210, h = 26;
    b.prev   = {8,   y, 46, h, "<<"};
    b.play   = {58,  y, 56, h, ">II"};
    b.next   = {118, y, 46, h, ">>"};
    b.vol    = {168, y, 44, h, "VOL"};
    b.lyrics = {216, y, 96, h, "LYRICS"};
    return b;
}

// Fold UTF-8 accents to ASCII (fonts are ASCII-only), then truncate with an
// ellipsis so the text fits within maxW pixels.
static String fitText(TFT_eSPI& t, const char* s, int maxW, uint8_t font) {
    char folded[128];
    txt::asciiFold(s, folded, sizeof(folded));
    String str = folded;
    if (t.textWidth(str, font) <= maxW) return str;
    while (str.length() > 1) {
        str.remove(str.length() - 1);
        if (t.textWidth(str + "...", font) <= maxW) break;
    }
    return str + "...";
}

// HP bar DEPLETES from the left (remaining fill anchored right), green/yellow/red,
// labelled 'HP current/max' (remaining time / total) like a Pokemon life bar.
// The walking Pokemon rides on top of the bar (band between the label and bar).
static const int HP_X = 126, HP_Y = 176, HP_W = 178, HP_H = 16;

void drawProgressRegion(TFT_eSPI& t, const AppState& st) {
    uint32_t rem = (st.durationMs > st.progressMs) ? (st.durationMs - st.progressMs) : 0;
    uint32_t tot = st.durationMs;
    float hpFrac = st.durationMs ? (float)rem / (float)st.durationMs : 1.0f;
    char tbuf[28];
    snprintf(tbuf, sizeof(tbuf), "HP  %u:%02u/%u:%02u",
             rem / 60000, (rem / 1000) % 60, tot / 60000, (tot / 1000) % 60);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.setTextDatum(TL_DATUM);
    t.setTextPadding(178);          // overwrite the old label in place (no flicker)
    t.drawString(tbuf, HP_X, 124, 2);
    t.setTextPadding(0);
    hpBar(t, HP_X, HP_Y, HP_W, HP_H, hpFrac);
}

// The walker is composed onto the cream panel in RAM and pushed as one rect.
// The rect also covers where it stood last frame, so moving erases its trail.
void drawWalker(TFT_eSPI& t, const AppState& st, int step) {
    static const int SLACK = 8;
    static walkrect::Rect prev{0, 0, 0, 0};
    static uint16_t buf[(img::WALK_MAX + 2 * SLACK) * (img::WALK_MAX + 1 + 2 * SLACK)];
    if (!img::walkReady()) return;
    const uint16_t* spr = img::walkBuffer();
    const uint8_t* msk = img::walkMask();
    int w = img::walkW(), h = img::walkH();

    float frac = st.durationMs ? (float)st.progressMs / (float)st.durationMs : 0.0f;
    // Stand on the drained/remaining boundary, inside the bar's inner fill area.
    int cx = interp::walkX(frac, HP_X + 2, HP_W - 4, w);
    int bob = (step % 2) ? 1 : 0;                 // lift 1px every other frame
    bool mirror = (step / 4) % 2;                 // turn every few frames = "step"
    int x0 = cx - w / 2, y0 = HP_Y - h - bob;     // feet rest on the bar top

    // Sprite rect includes the 1px bob headroom; union with last frame's rect
    // so moving erases the trail (a far jump clears the old rect instead).
    walkrect::Plan p = walkrect::plan(prev, {x0, HP_Y - h - 1, w, h + 1}, SLACK);
    if (p.clearPrev) t.fillRect(prev.x, prev.y, prev.w, prev.h, theme::GBA_CREAM);
    const walkrect::Rect& r = p.push;
    const uint16_t creamBE = (uint16_t)((theme::GBA_CREAM >> 8) | (theme::GBA_CREAM << 8));
    for (int i = 0; i < r.w * r.h; ++i) buf[i] = creamBE;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sx = mirror ? (w - 1 - x) : x;
            if (!msk[y * w + sx]) continue;
            int bx = x0 + x - r.x, by = y0 + y - r.y;
            if (bx >= 0 && bx < r.w && by >= 0 && by < r.h) buf[by * r.w + bx] = spr[y * w + sx];
        }
    t.pushImage(r.x, r.y, r.w, r.h, buf);
    prev = p.next;
}

// Current synced lyric line, wrapped to up to two centered lines in the area
// below the HP bar. Empty string clears the area.
void drawLyricArea(TFT_eSPI& t, const char* currentLine) {
    const int ax = 123, ay = 196, aw = 184, ah = 40;
    t.fillRect(ax, ay, aw, ah, theme::GBA_CREAM);   // clear (inside the HP panel)
    char folded[160];
    txt::asciiFold(currentLine, folded, sizeof(folded));
    if (!folded[0]) return;

    t.setTextColor(theme::POKE_RED, theme::GBA_CREAM);   // highlight the sung line
    t.setTextDatum(MC_DATUM);
    int cx = ax + aw / 2;
    String s = folded;
    if (t.textWidth(s, 2) <= aw) {
        t.drawString(s, cx, ay + ah / 2, 2);
    } else {
        // greedy wrap into two lines on spaces
        String l1, l2;
        int sp = -1;
        for (int i = 0; i < (int)s.length(); ++i) {
            if (s[i] == ' ') {
                if (t.textWidth(s.substring(0, i), 2) <= aw) sp = i; else break;
            }
        }
        if (sp < 0) sp = s.length() / 2;   // no good space -> hard split
        l1 = s.substring(0, sp);
        l2 = s.substring(sp + (s[sp] == ' ' ? 1 : 0));
        while (l2.length() > 1 && t.textWidth(l2 + "...", 2) > aw) l2.remove(l2.length() - 1);
        if (t.textWidth(s.substring(sp), 2) > aw) l2 += "...";
        t.drawString(l1, cx, ay + 10, 2);
        t.drawString(l2, cx, ay + 28, 2);
    }
    t.setTextDatum(TL_DATUM);
}

void drawOffline(TFT_eSPI& t, const char* msg) {
    t.fillScreen(theme::GBA_NAVY);
    t.fillRect(0, 0, 320, 22, theme::GBA_NAVY);
    t.setTextColor(theme::GBA_CREAM, theme::GBA_NAVY);
    t.setTextDatum(MC_DATUM);
    t.drawString(msg, 160, 120, 4);
    t.setTextDatum(TL_DATUM);
}

void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent) {
    t.fillScreen(0x6ADC);  // GBA sky blue background

    // top bar
    t.fillRect(0, 0, 320, 22, theme::GBA_NAVY);
    t.setTextColor(theme::GBA_CREAM, theme::GBA_NAVY);
    t.setTextDatum(TL_DATUM);
    t.drawString("NOW PLAYING", 8, 5, 2);
    t.setTextDatum(TR_DATUM);
    t.drawString(fitText(t, st.deviceName[0] ? st.deviceName : "device", 120, 2), 312, 5, 2);
    t.setTextDatum(TL_DATUM);

    // LEFT column: album art box (cover pushed on top by main) ...
    panel(t, 8, 26, 104, 104);
    t.fillRect(11, 29, 98, 98, 0xBDD7);   // placeholder until the cover is pushed

    // ... and a roomy Pokemon box below (sprite pushed by main between the labels)
    panel(t, 8, 134, 104, 104);
    t.setTextDatum(MC_DATUM);
    if (st.pokedexNum > 0) {
        char no[12];
        snprintf(no, sizeof(no), "No.%03d", st.pokedexNum);
        t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
        t.drawString(no, 60, 141, 1);
    }
    t.setTextColor(accent, theme::GBA_CREAM);
    t.drawString(fitText(t, st.pokeName[0] ? st.pokeName : "Pokemon", 100, 2), 60, 226, 2);
    t.setTextDatum(TL_DATUM);

    // RIGHT column (all text truncated to the panel width)
    const int RW = 182;
    panel(t, 118, 26, 194, 48);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.drawString(fitText(t, st.trackName[0] ? st.trackName : "Track title", RW, 2), 124, 32, 2);
    t.drawString(fitText(t, st.artist[0] ? st.artist : "Artist", RW, 2), 124, 54, 2);

    panel(t, 118, 78, 194, 38);
    t.drawString("From:", 124, 82, 2);
    t.drawString(fitText(t, st.context[0] ? st.context : "Playlist", RW, 2), 124, 98, 2);

    panel(t, 118, 120, 194, 118);   // HP panel: label, walker band, bar, lyric
    drawProgressRegion(t, st);
}
}
