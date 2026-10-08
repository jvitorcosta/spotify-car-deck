#include "screen_now.h"
#include <cstring>
#include "battle.h"
#include "icons.h"
#include "pokeball.h"
#include "typebadge.h"
#include "genrebadge.h"
#include "../util/genre.h"
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

// ---- layout (320x240 landscape) ----
static const int INFO_X = 114, INFO_Y = 30, INFO_W = 198, INFO_H = 86;
static const int STAT_X = 8, STAT_Y = 126, STAT_W = 304, STAT_H = 68;
static const int BAR_X = 24, BAR_Y = 176, BAR_W = 280, BAR_H = 10;   // incl. "HP" tag
static const int FILL_X = BAR_X + HP_TAG_W + 1, FILL_W = BAR_W - HP_TAG_W - 2;
static const int EXP_Y = 188;
static const int DLG_X = 4, DLG_Y = 198, DLG_W = 312, DLG_H = 41;
static const int CD_CY = 10;

static int g_cdX = 100;   // set by drawTopStrip from the title width

void drawCdFrame(TFT_eSPI& t, int frame) { drawCd(t, g_cdX, CD_CY, frame, theme::TOP_DARK); }

void drawTopStrip(TFT_eSPI& t, const AppState& st) {
    topStrip(t);
    const char* title = topTitle(st.isPlaying);
    shadowText(t, title, 6, 2, 2, theme::BOX_FILL, theme::BOX_BORDER, TL_DATUM);
    g_cdX = 6 + t.textWidth(title, 2) + 10;
    drawCdFrame(t, 0);

    int nameW = drawText(t, st.deviceName[0] ? st.deviceName : "device", 314, 2, theme::BOX_FILL,
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

// Spinning Poke Ball left of the time (it replaced the "HP " prefix); position follows the
// time's width, frame advances with the top-bar CD (main.cpp).
static const int BALL_Y = 130;
static int g_ballX = -1;   // left edge, set by drawProgressRegion
static int g_ballFrame = 0;

static void drawBall(TFT_eSPI& t) {
    if (g_ballX < 0) return;
    uint16_t buf[pokeball::SIZE * pokeball::SIZE];
    for (int y = 0; y < pokeball::SIZE; ++y)
        for (int x = 0; x < pokeball::SIZE; ++x) {
            uint16_t c;
            switch (pokeball::pixel(x, y, g_ballFrame)) {
                case pokeball::Px::Red:   c = theme::POKE_RED; break;
                case pokeball::Px::White: c = TFT_WHITE; break;
                case pokeball::Px::Dark:  c = theme::TEXT; break;
                default:                  c = theme::BOX_FILL; break;
            }
            buf[y * pokeball::SIZE + x] = (uint16_t)((c >> 8) | (c << 8));   // pushImage is big-endian
        }
    t.pushImage(g_ballX, BALL_Y, pokeball::SIZE, pokeball::SIZE, buf);
}

void drawPokeballFrame(TFT_eSPI& t, int frame) {
    g_ballFrame = frame;
    drawBall(t);
}

void drawProgressRegion(TFT_eSPI& t, const AppState& st) {
    uint32_t rem = (st.durationMs > st.progressMs) ? (st.durationMs - st.progressMs) : 0;
    uint32_t tot = st.durationMs;
    float hpFrac = st.durationMs ? (float)rem / (float)st.durationMs : 1.0f;
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "%u:%02u/%u:%02u",
             rem / 60000, (rem / 1000) % 60, tot / 60000, (tot / 1000) % 60);
    t.fillRect(206, 129, 100, 16, theme::BOX_FILL);   // clear the old time (and ball)
    shadowText(t, tbuf, 304, 129, 2, theme::TEXT, theme::TEXT_SHADOW, TR_DATUM);
    g_ballX = 304 - t.textWidth(tbuf, 2) - 4 - pokeball::SIZE;
    drawBall(t);
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
// Text reaching here is at most 159 bytes (shared::LyricView / lyricmsg::TEXT_CAP), well under
// LINE_CAP, so a full compare always fits.
static char g_lastLyric[lrcstream::LINE_CAP] = "\x01";   // never a real line -> forces a draw
void resetLyricArea() { g_lastLyric[0] = '\x01'; g_lastLyric[1] = '\0'; }

static bool g_lastNotes = true;

// The two note icons at the ends of the dialogue box. frame < 0: at rest; otherwise they bob
// 1 px up/down in opposite directions, swapping each frame. Only their 12x16 columns are
// repainted, never the lyric text.
static void drawNotes(TFT_eSPI& t, int frame) {
    const int ix = DLG_X + 6, iw = DLG_W - 12, y = DLG_Y + 14;
    const int xl = ix + 2, xr = ix + iw - 14;
    int dl = 0, dr = 0;
    if (frame >= 0) { dl = (frame & 1) ? 1 : -1; dr = -dl; }
    t.fillRect(xl, y - 2, icons::SIZE, icons::SIZE + 4, theme::DLG_FILL);
    t.fillRect(xr, y - 2, icons::SIZE, icons::SIZE + 4, theme::DLG_FILL);
    drawIcon(t, icons::Icon::Note, xl, y + dl, theme::DLG_FRAME, theme::DLG_FILL);
    drawIcon(t, icons::Icon::Note, xr, y + dr, theme::DLG_FRAME, theme::DLG_FILL);
}

void drawNoteFrame(TFT_eSPI& t, int frame) {
    if (g_lastNotes) drawNotes(t, frame);   // plain lyrics have no notes to move
}

void drawLyricArea(TFT_eSPI& t, const char* text, bool notes) {
    const char* line = text ? text : "";
    if (notes == g_lastNotes && strncmp(g_lastLyric, line, sizeof(g_lastLyric)) == 0) return;
    g_lastNotes = notes;
    strncpy(g_lastLyric, line, sizeof(g_lastLyric) - 1);
    g_lastLyric[sizeof(g_lastLyric) - 1] = '\0';

    const int ix = DLG_X + 6, iy = DLG_Y + 5, iw = DLG_W - 12, ih = DLG_H - 10;
    t.fillRect(ix, iy, iw, ih, theme::DLG_FILL);
    // static: keep the UI loop stack small; +3 slots so line b's "..." can be written in place
    static glyphrun::Item items[163];
    size_t n = glyphrun::decode(line, items, 160, asciiWidth2, wideWidth, &t);
    if (!n) return;
    if (notes) drawNotes(t, -1);
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

// Gen-3 summary-style badge: rounded box in the colour, darker border, white font-1 label.
static void drawBadge(TFT_eSPI& t, int x, int y, int w, int h, const char* label, uint16_t c) {
    uint16_t edge = theme::darken(c);
    t.fillRoundRect(x, y, w, h, 3, c);
    t.drawRoundRect(x, y, w, h, 3, edge);
    shadowText(t, label, x + w / 2, y + h / 2, 1, TFT_WHITE, edge, MC_DATUM);
}

// Info box, artist row: [GENRE] Artist. Without a badge the name keeps the full width.
void drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre) {
    const int TW = INFO_W - 26, x0 = INFO_X + 8, y = INFO_Y + 32;
    t.fillRect(x0, y - 1, TW, 19, theme::BOX_FILL);
    int x = x0, maxW = TW;
    if (genre != genre::NONE && genre < genrebadge::count()) {
        const genrebadge::Badge& b = genrebadge::at(genre);
        drawBadge(t, x, y + 2, genrebadge::W, genrebadge::H, b.label, b.color);
        x += genrebadge::W + 6;
        maxW -= genrebadge::W + 6;
    }
    drawText(t, st.artist[0] ? st.artist : "Artist", x, y, theme::TEXT, theme::TEXT_SHADOW,
             TL_DATUM, maxW);
}

void drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre) {
    background(t);
    drawTopStrip(t, st);

    // album art battle box (main pushes the cover into ART_X/Y/W/H)
    battleBox(t, 8, 24, 98, 98, Tab::None);
    t.fillRect(ART_X, ART_Y, ART_W, ART_H, theme::SKY);

    // info box (opponent-style, slanted right end)
    battleBox(t, INFO_X, INFO_Y, INFO_W, INFO_H, Tab::Right);
    const int TW = INFO_W - 26;
    drawText(t, st.trackName[0] ? st.trackName : "Track title", INFO_X + 8, INFO_Y + 8,
             theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW, true);
    drawArtistRow(t, st, genre);
    char from[96];
    snprintf(from, sizeof(from), "From: %s", st.context[0] ? st.context : "Playlist");
    drawText(t, from, INFO_X + 8, INFO_Y + 58, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM, TW);

    // status box (player-style, slanted left end): name + No., HP time, walker, bars
    battleBox(t, STAT_X, STAT_Y, STAT_W, STAT_H, Tab::Left);
    // Gen-3 type badge first (fixed spot: readable at a glance, doesn't move with the name's
    // length), then the name, then "No.0025" only if it still ends before the time area
    // (x >= 206 is cleared and redrawn every 250 ms by drawProgressRegion).
    int x = 24;
    char lbl[12];
    if (typebadge::label(st.pokeType, lbl, sizeof(lbl))) {
        drawBadge(t, x, 131, typebadge::W, typebadge::H, lbl, theme::typeColor(st.pokeType));
        x += typebadge::W + 6;
    }
    int nmW = drawText(t, st.pokeName[0] ? st.pokeName : "Pokemon", x, 129, theme::TEXT,
                       theme::TEXT_SHADOW, TL_DATUM, 96, true);
    x += nmW + 6;
    if (st.pokedexNum > 0) {
        char no[12];
        snprintf(no, sizeof(no), "No.%04d", st.pokedexNum);
        if (x + (int)strlen(no) * typebadge::CHAR_W <= 204)
            shadowText(t, no, x, 134, 1, theme::TEXT, theme::TEXT_SHADOW, TL_DATUM);
    }
    drawProgressRegion(t, st);

    dialogueBox(t, DLG_X, DLG_Y, DLG_W, DLG_H);
    resetLyricArea();   // box just repainted; main draws the current line next
}

}
