/*
 * ES8311 playback setup. Register sequence and clock coefficients adapted from Espressif's
 * ES8311 driver (as shipped in Freenove's FNK0104 examples, es8311.cpp):
 *   SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *   SPDX-License-Identifier: Apache-2.0
 * Changes: Arduino Wire instead of the IDF I2C driver; playback only (16-bit I2S slave, MCLK
 * from the MCLK pin); only the 16 kHz clock rows this project uses.
 */
#include "es8311.h"
#include <Arduino.h>
#include <Wire.h>

namespace es8311 {
namespace {

struct Coeff {
    uint32_t mclk, rate;
    uint8_t preDiv, preMulti, adcDiv, dacDiv, fsMode, lrckH, lrckL, bclkDiv, adcOsr, dacOsr;
};
constexpr Coeff COEFFS[] = {
    {6144000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},   // 384 x fs (Freenove)
    {4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},   // 256 x fs
};

bool wr(uint8_t reg, uint8_t v) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    Wire.write(v);
    return Wire.endTransmission() == 0;
}

bool rd(uint8_t reg, uint8_t* v) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)ADDR, 1) != 1) return false;
    *v = (uint8_t)Wire.read();
    return true;
}

}

bool begin(int rate, int mclkHz) {
    const Coeff* c = nullptr;
    for (const Coeff& k : COEFFS)
        if (k.rate == (uint32_t)rate && k.mclk == (uint32_t)mclkHz) c = &k;
    if (!c) return false;
    uint8_t v = 0;
    if (!wr(0x00, 0x1F)) return false;                              // reset to defaults
    delay(20);
    bool ok = wr(0x00, 0x00) && wr(0x00, 0x80);                     // power-on command
    ok = ok && wr(0x01, 0x3F);                                      // MCLK from its pin, all clocks on
    ok = ok && rd(0x06, &v) && wr(0x06, v & ~0x20);                 // SCLK not inverted
    ok = ok && rd(0x02, &v) && wr(0x02, (v & 0x07) | ((c->preDiv - 1) << 5) | (c->preMulti << 3));
    ok = ok && wr(0x03, (c->fsMode << 6) | c->adcOsr) && wr(0x04, c->dacOsr);
    ok = ok && wr(0x05, ((c->adcDiv - 1) << 4) | (c->dacDiv - 1));
    ok = ok && rd(0x06, &v) && wr(0x06, (v & 0xE0) | (c->bclkDiv < 19 ? c->bclkDiv - 1 : c->bclkDiv));
    ok = ok && rd(0x07, &v) && wr(0x07, (v & 0xC0) | c->lrckH) && wr(0x08, c->lrckL);
    ok = ok && rd(0x00, &v) && wr(0x00, v & 0xBF);                  // serial port slave
    ok = ok && wr(0x09, 3 << 2) && wr(0x0A, 3 << 2);                // I2S, 16-bit in and out
    ok = ok && wr(0x0D, 0x01) && wr(0x0E, 0x02) && wr(0x12, 0x00);  // analog, PGA/modulator, DAC up
    ok = ok && wr(0x13, 0x10) && wr(0x1C, 0x6A) && wr(0x37, 0x08);  // output driver; EQs bypassed
    return ok && setVolume(0xBF);
}

bool setVolume(uint8_t reg32) { return wr(0x32, reg32); }

}
