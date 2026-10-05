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

// Truncate a string with a trailing ellipsis so it fits within maxW pixels.
static String fitText(TFT_eSPI& t, const char* s, int maxW, uint8_t font) {
    String str = (s && s[0]) ? String(s) : String("");
    if (t.textWidth(str, font) <= maxW) return str;
    while (str.length() > 1) {
        str.remove(str.length() - 1);
        if (t.textWidth(str + "...", font) <= maxW) break;
    }
    return str + "...";
}

// HP bar now DEPLETES as the song plays: full at the start, empty at the end
// (the Pokemon "takes damage" over the song). Label shows remaining time.
void drawProgressRegion(TFT_eSPI& t, const AppState& st) {
    panel(t, 118, 116, 194, 36);
    uint32_t rem = (st.durationMs > st.progressMs) ? (st.durationMs - st.progressMs) : 0;
    uint32_t tot = st.durationMs;
    float hpFrac = st.durationMs ? (float)rem / (float)st.durationMs : 1.0f;
    // Pokémon-style "current/max": remaining time = current HP, total = max HP.
    char tbuf[28];
    snprintf(tbuf, sizeof(tbuf), "HP  %u:%02u/%u:%02u",
             rem / 60000, (rem / 1000) % 60, tot / 60000, (tot / 1000) % 60);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.setTextDatum(TL_DATUM);
    t.drawString(tbuf, 124, 120, 2);
    hpBar(t, 124, 138, 182, 9, hpFrac);
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

    // left: album art box (image drawn on top by main) + pokemon box
    panel(t, 8, 28, 104, 104);
    t.fillRect(11, 31, 98, 98, 0xBDD7); // placeholder until art is pushed over it
    panel(t, 8, 136, 104, 96);
    t.setTextDatum(MC_DATUM);
    // dex number at the top, name at the bottom (sprite is drawn between by main)
    if (st.pokedexNum > 0) {
        char no[12];
        snprintf(no, sizeof(no), "No.%03d", st.pokedexNum);
        t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
        t.drawString(no, 60, 143, 1);
    }
    t.setTextColor(accent, theme::GBA_CREAM);
    t.drawString(fitText(t, st.pokeName[0] ? st.pokeName : "Pokemon", 100, 2), 60, 218, 2);
    t.setTextDatum(TL_DATUM);

    // right column (all text truncated to the panel width)
    const int RW = 182;  // usable text width in the right column
    panel(t, 118, 28, 194, 44);
    t.setTextColor(theme::GBA_NAVY, theme::GBA_CREAM);
    t.drawString(fitText(t, st.trackName[0] ? st.trackName : "Track title", RW, 2), 124, 32, 2);
    t.drawString(fitText(t, st.artist[0] ? st.artist : "Artist", RW, 2), 124, 52, 2);

    panel(t, 118, 76, 194, 36);
    t.drawString("From:", 124, 80, 2);
    t.drawString(fitText(t, st.context[0] ? st.context : "Playlist", RW, 2), 124, 94, 2);

    drawProgressRegion(t, st);

    // controls
    NowButtons b = nowButtons();
    drawButton(t, b.prev, false);
    drawButton(t, b.play, st.isPlaying);
    drawButton(t, b.next, false);
    drawButton(t, b.vol,  false);
    drawButton(t, b.lyrics, false);
}
}
