#include "icons.h"
#include "theme.h"
#include <math.h>

namespace ui {

static inline uint16_t be(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

void drawIcon(TFT_eSPI& t, icons::Icon i, int x, int y, uint16_t fg, uint16_t bg) {
    uint16_t buf[icons::SIZE * icons::SIZE];
    for (int py = 0; py < icons::SIZE; ++py)
        for (int px = 0; px < icons::SIZE; ++px)
            buf[py * icons::SIZE + px] = be(icons::pixel(i, px, py) ? fg : bg);
    t.pushImage(x, y, icons::SIZE, icons::SIZE, buf);
}

void drawCd(TFT_eSPI& t, int cx, int cy, int frame, uint16_t bg) {
    t.fillRect(cx - 7, cy - 7, 15, 15, bg);
    t.fillCircle(cx, cy, 6, theme::CD_SILVER);
    t.drawCircle(cx, cy, 6, theme::ICON_OFF);
    float a = (frame & 3) * (float)M_PI / 4.0f;
    int dx = (int)lroundf(5 * cosf(a)), dy = (int)lroundf(5 * sinf(a));
    t.drawLine(cx - dx, cy - dy, cx + dx, cy + dy, TFT_WHITE);   // sheen
    t.fillCircle(cx, cy, 2, theme::TOP_DARK);                    // hub
}

}
