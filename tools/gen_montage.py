"""Build data/montage.bin: the boot scene's photo montage from the owner's photos
(docs/superpowers/specs/2026-10-09-boot-scene-v3-design.md, section 4).
Run: python tools/gen_montage.py [photo dir]      (default: montage-photos/)
The photos and data/montage.bin are git-ignored (they show the number plate). Needs ffmpeg.

Format (little-endian): "MTG1", u16 count, u16 fps, count x (u32 offset, u32 size), then the
baseline JPEG frames (320x240).
"""
import glob
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "montage.bin")
FPS, XFADE = 10, 3                         # 10 fps, 0.3 s crossfades
CX, CY = 320, 250                          # where each car's centre lands on the 640x480 canvas
GRADE = "eq=contrast=1.06:saturation=1.08:gamma=0.97"
# file, car centre (x, y) and width in a 640-wide copy, target width, frames, push-in per frame, slide
SHOTS = [("07-right-wide-turning.jpg", 325, 603, 610, 0.95, 18, 0.0015, 0),
         ("02-front-close.jpg", 282, 655, 556, 0.87, 14, 0.004, -1),
         ("04-left-closer.jpg", 291, 653, 570, 0.89, 16, 0.004, -1)]
DETAIL = ("06-headlight-detail.jpg", 290, 28, 0.003)       # plain crop at y, frames, push-in
# Optional end card: this logo (git-ignored with the photos) centred on white with a thin red frame.
# The last photo fades to white, the logo fades in, holds, then everything fades to black.
END_LOGO = "honda-logo.png"
END_LOGO_W, END_BORDER, END_RED = 296, 4, "0xE2001A"
END_IN, END_HOLD, END_OUT = 6, 20, 6                        # frames (the held frames share one JPEG)
END_Q = 2      # q 8 left grey specks around the black letters on white (photos hide them)


def ffmpeg(args):
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y"] + args, check=True)


def shot_filter(cx, cy, w, target, n, rate, slide, first):
    s = max(1.0, target * 640 / w)             # never shrink: no margins to fill
    fw, fx, fy = 640 * s, CX - cx * s, CY - cy * s
    fx = min(0.0, max(640 - fw, fx))           # the photo always covers the full width
    x = "(iw-iw/zoom)*(0.5+0.45*(%d)*(on/%d-0.5))" % (slide, n)
    fade = ",fade=t=in:st=0:d=0.8" if first else ""
    return ("scale=640:-1,scale=iw*%.4f:-1,crop=640:480:%d:%d,%s,"
            "zoompan=z='1.0+%s*on':x='%s':y='(ih-ih/zoom)/2':d=%d:s=320x240:fps=%d,vignette=PI/8%s,format=yuv420p"
            % (s, round(-fx), round(-fy), GRADE, rate, x, n, FPS, fade))


def end_card(photos, tmp):
    """JPEG frames of the end card, or [] without a logo. Encoded straight from stills, so the
    held frames come out byte-identical and pack() stores them once."""
    src = os.path.join(photos, END_LOGO)
    if not os.path.isfile(src):
        return []
    w, h = (int(v) for v in subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "stream=width,height", "-of", "csv=p=0", src],
        check=True, capture_output=True, text=True).stdout.split(",")[:2])
    flat = "color=white:s=%dx%d[w];[w][0]overlay=shortest=1" % (w, h)    # transparency on white
    gray = os.path.join(tmp, "logo.gray")
    ffmpeg(["-i", src, "-filter_complex", flat + ",format=gray", "-frames:v", "1", "-f", "rawvideo", gray])
    px = open(gray, "rb").read()
    rows = [y for y in range(h) if min(px[y * w:(y + 1) * w]) < 128]
    cols = [x for x in range(w) if min(px[x::w]) < 128]
    if not rows:
        return []
    x0, y0 = cols[0], rows[0]
    n = END_IN + END_HOLD + END_OUT
    ffmpeg(["-i", src, "-f", "lavfi", "-i", "color=white:s=320x240:r=%d" % FPS, "-filter_complex",
            flat + ",crop=%d:%d:%d:%d,scale=%d:-1:flags=area[l];[1][l]overlay=(W-w)/2:(H-h)/2-6,"
            "drawbox=x=0:y=0:w=320:h=240:color=%s:t=%d,"
            "fade=t=in:s=0:n=%d:color=white,fade=t=out:s=%d:n=%d,format=yuvj420p"
            % (cols[-1] - x0 + 1, rows[-1] - y0 + 1, x0, y0, END_LOGO_W, END_RED, END_BORDER,
               END_IN, END_IN + END_HOLD, END_OUT - 1),
            "-frames:v", str(n), "-q:v", str(END_Q), os.path.join(tmp, "e%03d.jpg")])
    return [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "e*.jpg")))]


def build(photos):
    tmp = tempfile.mkdtemp(prefix="montage_")
    try:
        clips = []
        for i, (name, cx, cy, w, target, n, rate, slide) in enumerate(SHOTS):
            src = os.path.join(photos, name)
            if not os.path.isfile(src):
                sys.exit("gen_montage: missing photo %s" % src)
            out = os.path.join(tmp, "c%d.mp4" % i)
            ffmpeg(["-i", src, "-vf", shot_filter(cx, cy, w, target, n, rate, slide, i == 0),
                    "-frames:v", str(n), "-c:v", "libx264", "-crf", "14", out])
            clips.append((out, n))
        name, y, n, rate = DETAIL
        src = os.path.join(photos, name)
        if not os.path.isfile(src):
            sys.exit("gen_montage: missing photo %s" % src)
        out = os.path.join(tmp, "c%d.mp4" % len(clips))
        ffmpeg(["-i", src, "-vf",
                "scale=640:-1,crop=640:480:0:%d,%s,zoompan=z='1.0+%s*on':x='(iw-iw/zoom)/2':y='(ih-ih/zoom)/2':"
                "d=%d:s=320x240:fps=%d,vignette=PI/8,fade=t=out:st=%.1f:d=0.6:color=%s,format=yuv420p"
                % (y, GRADE, rate, n, FPS, n / FPS - 0.6,
                   "white" if os.path.isfile(os.path.join(photos, END_LOGO)) else "black"), "-frames:v", str(n), "-c:v", "libx264", "-crf", "14", out])
        clips.append((out, n))
        inputs, graph, last, t = [], "", "[0]", clips[0][1] / FPS - XFADE / FPS
        for c, _ in clips:
            inputs += ["-i", c]
        for k in range(1, len(clips)):
            graph += "%s[%d]xfade=transition=fade:duration=%.1f:offset=%.2f[v%d];" % (last, k, XFADE / FPS, t, k)
            last = "[v%d]" % k
            t += clips[k][1] / FPS - XFADE / FPS
        joined = os.path.join(tmp, "montage.mp4")
        ffmpeg(inputs + ["-filter_complex", graph.rstrip(";"), "-map", last, "-c:v", "libx264", "-crf", "14", joined])
        ffmpeg(["-i", joined, "-q:v", "8", "-pix_fmt", "yuvj420p", os.path.join(tmp, "f%03d.jpg")])
        frames = [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "f*.jpg")))]
        return frames + end_card(photos, tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def pack(frames):
    head = b"MTG1" + struct.pack("<HH", len(frames), FPS)
    offset, index, data, seen = len(head) + 8 * len(frames), b"", [], {}
    for f in frames:
        if f not in seen:                      # repeated frames (the end card's hold) stored once
            seen[f] = offset
            data.append(f)
            offset += len(f)
        index += struct.pack("<II", seen[f], len(f))
    return head + index + b"".join(data)


if __name__ == "__main__":
    photos = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "montage-photos")
    blob = pack(build(photos))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    n = struct.unpack("<H", blob[4:6])[0]
    print("gen_montage: %s  %d frames, %.1f s, %d KB" % (os.path.relpath(OUT), n, n / FPS, len(blob) // 1024))
