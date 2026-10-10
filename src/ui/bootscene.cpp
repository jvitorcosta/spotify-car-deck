#include "bootscene.h"
#include <Arduino.h>
#include <WiFi.h>
#include "tjpgd.h"
#include <cmath>
#include <cstdlib>
#include "car_sprite.h"
#include "montage.h"
#include "theme.h"
#include "../audio/greeting.h"
#include "../core/mem.h"
#include "../util/bootanim.h"
#include "../util/bios.h"

extern const uint8_t _binary_data_montage_bin_start[] asm("_binary_data_montage_bin_start");
extern const uint8_t _binary_data_montage_bin_end[] asm("_binary_data_montage_bin_end");

namespace bootscene {
namespace {

using bootanim::hex565;

constexpr int W = 320, H = 240;
constexpr int SKY_H = 72;                        // static sky while cruising, drawn once
constexpr int STRIP_H = 24;
constexpr int STRIPS = (H - SKY_H) / STRIP_H;    // cruising: y 72-239 (7 strips)
constexpr int ALL_STRIPS = H / STRIP_H;          // brake fade: whole screen (10 strips)
static_assert(SKY_H % STRIP_H == 0 && H % STRIP_H == 0, "strips must tile the screen");
constexpr int GROUND_Y = 214;                    // the car's ground line
constexpr int CAR_TOP = GROUND_Y - 65;           // sprite row 0 (without the bob)
constexpr uint32_t FRAME_MS = 40;                // 25 fps target while drawing the pixel scene
constexpr uint32_t TERM_TTL = 3000;              // a terminal line disappears this long after its last change
constexpr int ROAD_SPEED = 90, FENCE_SPEED = 30;
constexpr int LAMP_PERIOD = 170, DASH_PERIOD = 48, BAY_PERIOD = 60, CARS_PERIOD = 450;
constexpr float TAU = 6.28318530718f;            // Arduino.h #defines TWO_PI
constexpr uint16_t UNDERGLOW = hex565(0x8C46FF), GLOW_CORE = hex565(0xBE8CFF);
constexpr uint16_t WHITE = 0xFFFF;
// The montage sound (data/finale.pcm) starts with the first montage frame, so something plays over
// the photos; false = end it with the fade to black (bootanim::finaleStartFrame).
constexpr bool MONTAGE_SOUND_AT_START = false;

TFT_eSPI* s_tft = nullptr;
bool (*s_online)() = nullptr;
volatile bool s_active = false;
const char* volatile s_caption = "";
volatile uint32_t s_captionGen = 0;

uint16_t* s_buf = nullptr;   // the strip being rendered (byte-swapped, as TFT_eSprite stores it)
int s_y0 = 0;                // screen y of the strip's first row
uint8_t s_treeTop[W];        // top y of the tree-line canopy per column
uint8_t s_haloLut[64];       // 140 x (1 - d)^1.5 for the underglow halo, d in 64 steps
uint8_t* s_wheelAng = nullptr;   // per wheel pixel (29 x 29): angle in 256 steps (heap, scene only)
uint8_t* s_wheelRing = nullptr;  // per wheel pixel: 0 out, 1 tyre, 2 lip, 3 hub centre, 4 hub, 5 face, 6 face with spokes
constexpr int WHEEL_D = 29;

// ---- strip drawing (everything clipped to the current strip) ----
inline void put(int x, int y, uint16_t c) { s_buf[(y - s_y0) * W + x] = theme::be(c); }
inline uint16_t get(int x, int y) { return theme::be(s_buf[(y - s_y0) * W + x]); }

void rect(int x, int y, int w, int h, uint16_t c) {
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h;
    if (!bootanim::clip(x0, x1, 0, W) || !bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) return;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) put(xx, yy, c);
}

void blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a) {
    if (!a) return;
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h;
    if (!bootanim::clip(x0, x1, 0, W) || !bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) return;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) put(xx, yy, bootanim::blend565(c, get(xx, yy), a));
}

inline void blendPx(int x, int y, uint16_t c, uint8_t a) {
    if (a && x >= 0 && x < W && y >= s_y0 && y < s_y0 + STRIP_H) put(x, y, bootanim::blend565(c, get(x, y), a));
}

void dimStrip(uint8_t level) {
    if (level == 255) return;
    for (int i = 0; i < W * STRIP_H; ++i) s_buf[i] = theme::be(bootanim::scale565(theme::be(s_buf[i]), level));
}

// Linear blend between two colours over a range of rows.
uint16_t lerp565(uint16_t a, uint16_t b, int num, int den) {
    return bootanim::blend565(b, a, (uint8_t)(255 * num / den));
}

// ---- sky ----
uint16_t skyAt(int y) {                          // #05070C at the top -> #0B1222 at 71 -> #0E1628 at 129
    if (y < SKY_H) return lerp565(hex565(0x05070C), hex565(0x0B1222), y, SKY_H - 1);
    return lerp565(hex565(0x0B1222), hex565(0x0E1628), y - SKY_H, 129 - SKY_H);
}
struct Star { int x, y; };
Star star(int i) { return {(i * 73) % W, (i * 37) % (SKY_H - 6)}; }
constexpr int STARS = 18;
constexpr uint16_t STAR = hex565(0x96A0BE);
constexpr int TERM_X = 4, TERM_Y = 4, TERM_ROW = 10;     // the on-board terminal, top left of the sky
constexpr int TERM_W = (bios::TERM_COLS + 2) * 6 + 2;
bool starSkipped(const Star& s) { return s.y < TERM_Y + bios::TERM_ROWS * TERM_ROW && s.x < TERM_X + TERM_W; }

// Cruising: sky straight on the display, once (the terminal draws itself on top).
void drawSky() {
    for (int y = 0; y < SKY_H; ++y) s_tft->drawFastHLine(0, y, W, skyAt(y));
    for (int i = 0; i < STARS; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, STAR);
    }
}

void twinkle(uint32_t t) {
    for (int i = 0; i < STARS; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, bootanim::starHidden(t, i) ? skyAt(s.y) : STAR);
    }
}

// Brake/fade-in: the sky rendered into the strip so it dims with the rest.
void drawSkyIntoStrip(uint32_t t) {
    for (int y = 0; y < SKY_H; ++y) rect(0, y, W, 1, skyAt(y));
    for (int i = 0; i < STARS; ++i) {
        const Star s = star(i);
        if (!starSkipped(s) && !bootanim::starHidden(t, i)) rect(s.x, s.y, 1, 1, STAR);
    }
}

// Tree line: 26 overlapping round canopies, tops y 86-110, deterministic; computed once.
void initTrees() {
    for (int x = 0; x < W; ++x) s_treeTop[x] = 126;
    for (int i = 0; i < 26; ++i) {
        const int cx = i * 13 + (i * 37) % 9 - 4, r = 10 + (i * 7) % 10, top = 86 + (i * 29) % 25;
        const int cy = top + r;
        const int hw = (int)(r * 1.2f);
        for (int x = cx - hw; x <= cx + hw; ++x) {
            if (x < 0 || x >= W) continue;
            const float u = (float)(x - cx) / 1.2f;
            const float d2 = (float)(r * r) - u * u;
            if (d2 < 0) continue;
            const int yt = (int)(cy - sqrtf(d2));
            if (yt < s_treeTop[x]) s_treeTop[x] = (uint8_t)yt;
        }
    }
}

// ---- the car park ----
void drawBackground(uint32_t t) {
    for (int y = SKY_H; y < 130; ++y) rect(0, y, W, 1, skyAt(y));         // sky continued
    const uint16_t tree = hex565(0x0A120E);                                // tree line (static)
    for (int y = s_y0 < 86 ? 86 : s_y0; y < s_y0 + STRIP_H && y < 126; ++y)
        for (int x = 0; x < W; ++x)
            if (y >= s_treeTop[x]) put(x, y, tree);
    rect(0, 126, W, 10, hex565(0x09100C));
    const int fo = bootanim::scroll(t, FENCE_SPEED, 1000);                // chain-link fence
    rect(0, 123, W, 1, hex565(0x32643F));
    for (int y = s_y0 < 124 ? 124 : s_y0; y < s_y0 + STRIP_H && y < 140; ++y)
        for (int x = 0; x < W; ++x)
            if ((x + fo + y) % 5 == 0 || (x + fo - y + 1000) % 5 == 0) put(x, y, bootanim::blend565(hex565(0x285A37), get(x, y), 90));
    const int co = bootanim::scroll(t, FENCE_SPEED, CARS_PERIOD);         // distant parked cars
    for (int k = 0; k < 3; ++k) {
        const int px = ((k * 150 + 60 - co) % CARS_PERIOD + CARS_PERIOD) % CARS_PERIOD - 40;
        rect(px, 132, 34, 8, hex565(0x1E2128));
        rect(px + 6, 127, 20, 6, hex565(0x1A1D24));
        rect(px + 1, 135, 1, 1, hex565(0xC81E1E));
        rect(px + 32, 135, 1, 1, hex565(0xC81E1E));
    }
    for (int y = s_y0 < 140 ? 140 : s_y0; y < s_y0 + STRIP_H; ++y) {      // near-black asphalt
        const uint16_t base = lerp565(hex565(0x0E0F13), hex565(0x12131A), y - 140, 99);
        const uint16_t grain[4] = {bootanim::scale565(base, 225), base,
                                   bootanim::blend565(WHITE, base, 5), bootanim::blend565(WHITE, base, 10)};
        for (int x = 0; x < W; ++x) put(x, y, grain[((uint32_t)(x * 73856093u) ^ (uint32_t)(y * 19349663u)) >> 13 & 3]);
    }
    const int bo = bootanim::scroll(t, ROAD_SPEED, BAY_PERIOD);           // parking-bay lines
    for (int bx = -BAY_PERIOD; bx < W + BAY_PERIOD; bx += BAY_PERIOD)
        for (int y = 150; y < 168; ++y) blendRect(bx - bo + (y - 150) / 3, y, 2, 1, hex565(0x46484E), 140);
    const int d = bootanim::scroll(t, ROAD_SPEED, DASH_PERIOD);           // lane dashes
    for (int x = -DASH_PERIOD; x < W; x += DASH_PERIOD) rect(x - d, 226, 22, 2, hex565(0x3C3E42));
    const int lo = bootanim::scroll(t, ROAD_SPEED, LAMP_PERIOD);          // white LED street lamps
    for (int lx = 40 - lo; lx < W + LAMP_PERIOD; lx += LAMP_PERIOD) {
        rect(lx, 74, 2, 76, hex565(0x242830));
        rect(lx, 72, 14, 2, hex565(0x242830));
        rect(lx + 10, 74, 6, 2, hex565(0xF5FAFF));
        for (int y = 76; y < 154; ++y) {
            const int hw = (y - 76) * 45 / 100;
            blendRect(lx + 13 - hw, y, 2 * hw + 1, 1, hex565(0xE1EBFF), 13);
        }
        for (int y = 150; y < 170; ++y) {
            if (y < s_y0 || y >= s_y0 + STRIP_H) continue;         // only this strip's rows
            const float dy = (float)(y - 160) / 9.0f, dy2 = dy * dy;
            for (int x = lx - 30; x < lx + 56; ++x) {
                const float dx = (float)(x - lx - 13) / 45.0f, dd = dx * dx + dy2;
                if (dd < 1.0f) blendPx(x, y, hex565(0xC8D2E6), (uint8_t)(26.0f * (1.0f - dd)));
            }
        }
    }
}

// Wheel geometry, computed once per scene: ring class and angle (256 steps) for each pixel.
bool initWheels() {
    s_wheelAng = (uint8_t*)malloc(WHEEL_D * WHEEL_D);
    s_wheelRing = (uint8_t*)malloc(WHEEL_D * WHEEL_D);
    if (!s_wheelAng || !s_wheelRing) {
        free(s_wheelAng);
        free(s_wheelRing);
        s_wheelAng = s_wheelRing = nullptr;
        return false;
    }
    for (int dy = -14; dy <= 14; ++dy)
        for (int dx = -14; dx <= 14; ++dx) {
            const int i = (dy + 14) * WHEEL_D + (dx + 14);
            const float d = sqrtf((float)(dx * dx + dy * dy));
            uint8_t ring;
            if (d > 13.5f) ring = 0;
            else if (d > 10.5f) ring = 1;                    // tyre
            else if (d > 9.5f) ring = 2;                     // rim lip
            else if (d <= 1.0f) ring = 3;                    // hub centre
            else if (d <= 2.3f) ring = 4;                    // hub
            else ring = d >= 2.5f ? 6 : 5;                   // rim face (6: where spokes can be)
            s_wheelRing[i] = ring;
            float a = atan2f((float)dy, (float)dx);
            if (a < 0) a += TAU;
            s_wheelAng[i] = (uint8_t)((int)(a * 256.0f / TAU) & 255);
        }
    return true;
}

// Per frame: spoke class per angle step for the current rotation (0 rim face, 1 machined face, 2 edge).
// 5 twin spokes at k * TAU/5 +- 0.17 rad, as before.
void spokeTable(float base, uint8_t* out) {
    const float period = TAU / 5;
    for (int k = 0; k < 256; ++k) {
        float m = fmodf((float)k * TAU / 256.0f - base, period);
        if (m < 0) m += period;
        const float dist = fminf(fabsf(m - 0.17f), fabsf(m - (period - 0.17f)));
        out[k] = dist < 0.10f ? 1 : dist < 0.18f ? 2 : 0;
    }
}

void wheel(int cx, int cy, const uint8_t* spokes, float glow) {
    static const uint16_t RING[7] = {0, hex565(0x141416), hex565(0xC9CED4), hex565(0x3A3E46), hex565(0xC8CCD2),
                                     hex565(0x2B2F36), hex565(0x2B2F36)};
    static const uint16_t SPOKE[3] = {hex565(0x2B2F36), hex565(0xDFE3E8), hex565(0x6A7078)};
    for (int dy = -14; dy <= 14; ++dy) {
        const int y = cy + dy;
        if (y < s_y0 || y >= s_y0 + STRIP_H) continue;
        const uint8_t g = dy > 4 ? (uint8_t)(fminf(128.0f, (dy - 4) * 13.0f) * glow) : 0;   // underglow on the tyre
        for (int dx = -14; dx <= 14; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= W) continue;
            const int i = (dy + 14) * WHEEL_D + (dx + 14);
            const uint8_t ring = s_wheelRing[i];
            if (!ring) continue;
            uint16_t c = ring == 6 ? SPOKE[spokes[s_wheelAng[i]]] : RING[ring];
            if (g) c = bootanim::blend565(UNDERGLOW, c, g);
            put(x, y, c);
        }
    }
}

struct Car { uint32_t t; int x; int top; uint8_t e; bool lamp; int lampX; float spoke; };

bool nearestLamp(uint32_t t, int carX, int* lampX) {
    const int lo = bootanim::scroll(t, ROAD_SPEED, LAMP_PERIOD);
    bool found = false;
    for (int lx = 40 - lo; lx < W + LAMP_PERIOD; lx += LAMP_PERIOD) {
        const int hx = lx + 13 - carX;
        if (hx <= -40 || hx >= 240) continue;
        if (!found || abs(hx - 100) < abs(*lampX - 100)) *lampX = hx;
        found = true;
    }
    return found;
}

Car makeCar(uint32_t world, int x, int bob, uint8_t e) {
    Car c{};
    c.t = world;
    c.x = x;
    c.top = CAR_TOP + bob;
    c.e = e;
    c.spoke = bootanim::spokeAngle(world);
    c.lamp = nearestLamp(world, x, &c.lampX);
    return c;
}

void drawCar(const Car& c) {
    const float glow = bootanim::glowAlpha(c.e) / 217.0f;            // 0.53 at rest .. 1.0 loud
    rect(c.x + 24, GROUND_Y, 154, 2, hex565(0x08090C));              // shadow
    const int cx = c.x + 100;                                         // underglow halo + core on the asphalt
    for (int y = 199; y < 228; ++y) {
        if (y < s_y0 || y >= s_y0 + STRIP_H) continue;
        const float hy = (float)(y - 213) / 14.0f, hy2 = hy * hy, ky = (float)(y - 211) / 5.0f, ky2 = ky * ky;
        for (int x = c.x - 26; x < c.x + 226; ++x) {
            const float dx = (float)(x - cx);
            const float d = dx * dx * (1.0f / (126.0f * 126.0f)) + hy2;
            if (d < 1.0f) blendPx(x, y, UNDERGLOW, (uint8_t)(s_haloLut[(int)(d * 64.0f)] * glow));
            if (ky2 < 1.0f) {
                const float k = dx * dx * (1.0f / (90.0f * 90.0f)) + ky2;
                if (k < 1.0f) blendPx(x, y, GLOW_CORE, (uint8_t)(115.0f * (1.0f - k) * glow));
            }
        }
    }
    blendRect(c.x + 30, 205, 142, 1, GLOW_CORE, (uint8_t)(230 * glow));   // LED tube under the sills
    blendRect(c.x + 30, 206, 142, 1, UNDERGLOW, (uint8_t)(205 * glow));
    const uint8_t ba = bootanim::beamAlpha(c.e);                      // headlight beam, cool white
    for (int i = 0; i < 90; ++i) {
        const int h = 3 + i * 35 / 100;
        blendRect(c.x + 200 + i, c.top + 31 - h / 3, 1, h, hex565(0xF0F5FF), (uint8_t)(ba * (90 - i) / 90));
    }
    blendRect(c.x - 4, c.top + 21, 6, 9, hex565(0xFF3030), bootanim::tailGlowAlpha(c.e));

    int x0 = c.x, x1 = c.x + car_sprite::width(), y0 = c.top, y1 = c.top + car_sprite::height();
    if (bootanim::clip(x0, x1, 0, W) && bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) {
        const uint16_t head = bootanim::headColor(c.e), refl = hex565(0xE6EEFF);
        const uint8_t tail = bootanim::tailLevel(c.e);
        for (int y = y0; y < y1; ++y) {
            const int row = y - c.top;
            const uint8_t purple = row > 44 ? (uint8_t)(fminf(115.0f, (row - 44) * 8.0f) * glow) : 0;
            for (int x = x0; x < x1; ++x) {
                uint16_t col;
                uint8_t kind;
                if (!car_sprite::pixel(x - c.x, row, &col, &kind)) continue;
                if (kind == car_sprite::HEAD) col = head;
                else if (kind == car_sprite::TAIL) col = bootanim::scale565(col, tail);
                else if (c.lamp && (kind == car_sprite::UPPER || kind == car_sprite::LOWER)) {
                    const uint8_t a = bootanim::reflectAlpha((x - c.x) - c.lampX, kind == car_sprite::UPPER);
                    if (a) col = bootanim::blend565(refl, col, a);
                }
                if (purple) col = bootanim::blend565(UNDERGLOW, col, purple);   // underglow on the lower body
                put(x, y, col);
            }
        }
    }
    if (s_wheelAng) {                                                  // wheels stay on the road (no bob)
        uint8_t spokes[256];
        spokeTable(c.spoke, spokes);
        const int wy = CAR_TOP + car_sprite::WHEEL_Y;
        wheel(c.x + car_sprite::REAR_WHEEL_X, wy, spokes, glow);
        wheel(c.x + car_sprite::FRONT_WHEEL_X, wy, spokes, glow);
    }
}

// ---- montage ----
// Decoded with the plain tjpgd functions (as images/art does), not the TJpg_Decoder class: that
// class's global object carries a 3.5 KB workspace in static RAM for good, which shrank the heap
// enough that Spotify's TLS buffers stopped fitting. The workspace here is borrowed per montage.
struct JpgSrc { const uint8_t* p; size_t len, pos; };

size_t jpgIn(JDEC* jd, uint8_t* buf, size_t n) {
    JpgSrc* src = (JpgSrc*)jd->device;
    if (n > src->len - src->pos) n = src->len - src->pos;
    if (buf) memcpy(buf, src->p + src->pos, n);
    src->pos += n;
    return n;
}

int jpgOut(JDEC*, void* bmp, JRECT* r) {
    s_tft->pushImage(r->left, r->top, r->right - r->left + 1, r->bottom - r->top + 1, (uint16_t*)bmp);
    return 1;
}

void drawJpg(uint8_t* work, const uint8_t* jpg, size_t len) {
    JpgSrc src{jpg, len, 0};
    JDEC jd = {};
    jd.swap = 1;                                 // big-endian RGB565, as pushImage sends it as-is
    if (jd_prepare(&jd, jpgIn, work, TJPGD_WORKSPACE_SIZE, &src) == JDR_OK) jd_decomp(&jd, jpgOut, 0);
}

// ---- BIOS colours (util/bios) ----
constexpr uint16_t BIOS_GREY = hex565(0xAAAAAA), BIOS_GREEN = hex565(0x55FF55), BIOS_YELLOW = hex565(0xFFFF55);

uint16_t biosTone(uint8_t tone) {
    switch (tone) {
    case bios::HEADER: return WHITE;
    case bios::OK: return BIOS_GREEN;
    case bios::WARN: return BIOS_YELLOW;
    case bios::PURPLE: return UNDERGLOW;
    default: return BIOS_GREY;
    }
}

// ---- on-board terminal: the BIOS log carried on in the sky during the drive ----
constexpr uint16_t TERM_TEXT = hex565(0x6FBF7F);
bios::Term s_term;                    // static: keeps ~210 B off the scene task's small stack
int s_termDrawn[bios::TERM_ROWS];     // characters drawn per row; -1 = redraw
uint32_t s_termGen = 0;

// Row i's text at t ("> " is drawn apart): returns the typed characters, *v the value column (-1 none).
int termRow(int i, uint32_t t, char* out, int* v) {
    const bios::TermLine& r = s_term.rows[i];
    *v = bios::format(r.label, r.value, out, bios::TERM_COLS + 1, bios::TERM_COLS);
    return t < r.at ? 0 : bios::typed(t - r.at, (int)strlen(r.label), (int)strlen(out));
}

// Draws row i through draw(text, x, colour): label + dots in terminal green, the value in its tone.
template <typename F>
void termText(int i, uint32_t t, F draw) {
    if (i >= s_term.n) return;
    char out[bios::TERM_COLS + 1], part[bios::TERM_COLS + 1];
    int v;
    const int n = termRow(i, t, out, &v);
    draw("> ", TERM_X, TERM_TEXT);
    const int split = v < 0 || n < v ? n : v;
    memcpy(part, out, split);
    part[split] = 0;
    draw(part, TERM_X + 12, TERM_TEXT);
    if (v >= 0 && n > v) {
        memcpy(part, out + v, n - v);
        part[n - v] = 0;
        draw(part, TERM_X + 12 + v * 6, biosTone(s_term.rows[i].tone));
    }
}

// Cruising: rows straight on the display, redrawn only when they change.
void termDrawDirect(uint32_t t) {
    const bool all = s_termGen != s_term.gen;
    s_termGen = s_term.gen;
    for (int i = 0; i < bios::TERM_ROWS; ++i) {
        char out[bios::TERM_COLS + 1];
        int v;
        const int n = i < s_term.n ? termRow(i, t, out, &v) : 0;
        if (!all && n == s_termDrawn[i]) continue;
        s_termDrawn[i] = n;
        const int y = TERM_Y + i * TERM_ROW;
        for (int yy = y; yy < y + 8; ++yy) s_tft->drawFastHLine(0, yy, TERM_X + TERM_W, skyAt(yy));
        termText(i, t, [&](const char* text, int x, uint16_t c) {
            s_tft->setTextColor(c);                  // one argument: transparent background
            s_tft->drawString(text, x, y, 1);
        });
    }
}

// Fade-in/brake: rows rendered into the current strip so they dim with the scene.
void termIntoStrip(TFT_eSprite& spr, uint32_t t) {
    for (int i = 0; i < s_term.n; ++i) {
        const int y = TERM_Y + i * TERM_ROW;
        if (y >= s_y0 + STRIP_H || y + 8 <= s_y0) continue;
        termText(i, t, [&](const char* text, int x, uint16_t c) {
            spr.setTextColor(c);
            spr.drawString(text, x, y - s_y0, 1);
        });
    }
}

// What the terminal says at t (ms since the scene started): the BIOS script, then the drive's own
// events. Lines scroll up and expire; WI-FI stays while it is still connecting.
void termEvents(uint32_t t, bool braking) {
    static bool online = false, engine = false, greet = false, done = false, parked = false;
    bios::termScript(s_term, t);
    if (t >= bios::liveAt() && !online) {
        if (s_online()) {
            const String ssid = WiFi.SSID();             // once: no String churn every frame
            char v[bios::TERM_VAL];
            const size_t k = ssid.length() < bios::TERM_VAL - 6 ? ssid.length() : bios::TERM_VAL - 6;
            memcpy(v, ssid.c_str(), k);
            memcpy(v + k, " [OK]", 6);
            bios::termSet(s_term, "WI-FI", v, bios::OK, t);
            online = true;
        } else {
            // main.cpp's caption mentions "retrying" once every network has failed (connectAny)
            bios::termSet(s_term, "WI-FI", strstr(s_caption, "retry") ? "RETRYING..." : "CONNECTING...",
                          bios::WARN, t, true);
        }
    }
    if (t >= 4800 && !engine) engine = bios::termSet(s_term, "ENGINE", "RUNNING", bios::OK, t);
    if (t >= 5300 && !greet && t < greeting::durationMs())
        greet = bios::termSet(s_term, "GREETING", "PLAYING", bios::PURPLE, t);
    if (greet && !done && t >= greeting::durationMs()) done = bios::termSet(s_term, "GREETING", "DONE", bios::OK, t);
    if (braking && !parked) parked = bios::termSet(s_term, "PARKING", "BRAKE", bios::OK, t);
    bios::termExpire(s_term, t, TERM_TTL);
}

// No strip buffer: caption on black, same exit rule, no animation.
void runWithoutScene(uint32_t t0) {
    uint32_t gen = s_captionGen - 1;
    for (;;) {
        if (gen != s_captionGen) {
            gen = s_captionGen;
            s_tft->fillScreen(TFT_BLACK);
            s_tft->setTextColor(TFT_WHITE, TFT_BLACK);
            s_tft->drawString(s_caption, 10, 10, 2);
        }
        const uint32_t t = millis() - t0;
        if (bootanim::mayExit(t, t >= greeting::durationMs(), s_online())) return;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void task(void*) {
    mem::log("bootscene start");
    const uint32_t t0 = millis();
    montage::parse(_binary_data_montage_bin_start,
                   (size_t)(_binary_data_montage_bin_end - _binary_data_montage_bin_start));
    initTrees();
    for (int k = 0; k < 64; ++k) {
        const float d = (float)k / 64.0f;
        s_haloLut[k] = (uint8_t)(140.0f * (1.0f - d) * sqrtf(1.0f - d));
    }
    initWheels();                                                // 1.7 KB, freed at the end
    TFT_eSprite spr(s_tft);
    spr.setColorDepth(16);
    s_buf = (uint16_t*)spr.createSprite(W, STRIP_H);
    if (!s_buf) {
        Serial.println("[bootscene] no memory for the strip buffer: scene skipped");
        runWithoutScene(t0);
    } else {
        drawSky();
        for (int& d : s_termDrawn) d = -1;
        uint32_t tHero = bootanim::NO_EXIT;
        int xc = 0, shown = -1;
        uint8_t* jpgWork = nullptr;                              // tjpgd workspace, montage only
        int finaleAt = -1;                                       // montage frame that starts the finale sound
        bool finaleStarted = false;
        uint32_t frames[3] = {0, 0, 0}, busyMs[3] = {0, 0, 0};   // drive, brake, montage
        TickType_t wake = xTaskGetTickCount();
        for (;;) {
            const uint32_t t = millis() - t0;
            if (tHero == bootanim::NO_EXIT && bootanim::mayExit(t, t >= greeting::durationMs(), s_online())) {
                tHero = t;
                xc = bootanim::cruiseX(t);
            }
            const uint32_t u = tHero == bootanim::NO_EXIT ? 0 : t - tHero;
            const uint32_t f0 = millis();
            const uint8_t e = greeting::level(t);
            int mode;
            termEvents(t, tHero != bootanim::NO_EXIT);
            if (tHero == bootanim::NO_EXIT) {                    // enter + cruise
                mode = 0;
                termDrawDirect(t);
                twinkle(t);
                const Car car = makeCar(t, bootanim::carX(t, bootanim::NO_EXIT), bootanim::bob(t), e);
                for (int s = 0; s < STRIPS; ++s) {
                    s_y0 = SKY_H + s * STRIP_H;
                    drawBackground(t);
                    drawCar(car);
                    spr.pushSprite(0, s_y0);
                }
            } else if (u < bootanim::BRAKE_MS) {                 // brake + fade the whole screen
                mode = 1;
                const uint32_t world = bootanim::brakeTime(tHero, u);
                const Car car = makeCar(world, bootanim::brakeX(xc, u), 0, e);
                const uint8_t lvl = bootanim::brakeLevel(u);
                for (int s = 0; s < ALL_STRIPS; ++s) {
                    s_y0 = s * STRIP_H;
                    if (s_y0 < SKY_H) {
                        drawSkyIntoStrip(world);
                        termIntoStrip(spr, t);
                    }
                    drawBackground(world);
                    drawCar(car);
                    dimStrip(lvl);
                    spr.pushSprite(0, s_y0);
                }
            } else {                                             // the owner's photo montage
                mode = 2;
                const int i = bootanim::montageFrame(u, montage::fps(), montage::count());
                if (i >= montage::count()) break;                // finished (or no montage)
                if (i != shown) {
                    if (shown < 0) {
                        spr.deleteSprite();                      // the strip isn't needed any more
                        s_buf = nullptr;
                        jpgWork = (uint8_t*)malloc(TJPGD_WORKSPACE_SIZE);
                        if (!jpgWork) break;                     // no memory: skip the montage
                        finaleAt = !greeting::finaleMs() ? -1
                                   : MONTAGE_SOUND_AT_START ? 0
                                   : bootanim::finaleStartFrame(montage::count(), montage::fps(), greeting::finaleMs());
                        if (bootanim::openerFits(finaleAt, montage::fps(), greeting::openerMs()))
                            greeting::playOpener();              // the montage's first sound
                    }
                    if (!finaleStarted && finaleAt >= 0 && i >= finaleAt) {   // the montage sound
                        greeting::playFinale();
                        finaleStarted = true;
                    }
                    const uint8_t* jpg;
                    size_t len;
                    if (montage::frame(i, &jpg, &len)) drawJpg(jpgWork, jpg, len);
                    shown = i;
                }
            }
            busyMs[mode] += millis() - f0;
            ++frames[mode];
            vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS));
        }
        // Let the finale finish (it ends with the last frame, ~0.1 s of DMA tail) so its I2S buffers
        // are gone before Spotify's TLS allocates; overlapping them fragmented the heap.
        for (int k = 0; k < 60 && greeting::playing(); ++k) vTaskDelay(pdMS_TO_TICKS(50));
        free(jpgWork);
        if (s_buf) spr.deleteSprite();
        s_buf = nullptr;
        Serial.printf("[bootscene] %u ms; render avg drive %u ms (%u frames), brake %u ms (%u), montage %u ms (%u of %d); stack free %u\n",
                      (unsigned)(millis() - t0),
                      (unsigned)(frames[0] ? busyMs[0] / frames[0] : 0), (unsigned)frames[0],
                      (unsigned)(frames[1] ? busyMs[1] / frames[1] : 0), (unsigned)frames[1],
                      (unsigned)(frames[2] ? busyMs[2] / frames[2] : 0), (unsigned)frames[2], montage::count(),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
    free(s_wheelAng);
    free(s_wheelRing);
    s_wheelAng = s_wheelRing = nullptr;
    mem::log("bootscene end");
    s_active = false;
    vTaskDelete(nullptr);
}

}  // namespace

void start(TFT_eSPI& tft, bool (*online)()) {
    s_tft = &tft;
    s_online = online;
    s_active = true;
    if (xTaskCreatePinnedToCore(task, "bootscene", 4096, nullptr, 1, nullptr, 1) != pdPASS) {
        s_active = false;                   // waitDone() and the net task would wait forever
        Serial.println("[bootscene] no memory for its task: scene skipped");
    }
}

void setCaption(const char* text) {
    s_caption = text;
    s_captionGen = s_captionGen + 1;
}

bool active() { return s_active; }

void waitDone() {
    while (s_active) delay(20);
}

}
