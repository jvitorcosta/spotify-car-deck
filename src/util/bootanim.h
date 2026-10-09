#pragma once
#include <cstdint>
// Boot scene motion, light and colour maths (docs/superpowers/specs/2026-10-09-boot-scene-design.md).
// PURE, host-tested. `t` is ms since the scene started; colours are native RGB565.
namespace bootanim {

constexpr uint32_t ENTER_MS = 900, EXIT_MS = 900, MIN_SHOW_MS = 5000, SWAY_MS = 4000;
constexpr int START_X = -202, CRUISE_X = 60, END_X = 330, SWAY_PX = 6;
constexpr uint32_t NO_EXIT = 0xFFFFFFFFu;   // tExit while the car hasn't started leaving

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t hex565(uint32_t rgb) {
    return rgb565((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
}

int cruiseX(uint32_t t);                      // sway around CRUISE_X, phase 0 at ENTER_MS
int carX(uint32_t t, uint32_t tExit);         // car's left edge on screen
bool exitDone(uint32_t t, uint32_t tExit);    // the car has left the screen
bool mayExit(uint32_t t, bool soundDone, bool online);
int bob(uint32_t t);                          // 1 = body 1 px lower this frame
float spokeAngle(uint32_t t);                 // radians
int scroll(uint32_t t, int pxPerS, int period);   // left scroll offset in [0, period)
bool starHidden(uint32_t t, int i);

uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha);   // alpha 255 = fg
uint16_t scale565(uint16_t c, uint8_t level);                  // brightness, 255 = unchanged
uint16_t hue565(uint32_t t);                  // underglow colour: hsl(t * 120 / 1000, 100 %, 58 %)

// Lights from the clip loudness e (0-255; 0 = resting look).
uint16_t headColor(uint8_t e);                // headlight LED/DRL pixels
uint8_t tailLevel(uint8_t e);                 // taillight brightness for scale565
uint8_t tailGlowAlpha(uint8_t e);             // red glow behind the taillight (0 when e <= 12)
uint8_t beamAlpha(uint8_t e);                 // headlight beam alpha at the lamp
uint8_t glowAlpha(uint8_t e);                 // underglow alpha
// Streetlight reflection on paint `d` px (horizontally) from the lamp; upper = above the shoulder line.
uint8_t reflectAlpha(int d, bool upper);

// Trims [a, b) to [lo, hi); false when nothing is left.
bool clip(int& a, int& b, int lo, int hi);

// ---- ending: brake (v2), then the photo montage (v3); u = ms since tHero ----
constexpr uint32_t BRAKE_MS = 600;
int brakeX(int xc, uint32_t u);                  // rolls 40 px from xc and stops (ease-out)
uint32_t brakeTime(uint32_t tHero, uint32_t u);  // braking clock for scroll and spokes
uint8_t brakeLevel(uint32_t u);                  // whole-frame brightness 255 -> 0

// ---- v3 montage: frame to show u ms after tHero (-1 while braking, count when finished) ----
int montageFrame(uint32_t u, int fps, int count);
// Montage frame at which the finale sound must start so it ends with the last frame (the fade to
// black); 0 if it's longer than the montage, -1 when there's no montage or no finale.
int finaleStartFrame(int count, int fps, uint32_t finaleMs);
// Whether the opener sound (played on the first montage frame) ends before the finale starts at
// montage frame finaleAt (-1 = no finale); false when there's no opener clip.
bool openerFits(int finaleAt, int fps, uint32_t openerMs);
}
