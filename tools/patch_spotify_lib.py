# PlatformIO pre-build script: turn off SpotifyArduino's SPOTIFY_DEBUG.
#
# The library #defines SPOTIFY_DEBUG in its own header (so a -D/-U build flag can't undo it),
# and in debug mode it prints the token request body over serial on every refresh:
# refresh token, client id and client secret. Anyone with a USB cable could read them.
# Library deps are installed before extra scripts run, so the header is present here.
# Idempotent: only rewrites the line once.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

DEFINE = "#define SPOTIFY_DEBUG 1"
DISABLED = "// #define SPOTIFY_DEBUG 1  // disabled by tools/patch_spotify_lib.py (prints secrets)"

libdeps = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))  # noqa: F821
header = os.path.join(libdeps, "SpotifyArduino", "src", "SpotifyArduino.h")
if os.path.isfile(header):
    with open(header, encoding="utf-8") as f:
        lines = f.read().split("\n")
    changed = False
    for i, line in enumerate(lines):
        if line.strip() == DEFINE:
            lines[i] = DISABLED
            changed = True
    if changed:
        with open(header, "w", encoding="utf-8", newline="") as f:
            f.write("\n".join(lines))
        print("patch_spotify_lib: SPOTIFY_DEBUG disabled")
else:
    print("patch_spotify_lib: WARNING SpotifyArduino.h not found at " + header)
