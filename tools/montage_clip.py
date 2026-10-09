# PlatformIO pre-build script: make sure data/montage.bin (the boot scene's photo montage) exists.
#
# data/montage.bin is built from the owner's photos by tools/gen_montage.py and is git-ignored
# (the photos show the number plate and the repo is public). Without one, write an empty montage
# ("MTG1", 0 frames, 10 fps) so fresh clones and CI build; the boot scene then skips the montage.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

path = os.path.join(env.subst("$PROJECT_DIR"), "data", "montage.bin")  # noqa: F821
if not os.path.isfile(path):
    with open(path, "wb") as f:
        f.write(b"MTG1" + (0).to_bytes(2, "little") + (10).to_bytes(2, "little"))
    print("montage_clip: no data/montage.bin, building without the photo montage")
