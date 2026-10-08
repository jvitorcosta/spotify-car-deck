#include "backlight.h"
#include <Arduino.h>
#include "pins.h"

namespace backlight {

constexpr int CHANNEL = 1;          // LEDC channel (nothing else uses LEDC)
constexpr int FREQ_HZ = 5000;
constexpr int BITS = 8;
constexpr int DAY_LEVEL = 255;      // full brightness (auto-dim declined 2026-10-07)
constexpr int NIGHT_LEVEL = 90;     // ~35 %; tune on the panel

void begin() {
    ledcSetup(CHANNEL, FREQ_HZ, BITS);
    ledcAttachPin(PIN_BL, CHANNEL);
    ledcWrite(CHANNEL, DAY_LEVEL);
}

void set(bool night) { ledcWrite(CHANNEL, night ? NIGHT_LEVEL : DAY_LEVEL); }

}
