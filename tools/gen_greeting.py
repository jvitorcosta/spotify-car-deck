"""Greeting clip for the boot sound (src/audio/greeting): raw signed 16-bit little-endian mono,
16 kHz. 16-bit so the firmware can lower the volume before reducing to the DAC's 8 bits.

  python tools/gen_greeting.py              rebuild the default chime -> data/greeting_default.pcm
  python tools/gen_greeting.py clip.wav     convert your own sound   -> data/greeting.pcm
  python tools/gen_greeting.py --finale clip.wav   boot-scene finale (ends with the montage) -> data/finale.pcm
  python tools/gen_greeting.py --opener clip.wav   montage opener (starts with the montage) -> data/opener.pcm

The default chime is synthesized here (original, safe to commit). Your own clip goes to
data/greeting.pcm, which is git-ignored and, when present, replaces the default on the next
build (tools/greeting_clip.py). Input: a PCM WAV of any rate/width/channels; convert other
formats first (e.g. ffmpeg -i in.mp3 out.wav). Standard library only.
"""
import math
import os
import struct
import sys
import wave

RATE = 16000
MAX_SECONDS = 13.0
PEAK = 0.95                # normalize the loudest sample to 95 % of full scale
SILENCE = 0.02             # trim leading/trailing samples quieter than 2 % of the peak

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DEFAULT_OUT = os.path.join(ROOT, "data", "greeting_default.pcm")
USER_OUT = os.path.join(ROOT, "data", "greeting.pcm")
FINALE_OUT = os.path.join(ROOT, "data", "finale.pcm")
OPENER_OUT = os.path.join(ROOT, "data", "opener.pcm")


def chime():
    """Two rising notes with a soft square tone and a decay: a short, original boot chime."""
    notes = [(784.0, 0.12), (1175.0, 0.30)]   # G5 then D6
    out = []
    for freq, dur in notes:
        n = int(RATE * dur)
        for i in range(n):
            t = i / RATE
            env = min(1.0, i / (RATE * 0.005)) * math.exp(-t * 7.0)
            tone = math.sin(2 * math.pi * freq * t) + 0.25 * math.sin(2 * math.pi * 3 * freq * t)
            out.append(0.55 * env * tone / 1.25)
    return out


def read_wav(path):
    """WAV -> mono float samples in [-1, 1] and the source rate."""
    with wave.open(path, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 1:
        vals = [(b - 128) / 128.0 for b in raw]
    elif width == 2:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    elif width == 3:
        vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0
                for i in range(0, len(raw), 3)]
    elif width == 4:
        vals = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(raw) // 4), raw)]
    else:
        sys.exit("gen_greeting: unsupported sample width %d bytes" % width)
    mono = [sum(vals[i:i + ch]) / ch for i in range(0, len(vals), ch)]
    return mono, rate


def resample(x, src):
    """Box-filter (cheap anti-alias) then linear interpolation to RATE."""
    if src == RATE:
        return x
    ratio = src / RATE
    if ratio > 1:
        k = int(math.ceil(ratio))
        acc, filt = 0.0, []
        for i, v in enumerate(x):
            acc += v
            if i >= k:
                acc -= x[i - k]
            filt.append(acc / min(i + 1, k))
        x = filt
    n = int(len(x) / ratio)
    out = []
    for j in range(n):
        p = j * ratio
        i = int(p)
        f = p - i
        b = x[i + 1] if i + 1 < len(x) else x[i]
        out.append(x[i] * (1 - f) + b * f)
    return out


def trim_normalize(x):
    peak = max((abs(v) for v in x), default=0.0)
    if peak == 0:
        sys.exit("gen_greeting: the clip is silent")
    loud = [i for i, v in enumerate(x) if abs(v) >= peak * SILENCE]
    x = x[loud[0]:loud[-1] + 1]
    return [v * PEAK / peak for v in x]


def write_pcm(x, path):
    data = struct.pack("<%dh" % len(x), *(max(-32768, min(32767, int(round(v * 32767)))) for v in x))
    with open(path, "wb") as f:
        f.write(data)
    print("gen_greeting: %s  %.2f s  %d bytes" % (os.path.relpath(path, ROOT), len(x) / RATE, len(data)))


def main():
    if len(sys.argv) == 1:
        write_pcm(chime(), DEFAULT_OUT)
        return
    out = FINALE_OUT if "--finale" in sys.argv else OPENER_OUT if "--opener" in sys.argv else USER_OUT
    args = [a for a in sys.argv[1:] if a not in ("--finale", "--opener")]
    x, rate = read_wav(args[0])
    x = trim_normalize(resample(x, rate))
    if len(x) > RATE * MAX_SECONDS:
        sys.exit("gen_greeting: clip is %.2f s after trimming; keep it under %.0f s"
                 % (len(x) / RATE, MAX_SECONDS))
    write_pcm(x, out)


if __name__ == "__main__":
    main()
