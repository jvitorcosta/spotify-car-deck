#include "cjkdata.h"
#include <Arduino.h>

extern const uint8_t _binary_data_cjk16_bin_start[] asm("_binary_data_cjk16_bin_start");
extern const uint8_t _binary_data_cjk16_bin_end[] asm("_binary_data_cjk16_bin_end");

namespace ui {

const cjkfont::Font& cjk() {
    static cjkfont::Font font;
    static bool opened = false;
    if (!opened) {
        opened = true;
        size_t len = (size_t)(_binary_data_cjk16_bin_end - _binary_data_cjk16_bin_start);
        if (!font.open(_binary_data_cjk16_bin_start, len))
            Serial.printf("[cjk] font blob invalid (%u bytes)\n", (unsigned)len);
    }
    return font;
}

}
