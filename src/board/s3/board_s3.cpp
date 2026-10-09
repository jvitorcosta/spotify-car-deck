#include "../board.h"
#include <Arduino.h>

namespace board {

void begin() {
    Serial.setTxTimeoutMs(0);   // native USB: never stall the deck when no PC is reading
    Serial.printf("[board] ESP32-S3 PSRAM %u KB (free %u KB), flash %u MB\n",
                  (unsigned)(ESP.getPsramSize() / 1024), (unsigned)(ESP.getFreePsram() / 1024),
                  (unsigned)(ESP.getFlashChipSize() >> 20));
}

bool sdMount() { return false; }   // the SDMMC slot is a later extra (spec section 4)

}
