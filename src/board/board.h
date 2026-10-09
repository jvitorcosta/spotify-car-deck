#pragma once
// Per-board hooks; build_src_filter compiles board/cyd/ or board/s3/.
namespace board {
void begin();      // right after Serial.begin(): board-specific early setup and boot log
bool sdMount();    // mounts the SD card as the global `SD` at "/sd"; false = no card, no cache
}
