#pragma once
#include "cjkfont.h"
namespace ui {
// The Unifont CJK font embedded in flash (data/cjk16.bin). Empty font if the blob is invalid.
const cjkfont::Font& cjk();
}
