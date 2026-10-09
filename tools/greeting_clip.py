# PlatformIO pre-build script: make sure data/greeting.pcm (the embedded boot sound) exists.
#
# data/greeting.pcm is your own clip (tools/gen_greeting.py clip.wav) and is git-ignored.
# Without one, the committed default chime is copied into place so fresh clones and CI build.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os
import shutil

data = os.path.join(env.subst("$PROJECT_DIR"), "data")  # noqa: F821
clip = os.path.join(data, "greeting.pcm")
default = os.path.join(data, "greeting_default.pcm")
if not os.path.isfile(clip):
    shutil.copyfile(default, clip)
    print("greeting_clip: no data/greeting.pcm, using the default chime")
finale = os.path.join(data, "finale.pcm")      # boot-scene finale (tools/gen_greeting.py --finale ...)
if not os.path.isfile(finale):
    with open(finale, "wb") as f:      # one silent sample: the build cannot embed an empty file
        f.write(bytes(2))
    print("greeting_clip: no data/finale.pcm, the montage ends silently")
opener = os.path.join(data, "opener.pcm")      # montage opener (tools/gen_greeting.py --opener ...)
if not os.path.isfile(opener):
    with open(opener, "wb") as f:
        f.write(bytes(2))
    print("greeting_clip: no data/opener.pcm, the montage starts silently")
