#include "../board.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"

namespace board {

static SPIClass s_sdSPI(VSPI);   // TFT on HSPI; VSPI stays the SD card's (see platformio.ini)

void begin() {}

bool sdMount() {
    s_sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    return SD.begin(SD_CS, s_sdSPI, 4000000, "/sd", 1);   // one file open at a time: the default 5 cost heap
}

}
