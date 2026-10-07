#include "screen_now.h"
#include <cstring>
#include "battle.h"
#include "icons.h"
#include "status_sprite.h"
#include "labels.h"
#include "textdraw.h"
#include "theme.h"
#include "../util/text.h"
#include "../util/interp.h"
#include "../util/walkrect.h"
#include "../util/walkanim.h"
#include "../util/lrcstream.h"
#include "../images/walksprite.h"
#include "../core/shared.h"

namespace ui {

// ---- layout (see spec §2) ----
static const int INFO_X = 114, INFO_Y = 30, INFO_W = 198, INFO_H = 86;
static const int STAT_X = 8, STAT_Y = 126, STAT_W = 304, STAT_H = 68;
static const int BAR_X = 24, BAR_Y = 176, BAR_W = 280, BAR_H = 10;   // incl. "HP" tag
static const int FILL_X = BAR_X + HP_TAG_W + 1, FILL_W = BAR_W - HP_TAG_W - 2;
static const int EXP_Y = 188;
static const int DLG_X = 4, DLG_Y = 198, DLG_W = 312, DLG_H = 41;
static const int CD_CY = 10;

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

static int g_cdX = 100;   // set by drawTopStrip from the title width

void drawCdFrame(TFT_eSPI& t, int frame) { drawCd(t, g_cdX, CD_CY, frame, theme::TOP_DARK); }

void drawTopStrip(TFT_eSPI& t, const AppState& st) {
    topStrip(t);
    const char* title = topTitle(st.isPlaying);
    shadowText(t, title, 6, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TL_DATUM);
    g_cdX = 6 + t.textWidth(title, 2) + 10;
    drawCdFrame(t, 0);

    int nameW = drawText(t, st.deviceName[0] ? st.deviceName : "device", 314, 2, 2, theme::BOX_FILL,
                         theme::BOX_BORDER, TR_DATUM, 110);
    int devX = 314 - nameW - 4 - icons::SIZE;
    drawIcon(t, icons::forDevice(st.deviceType), devX, 4, theme::BOX_FILL, theme::TOP_DARK);
    int repX = devX - 18, shufX = repX - 16;
    drawIcon(t, icons::Icon::Repeat, repX, 4,
             st.repeat ? theme::BOX_FILL : theme::ICON_OFF, theme::TOP_DARK);
    if (st.repeat == 2) {   // repeat-one: tiny "1"
        t.setTextColor(theme::HP_TAG_TEXT);
        t.drawString("1", repX + icons::SIZE, 9, 1);
    }
    drawIcon(t, icons::Icon::Shuffle, shufX, 4,
             st.shuffle ? theme::BOX_FILL : theme::ICON_OFF, theme::TOP_DARK);
}

void drawProgressRegion(TFT_eSPI& t, const AppState& st) {
    uint32_t rem = (st.durationMs > st.progressMs) ? (st.durationMs - st.progressMs) : 0;
    uint32_t tot = st.durationMs;
    float hpFrac = st.durationMs ? (float)rem / (float)st.durationMs : 1.0f;
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "HP %u:%02u/%u:%02u",
             rem / 60000, (rem / 1000) % 60, tot / 60000, (tot / 1000) % 60);
    t.fillRect(206, 129, 100, 16, theme::BOX_FILL);   // clear the old time
    shadowText(t, tbuf, 304, 129, 2, theme::TEXT, theme::TEXT_SHADOW, TR_DATUM);
    hpBarBattle(t, BAR_X, BAR_Y, BAR_W, BAR_H, hpFrac);
    expBar(t, FILL_X, EXP_Y, FILL_W, st.volume / 100.0f);
}

void drawWalker(TFT_eSPI& t, const AppState& st, uint32_t animMs, int step) {
    static const int SLACK = 8;
    static walkrect::Rect prev{0, 0, 0, 0};
    static uint16_t buf[(walk::MAX_W + 2 * SLACK) * (walk::BAND_H + 1 + 2 * SLACK)];
    // The network task may promote() a new walker at any time; read frames under the lock.
    shared::Guard lockWalker;
    const walk::Info& wi = walk::info();
    if (!wi.ready) return;

    int fr = 0;
    if (wi.pmd) {
        uint16_t d[walk::MAX_FRAMES];
        for (int i = 0; i < wi.frames; ++i) d[i] = walk::durationMs(i);
        fr = walkanim::frameAt(animMs, d, wi.frames);
    }
    const uint16_t* spr = walk::pixels(fr);
    const uint8_t* msk = walk::mask(fr);
    int w = wi.w, h = wi.h;

    float frac = st.durationMs ? (float)st.progressMs / (float)st.durationMs : 0.0f;
    int cx = interp::walkX(frac, FILL_X, FILL_W, w);   // drained/remaining boundary
    int bob = (!wi.pmd && (step % 2)) ? 1 : 0;           // fallback fake-walk only
    bool mirror = !wi.pmd && ((step / 4) % 2);
    int x0 = cx - w / 2, y0 = BAR_Y - h - bob;          // feet rest on the bar top

    walkrect::Plan p = walkrect::plan(prev, {x0, BAR_Y - h - 1, w, h + 1}, SLACK);
    if (p.clearPrev) t.fillRect(prev.x, prev.y, prev.w, prev.h, theme::BOX_FILL);
    const walkrect::Rect& r = p.push;
    const uint16_t fillBE = (uint16_t)((theme::BOX_FILL >> 8) | (theme::BOX_FILL << 8));
    for (int i = 0; i < r.w * r.h; ++i) buf[i] = fillBE;
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

// Fixed buffer, not std::string: the UI loop shouldn't touch the heap on lyric changes.
// Lyric lines are at most lrcstream::LINE_CAP - 1 bytes, so a full compare always fits.
static char g_lastLyric[lrcstream::LINE_CAP] = "\x01";   // never a real line -> forces a draw
void resetLyricArea() { g_lastLyric[0] = '\x01'; g_lastLyric[1] = '\0'; }

void drawLyricArea(TFT_eSPI& t, const char* currentLine) {
    const char* line = currentLine ? currentLine : "";
    if (strncmp(g_lastLyric, line, sizeof(g_lastLyric)) == 0) return;
    strncpy(g_lastLyric, line, sizeof(g_lastLyric) - 1);
    g_lastLyric[sizeof(g_lastLyric) - 1] = '\0';

    const int ix = DLG_X + 6, iy = DLG_Y + 5, iw = DLG_W - 12, ih = DLG_H - 10;
    t.fillRect(ix, iy, iw, ih, theme::DLG_FILL);
    // static: keep the UI loop stack small; +3 slots so line b's "..." can be written in place
    static glyphrun::Item items[163];
    size_t n = glyphrun::decode(line, items, 160, asciiWidth2, wideWidth, &t);
    if (!n) return;
    drawIcon(t, icons::Icon::Note, ix + 2, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    drawIcon(t, icons::Icon::Note, ix + iw - 14, DLG_Y + 14, theme::DLG_FRAME, theme::DLG_FILL);
    const int textW = iw - 2 * 18;
    glyphrun::Wrap w = glyphrun::wrapTwo(items, n, textW, 3 * asciiWidth2('.', &t));
    int cx = DLG_X + DLG_W / 2;
    if (w.bStart >= w.bEnd) {
        drawRun(t, items, w.aEnd, cx, DLG_Y + 20, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
    } else {
        drawRun(t, items, w.aEnd, cx, DLG_Y + 12, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
        size_t bn = w.bEnd - w.bStart;
        if (w.bEllipsis)   // items past bEnd are not drawn, so "..." can overwrite them
            for (int k = 0; k < 3; ++k)
                items[w.bEnd + k] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.',
                                     (uint8_t)asciiWidth2('.', &t)};
        if (w.bEllipsis) bn += 3;
        drawRun(t, items + w.bStart, bn, cx, DLG_Y + 29, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
    }
}

void drawOffline(TFT_eSPI& t, const char* msg) {
    background(t);
    topStrip(t);
    // Battle platform + the bundled Pokemon (no network needed), 2x nearest-neighbour.
    const int cx = 160, feetY = 136;
    t.fillEllipse(cx, feetY, 60, 12, theme::HORIZON);
    t.drawEllipse(cx, feetY, 60, 12, theme::BOX_SHADOW);
    int w = status_sprite::width(), h = status_sprite::height();
    int s = (h * 2 <= 110 && w * 2 <= 200) ? 2 : 1;
    int x0 = cx - w * s / 2, y0 = feetY - 4 - h * s;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint16_t c;
            if (status_sprite::pixel(x, y, &c)) t.fillRect(x0 + x * s, y0 + y * s, s, s, c);
        }
    dialogueBox(t, 20, 160, 280, 48);
    shadowText(t, msg, 160, 184, 4, theme::TEXT, theme::DLG_SHADOW, MC_DATUM);
}

void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent) {
    background(t);
    drawTopStrip(t, st);

    // album art battle box (main pushes the cover into ART_X/Y/W/H)
    battleBox(t, 8, 24, 98, 98, Tab::None);
    t.fillRect(ART_X, ART_Y, ART_W, ART_H, theme::SKY);

    // info box (opponent-style, slanted right end)
    battleBox(t, INFO_X, INFO_Y, INFO_W, INFO_H, Tab::Right);
    const int TW = INFO_W - 26;
    drawText(t, st.trackName[0] ? st.trackName : "Track title", INFO_X + 8, INFO_Y + 8, 2,
             theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW, true);
    drawText(t, st.artist[0] ? st.artist : "Artist", INFO_X + 8, INFO_Y + 32, 2,
             theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW);
    char from[96];
    snprintf(from, sizeof(from), "From: %s", st.context[0] ? st.context : "Playlist");
    drawText(t, from, INFO_X + 8, INFO_Y + 58, 2, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW);

    // status box (player-style, slanted left end): name + No., HP time, walker, bars
    battleBox(t, STAT_X, STAT_Y, STAT_W, STAT_H, Tab::Left);
    int nmW = drawText(t, st.pokeName[0] ? st.pokeName : "Pokemon", 24, 129, 2, theme::TEXT,
                       theme::TEXT_SHADOW, TL_DATUM, 130, true);
    if (st.pokedexNum > 0) {
        char no[12];
        snprintf(no, sizeof(no), "No.%04d", st.pokedexNum);
        shadowText(t, no, 24 + nmW + 6, 134, 1, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM);
    }
    drawProgressRegion(t, st);

    dialogueBox(t, DLG_X, DLG_Y, DLG_W, DLG_H);
    resetLyricArea();   // box just repainted; main draws the current line next
}

}
