# Japanese / Chinese Text (bundled Unifont CJK) — Design

Date: 2026-10-07. Extends `2026-10-06-merged-status-panel-pmd-walker-design.md` §2.0
("Accented letters"). Approved scope: kana + CJK ideographs from GNU Unifont, no Korean.

## 1. Problem

The fonts are ASCII-only and `txt::foldMarks` drops every 3-byte UTF-8 character, so
Japanese/Chinese lyric lines (and titles) render empty or as their ASCII leftovers. LRCLIB
returns them correctly.

## 2. Memory (the deciding constraint)

| Resource | Budget | CJK cost |
|---|---|---|
| Flash (`huge_app.csv`, 3 MB app) | 1.33 MB used (42 %) | ~690 KB glyphs → ~2.0 MB (65 %) |
| Byte-addressable RAM | the scarce resource (Part 3) | **0** for glyphs: read from flash on demand |
| Lyrics arena (4 KB text, static) | unchanged | CJK is 3 B/char → ~1 350 chars; typical song 1.2–2.7 KB |
| LRCLIB transient JSON | gated by `canRun` (≥ 20 KB block) | ~4–6 KB peak, same order as today |

Glyphs are embedded as a **binary blob in flash** (`board_build.embed_files`), accessed through
the memory-mapped flash cache. No glyph cache in RAM, no lookup tables in RAM.

## 3. Font data

- Source: GNU Unifont `.hex` (16-px tall bitmaps: 8 px half-width, 16 px full-width;
  GPLv2+ with the font-embedding exception / SIL OFL 1.1). Credited in `CREDITS.md`.
- `tools/gen_cjk_font.py` (dev-time, standard library) extracts these ranges and writes
  `data/cjk16.bin` (committed):
  - U+3000–303F CJK symbols & punctuation, U+3040–309F Hiragana, U+30A0–30FF Katakana,
    U+31F0–31FF Katakana ext., U+FF00–FFEF half/full-width forms, U+4E00–9FFF CJK Unified
    Ideographs.
- Blob format (little-endian): `"UFNT"`, `u16 nRanges`, then per range
  `{u32 first, u32 last, u8 width (8|16), u32 offset}`, then the bitmaps (16 rows;
  1 byte/row for width 8, 2 bytes/row for width 16, MSB = leftmost pixel). Codepoints inside
  a range that Unifont lacks are stored blank. The generator splits the requested ranges into
  runs of equal width (U+FF00–FFEF mixes 8- and 16-px glyphs), so each stored range has one
  width. Measured on Unifont 16.0.04: all 21 504 codepoints present (21 381 × 16 px, 123 × 8 px)
  ≈ 686 KB. Source: `https://ftpmirror.gnu.org/unifont/unifont-16.0.04/unifont_all-16.0.04.hex.gz`.
- Pure `ui/cjkfont`: `Font::open(blob, len)` validates the header; `glyph(cp, &width)` →
  bitmap pointer or nullptr (binary search over ≤ 8 ranges, then offset arithmetic).
  Host-tested with a synthetic blob.

## 4. Text pipeline

- Pure `util/glyphrun`: decodes UTF-8 into items — ASCII (with optional accent `txt::Mark`,
  as today), **Wide** (a CJK/kana codepoint the font covers), or dropped (anything else:
  emoji, Korean, …). Measures widths with a caller-supplied ASCII width function (font 2)
  and the font's widths for Wide items. Wraps into up to two lines: prefer breaking at a
  space; between Wide items any boundary is a valid break (CJK has no spaces); the second
  line gets "..." when cut. Upper-casing touches ASCII items only.
- `ui/textdraw` draws runs: ASCII items through the existing font-2 glyphs + accent marks,
  Wide items as 16-row bitmaps with the same 1-px shadow; a per-font vertical offset aligns
  Unifont's baseline with font 2's (tuned on the panel).
- Used by the lyric dialogue box (two lines) and the info box (title, artist, context) and the
  device name; widths/truncation stay pixel-exact.

## 5. Testing

- Host: `test_cjkfont` (header validation, range lookup incl. gaps/out-of-range, half vs full
  width), `test_glyphrun` (UTF-8 decode incl. 4-byte/emoji dropped, mixed ASCII+kana+hanzi
  widths, wrap at spaces vs between CJK, ellipsis, upper-case only ASCII).
- Device: a Japanese and a Chinese song show their lyric lines and titles; mixed lines sit on
  one baseline; `[mem]` unchanged; flash usage reported by the build.

## 6. Out of scope

Korean (Hangul), vertical text, ruby/furigana, right-to-left scripts, emoji.
