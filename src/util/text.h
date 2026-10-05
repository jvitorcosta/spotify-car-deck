#pragma once
#include <cstddef>
namespace txt {
// Copy src -> dst (at most n bytes incl. NUL), converting UTF-8 Latin-1 accented
// letters to their ASCII base (á->a, ç->c, ã->a, É->E, ...). Other non-ASCII
// multibyte sequences are dropped. Needed because the display fonts are ASCII-only
// and Spotify returns names in UTF-8. Pure (no Arduino) so it is unit-tested.
void asciiFold(const char* src, char* dst, size_t n);
}
