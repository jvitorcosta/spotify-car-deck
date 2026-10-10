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
if not os.path.isfile(header):
    # Fail loudly: a silent skip could ship firmware that prints the client secret.
    print("patch_spotify_lib: ERROR SpotifyArduino.h not found at " + header)
    env.Exit(1)  # noqa: F821
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
elif DISABLED not in lines:
    # Neither the define nor our patched line: the library changed (lib_deps pins the commit).
    print("patch_spotify_lib: ERROR SPOTIFY_DEBUG line not found in " + header + "; check it by hand")
    env.Exit(1)  # noqa: F821

# The token request bodies go into fixed stack buffers (char body[300]) with sprintf: a longer
# refresh token would overflow the network task's stack. snprintf truncates instead, and the
# refresh then fails cleanly. Idempotent.
source = os.path.join(libdeps, "SpotifyArduino", "src", "SpotifyArduino.cpp")
with open(source, encoding="utf-8") as f:
    code = f.read()
UNSAFE, SAFE = "sprintf(body, ", "snprintf(body, sizeof(body), "
if UNSAFE in code.replace(SAFE, ""):
    code = code.replace(SAFE, UNSAFE).replace(UNSAFE, SAFE)
    with open(source, "w", encoding="utf-8", newline="") as f:
        f.write(code)
    print("patch_spotify_lib: body buffers bounded (snprintf)")
elif SAFE not in code:
    print("patch_spotify_lib: ERROR sprintf(body, ...) not found in " + source + "; check it by hand")
    env.Exit(1)  # noqa: F821
