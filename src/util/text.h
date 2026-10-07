#pragma once
#include <cstddef>
#include <cstdint>
namespace txt {
// Copy src -> dst (at most n bytes incl. NUL), converting UTF-8 Latin-1 accented
// letters to their ASCII base (á->a, ç->c, ã->a, É->E, ...). Other non-ASCII
// multibyte sequences are dropped. Needed because the display fonts are ASCII-only
// and Spotify returns names in UTF-8. Pure (no Arduino) so it is unit-tested.
void asciiFold(const char* src, char* dst, size_t n);
// Accent carried by a folded character, so the UI can draw it over the ASCII glyph.
enum class Mark : uint8_t { None, Acute, Grave, Circumflex, Tilde, Diaeresis, Cedilla, Ring };
// asciiFold that also records each output character's accent. `marks` (room for n entries)
// may be nullptr. Letters that fold to two characters (Æ, ß) carry no mark. Returns the
// folded length.
size_t foldMarks(const char* src, char* dst, Mark* marks, size_t n);
// Copy an identifier (e.g. a Spotify URI) into n bytes. If it does not fit, keep the start
// and end with "#" + 8 hex digits of a hash of the whole id, so two long ids sharing a
// prefix (local files from one album) stay distinct. nullptr -> "".
void copyId(const char* src, char* dst, size_t n);
}
