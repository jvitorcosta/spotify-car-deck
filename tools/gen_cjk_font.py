"""Generate data/cjk16.bin: GNU Unifont glyphs for Japanese/Chinese text, embedded in flash.

Why: the built-in fonts are ASCII-only, so kana/hanzi lyric lines rendered empty. Unifont is
16 px tall (same as font 2); kana + CJK ideographs are ~686 KB of flash and 0 RAM.
Run: python tools/gen_cjk_font.py [path-or-url to unifont_all-*.hex.gz]
Format (little-endian): "UFNT", u16 nRanges, nRanges x {u32 first, u32 last, u8 width,
u32 offset}, then bitmaps (16 rows; 1 B/row for width 8, 2 B/row for width 16).
"""
import gzip
import io
import os
import struct
import sys
import urllib.request

SRC = sys.argv[1] if len(sys.argv) > 1 else \
    "https://ftpmirror.gnu.org/unifont/unifont-16.0.04/unifont_all-16.0.04.hex.gz"
OUT = os.path.join(os.path.dirname(__file__), "..", "data", "cjk16.bin")
WANT = [(0x3000, 0x303F), (0x3040, 0x309F), (0x30A0, 0x30FF), (0x31F0, 0x31FF),
        (0x4E00, 0x9FFF), (0xFF00, 0xFFEF)]

raw = urllib.request.urlopen(SRC).read() if SRC.startswith("http") else open(SRC, "rb").read()
glyphs = {}
for line in io.TextIOWrapper(gzip.GzipFile(fileobj=io.BytesIO(raw)), encoding="ascii"):
    cp, bm = line.strip().split(":")
    c = int(cp, 16)
    if any(a <= c <= b for a, b in WANT):
        glyphs[c] = bytes.fromhex(bm)          # 16 B (8 px wide) or 32 B (16 px wide)

# Split each wanted range into runs of equal width (missing codepoints: blank, full width).
runs = []
for a, b in WANT:
    start = a
    width = 8 if len(glyphs.get(a, b"\0" * 32)) == 16 else 16
    for c in range(a, b + 2):
        w = None if c > b else (8 if len(glyphs.get(c, b"\0" * 32)) == 16 else 16)
        if w != width:
            runs.append((start, c - 1, width))
            start, width = c, w
header = 4 + 2 + 13 * len(runs)
blob, ranges = bytearray(), []
for first, last, width in runs:
    per = 16 if width == 8 else 32
    ranges.append(struct.pack("<IIBI", first, last, width, header + len(blob)))
    for c in range(first, last + 1):
        g = glyphs.get(c, b"\0" * per)
        blob += g if len(g) == per else b"\0" * per
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "wb") as f:
    f.write(b"UFNT" + struct.pack("<H", len(runs)) + b"".join(ranges) + bytes(blob))
print("wrote %s: %d ranges, %d glyphs, %d bytes" % (OUT, len(runs), len(glyphs), header + len(blob)))
