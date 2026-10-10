"""Fail when a firmware image fills too much of its app partition.

  python tools/check_flash.py <firmware.bin> <partition size, e.g. 0x300000> [max percent, default 97]

CI runs it after building with data/ stand-ins at the real media sizes (tools/media_sizes.py),
so a clip or montage that would no longer fit fails the build instead of the flash.
"""
import os
import sys

if len(sys.argv) < 3:
    sys.exit(__doc__)
image, part = sys.argv[1], int(sys.argv[2], 0)
limit = float(sys.argv[3]) if len(sys.argv) > 3 else 97.0
size = os.path.getsize(image)
pct = 100.0 * size / part
print("check_flash: %s %d B = %.1f %% of %d B (limit %.0f %%)" % (image, size, pct, part, limit))
if pct > limit:
    sys.exit("check_flash: over the limit: shrink the montage (tools/gen_montage.py) or the clips")
