"""Generate src/ui/car_sprite.inc: the boot scene's car, a side view of a silver Honda City sedan
(7th gen, Brazil 2024) facing right, 200 x 66 px, built from geometry measured off reference
photos (docs/superpowers/specs/2026-10-09-boot-scene-design.md, "Car sprite").
Side profile measured from "2024 Honda City RS - Sideview" (Wikimedia Commons, CC BY-SA 4.0).
Run: python tools/gen_car_sprite.py [--png out.png]   (output is committed)
Wheels are not in the sprite (drawn in code so they spin). Standard library only.
"""
import math
import os
import struct
import sys
import zlib

W, H = 200, 66
RWX, FWX, WCY = 46, 160, 51.5            # wheel centres (x rear, x front, y)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "ui", "car_sprite.inc")

# Pixel kinds (match ui/car_sprite.h)
NONE, UPPER, LOWER, OTHER, HEAD, TAIL = 0, 1, 2, 3, 4, 5


def hexrgb(s):
    return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))


C = {k: hexrgb(v) for k, v in dict(
    mid="#a7afb8", body="#bcc3cb", hi="#eef2f5", hi2="#d5dbe1", lo="#959ea8", shade="#7c858f",
    deep="#5d656e", dark="#22262e", glass="#0c1016", glassHi="#2c3a4a", trim="#2c3038",
    chrome="#e8ecef", liner="#0e1016", housing="#28323f", led="#e8f2ff",
    drl="#ffffff", tailTop="#ff5050", tailLow="#b81414", tailLed="#ff8080", reflector="#a01010",
    amber="#ffb030", fog="#ffffff", fogRing="#cfe8ff").items()}


def in_poly(px, py, poly):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def arc(cx, r):
    """Wheel-arch points from the front (cx + r) to the rear (cx - r), centre (cx, WCY + 2.5)."""
    return [(cx + math.cos(math.radians(a)) * r, WCY + 2.5 - math.sin(math.radians(a)) * r)
            for a in range(0, 181, 10)]


BODY = ([(4, 51), (1, 46), (0, 38), (0, 29), (1, 22), (3, 18), (20, 16.4), (24, 16), (40, 9.6), (56, 4.2),
         (70, 1.9), (86, 1.0), (100, 1), (112, 2), (120, 4), (134, 13), (144, 19.6), (150, 19.4),
         (160, 19.4), (170, 19.8), (178, 20.8), (184, 22.2), (188, 23.8), (192, 25.8), (195, 28.0),
         (197.5, 30.6), (199.3, 34), (200, 40), (199, 47), (196, 52), (186, 54),
         (178, 54)] + arc(FWX, 17.5) + [(64, 55)] + arc(RWX, 17.5) + [(12, 52)])
GLASS = [(44, 15.0), (56, 6.6), (72, 3.6), (86, 3.0), (100, 3.0), (111, 3.8), (119, 6.0), (133, 15.2),
         (139, 19.9)]
TAIL_POLY = [(0, 24.5), (2, 22), (18, 23.5), (21, 26), (18, 28.5), (1, 29)]
HEAD_POLY = [(164, 25.2), (186, 27.4), (197, 29.8), (199.5, 34.5), (191, 34.5), (176, 30.4)]
INTAKE = [(186, 44), (197, 41.5), (198, 50), (189, 50.5)]


def shoulder_y(x):      # the sharp shoulder crease, nearly level, very slightly rising to the rear
    return 26.6 - (x - 16) * 0.005


def crease_y(x):        # lower door crease rising toward the rear wheel (steeper, per the side photo)
    return 47 - (140 - x) * 0.085


def belt_y(x):           # window bottom / beltline: rises toward the rear
    return 20.0 - (139 - x) * 0.053


def build():
    px = [[None] * W for _ in range(H)]     # (rgb, kind) or None

    def P(x, y, col, kind=OTHER):
        x, y = int(x), int(y)
        if 0 <= x < W and 0 <= y < H:
            px[y][x] = (C[col] if isinstance(col, str) else col, kind)

    def fill(poly, fn):
        for y in range(H):
            for x in range(W):
                if in_poly(x + 0.5, y + 0.5, poly):
                    fn(x, y)

    # paint, banded top to bottom
    def paint(x, y):
        s = shoulder_y(x)
        if y < belt_y(x):
            P(x, y, "mid", UPPER)
        elif y < s - 2:
            P(x, y, "hi2", UPPER)
        elif y < s:
            P(x, y, "hi", UPPER)
        elif y < s + 2:
            P(x, y, "shade", LOWER)
        elif y < crease_y(x):
            P(x, y, "body", LOWER)
        elif y < 52:
            P(x, y, "lo", LOWER)
        else:
            P(x, y, "deep", LOWER)
    fill(BODY, paint)
    for x in range(74, 116):
        P(x, 1, "hi", UPPER)
    for x in range(88, 108):
        P(x, 2, "hi", UPPER)
    # shark-fin antenna
    for x in range(75, 83):
        top = 2 - (x - 75) * 0.4 if x < 80 else 0
        for y in range(round(top), 2):
            P(x, y, "trim")
    # side glass, black B-pillar, quarter-glass divider, reflections, chrome beltline
    fill(GLASS, lambda x, y: P(x, y, "glass"))
    for y in range(3, 21):
        for x in range(86, 90):
            if y < belt_y(x) + 1:
                P(x, y, "trim")
    for y in range(5, 17):
        P(round(56 - (y - 5) * 0.2), y, "trim")
    for k in range(10):
        P(117 + k, 8 + k, "glassHi")
        P(118 + k, 8 + k, "glassHi")
    for k in range(7):
        P(66 + k, 6 + k, "glassHi")
    for x in range(45, 139):
        P(x, round(belt_y(x) + 0.6), "chrome")
    # lower door crease, side skirt
    for x in range(66, 141):
        P(x, round(crease_y(x)), "shade", LOWER)
    for x in range(63, 144):                      # side skirt (owner's side photo)
        P(x, 51, "hi2", LOWER)                    # light top edge of the skirt
        P(x, 52, "lo", LOWER)
        P(x, 53, "shade", LOWER)
        P(x, 54, "dark")                          # dark lower lip
        if 66 <= x <= 140:
            P(x, 55, "trim")                      # lip stands 1 px proud of the sill
    # door shut lines: front door, B-pillar, rear door curving round the rear arch
    for y in range(21, 54):
        P(141, y, "deep", LOWER)
    for y in range(18, 54):
        P(88, y, "deep", LOWER)
    for y in range(16, 34):
        P(43, y, "deep", LOWER)
    for y in range(34, 40):
        P(round(43 + (y - 34) * 1.2), y, "deep", LOWER)
    # hood / fender shut line, following the domed hood edge (owner's side photo)
    for x in range(146, 176):
        P(x, round(21.4 + (x - 146) * 0.055), "lo", UPPER)
    # door handles on the shoulder line
    for hx in (97, 51):
        for x in range(hx, hx + 7):
            P(x, round(shoulder_y(x)), "hi", UPPER)     # silver, body-coloured handles
            P(x, round(shoulder_y(x)) + 1, "shade")
    # fuel door on the rear quarter
    for y in range(19, 25):
        for x in range(32, 39):
            if y in (19, 24) or x in (32, 38):
                P(x, y, "shade", LOWER)
    # door-mounted mirror with an amber LED indicator
    for y in range(16, 22):
        for x in range(135, 143):
            if not (y == 16 and (x < 137 or x > 140)):
                P(x, y, "hi" if y < 18 else "body", UPPER)
    for x in range(136, 142):
        P(x, 21, "amber")
    for y in range(19, 23):
        P(141, y, "trim")
    # owner's trunk lip spoiler (body-coloured ducktail)
    for x in range(2, 15):
        P(x, 16, "body", UPPER)
        P(x, 17, "shade", UPPER)
    for x in range(2, 9):
        P(x, 15, "hi", UPPER)
    P(1, 16, "deep", UPPER)
    # wrap-around taillight with the Z-shaped LED strip, bumper reflector
    fill(TAIL_POLY, lambda x, y: P(x, y, "tailTop" if y < 25 else "tailLow", TAIL))
    for x in range(2, 17):
        P(x, 26, "tailLed", TAIL)
    P(17, 25, "tailLed", TAIL)
    P(18, 26, "tailLed", TAIL)
    for y in range(43, 48):
        P(1, y, "reflector")
        P(2, y, "reflector")
    # slim smoked headlight: chrome top edge, LED projector row, DRL strip; chrome bar; intake
    fill(HEAD_POLY, lambda x, y: P(x, y, "housing"))
    for x in range(166, 198):                     # DRL strip along the top edge (owner's photos)
        P(x, round(26.0 + (x - 166) * 0.135), "drl", HEAD)
    for x in range(180, 197, 3):                  # LED projectors under it
        y = round(28.6 + (x - 180) * 0.16)
        P(x, y, "led", HEAD)
        P(x + 1, y, "led", HEAD)
    for x in range(193, 200):
        P(x, 35, "chrome")
    fill(INTAKE, lambda x, y: P(x, y, "trim"))     # bumper corner recess
    for y in range(41, 51):                       # lit LED fog lamp (owner's photos)
        for x in range(186, 199):
            d = math.hypot(x + 0.5 - 192.5, y + 0.5 - 46)
            if d < 2.4:
                P(x, y, "fog", HEAD)
            elif d < 3.4 and px[y][x]:
                P(x, y, "fogRing", HEAD)
    for x in range(184, 196):
        P(x, 52, "trim")
    # wheel arches: very dark liner, lighter lip
    for wx in (RWX, FWX):
        for y in range(59):
            for x in range(W):
                d = math.hypot(x + 0.5 - wx, y + 0.5 - (WCY + 2.5))
                if d < 17.5 and (px[y][x] or d < 16.6):
                    P(x, y, "deep" if d > 16.6 else "liner", OTHER)
    # front bumper: air-curtain slot at the corner and a dark lower lip under the nose
    for y in range(37, 44):
        P(197, y, "trim")
        P(198, y, "dark")
    for x in range(178, 199):
        P(x, 53, "trim")
    for x in range(181, 197):
        P(x, 54, "dark")
    # rear bumper: crease line and a dark diffuser with small vertical fins
    for x in range(2, 15):
        P(x, round(40 + (x - 2) * 0.15), "shade", LOWER)
    for x in range(3, 24):
        P(x, 50, "trim")
        P(x, 51, "dark")
    for fx in (8, 13, 18):
        P(fx, 49, "trim")
    # fender-flare highlight along the upper edge of each wheel arch
    for wx in (RWX, FWX):
        for a in range(200, 341, 4):
            t = math.radians(a)
            x = int(wx + math.cos(t) * 18.2)
            y = int(WCY + 2.5 + math.sin(t) * 18.2)
            if 0 <= y < H and 0 <= x < W and px[y][x] and px[y][x][1] in (UPPER, LOWER):
                P(x, y, "hi2", LOWER)
    # spoiler: crisper ducktail - bright top, body, dark shadow line under the lip
    for x in range(2, 15):
        P(x, 18, "deep", UPPER)
    for x in range(2, 12):
        P(x, 14, "hi", UPPER)
    return px


def rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def write_inc(px):
    vals, kinds = [], []
    for y in range(H):
        for x in range(W):
            p = px[y][x]
            vals.append(rgb565(p[0]) if p else 0)
            kinds.append(p[1] if p else NONE)
    packed = [(kinds[i] << 4) | (kinds[i + 1] if i + 1 < len(kinds) else 0) for i in range(0, len(kinds), 2)]
    lines = ["// Generated by tools/gen_car_sprite.py (Honda City sedan, side view). Do not edit.",
             "static const int CAR_W = %d, CAR_H = %d;" % (W, H),
             "static const uint16_t CAR_PX[%d] = {" % len(vals)]
    for i in range(0, len(vals), 12):
        lines.append("    " + ", ".join("0x%04X" % v for v in vals[i:i + 12]) + ",")
    lines += ["};", "// Two 4-bit pixel kinds per byte, high nibble first (see car_sprite.h).",
              "static const uint8_t CAR_KIND[%d] = {" % len(packed)]
    for i in range(0, len(packed), 16):
        lines.append("    " + ", ".join("0x%02X" % v for v in packed[i:i + 16]) + ",")
    lines.append("};")
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("gen_car_sprite: %s  %dx%d, %d opaque px" % (os.path.relpath(OUT), W, H, sum(1 for k in kinds if k)))


def write_png(px, path, scale=4, bg=(0x1A, 0x25, 0x64)):
    rows = []
    for y in range(H):
        row = b"".join(bytes(px[y][x][0] if px[y][x] else bg) * scale for x in range(W))
        rows += [b"\x00" + row] * scale

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W * scale, H * scale, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


if __name__ == "__main__":
    pixels = build()
    if "--png" in sys.argv:
        write_png(pixels, sys.argv[sys.argv.index("--png") + 1])
    else:
        write_inc(pixels)
