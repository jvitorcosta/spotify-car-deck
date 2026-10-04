#include "screen_now.h"
#include "widgets.h"
#include "theme.h"

namespace ui {
NowButtons nowButtons() {
    NowButtons b;
    int y = 206, h = 30;
    b.prev   = {8,   y, 46, h, "<<"};
    b.play   = {58,  y, 56, h, ">II"};
    b.next   = {118, y, 46, h, ">>"};
    b.vol    = {168, y, 44, h, "VOL"};
    b.lyrics = {216, y, 96, h, "LYRICS"};
    return b;
}

void drawNow(TFT_eSPI& t, const AppState& st, uint16_t accent) {
    t.fillScreen(0x6ADC);  // GBA sky blue background

    // top bar
    t.fillRect(0, 0, 320, 22, theme::GBA_NAVY);
    t.setTextColor(theme::GBA_CREAM, theme::GBA_NAVY);
    t.drawString("NOW PLAYING", 8, 5, 2);
    t.setTextDatum(TR_DATUM);
    t.drawString(st.deviceName[0] ? st.deviceName : "device", 312, 5, 2);
    t.setTextDatum(TL_DATUM);

    // left: album art box + pokemon box (placeholders here; images in later tasks)
    panel(t, 8, 28, 104, 104);
    t.fillRect(11, 31, 98, 98, 0xBDD7); // placeholder art
    panel(t, 8, 136, 104, 96);
    t.setTextColor(accent, theme::GBA_CREAM);
    t.setTextDatum(MC_DATUM);
    t.drawString(st.pokeName[0] ? st.pokeName : "Pokemon", 60, 210, 2);
    t.setTextDatum(TL_DATUM);

    // right column
    panel(t, 118, 28, 194, 44);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.drawString(st.trackName[0] ? st.trackName : "Track title", 124, 32, 2);
    t.drawString(st.artist[0] ? st.artist : "Artist", 124, 52, 2);

    panel(t, 118, 76, 194, 36);
    t.drawString("From:", 124, 80, 2);
    t.drawString(st.context[0] ? st.context : "Playlist", 124, 94, 2);

    panel(t, 118, 116, 194, 36);
    float frac = st.durationMs ? (float)st.progressMs / st.durationMs : 0;
    hpBar(t, 124, 138, 182, 9, frac);
    char tbuf[32];
    snprintf(tbuf, sizeof(tbuf), "%u:%02u  CP %d",
             st.progressMs/60000, (st.progressMs/1000)%60, st.popularity);
    t.drawString(tbuf, 124, 120, 2);

    // controls
    NowButtons b = nowButtons();
    drawButton(t, b.prev, false);
    drawButton(t, b.play, st.isPlaying);
    drawButton(t, b.next, false);
    drawButton(t, b.vol,  false);
    drawButton(t, b.lyrics, false);
}
}
