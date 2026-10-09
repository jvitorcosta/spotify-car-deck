# Boot Scene v3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reshape the pixel City from a real side profile, put it in a dark car park like the owner's photos with a detailed purple underglow, and end the boot scene with the owner's photo montage (07 → 02 → 04 → 06) instead of the photo-hero card.

**Architecture:** `util/bootanim` gains the montage frame index (pure); a pure `ui/montage` reader parses a git-ignored `data/montage.bin` built by `tools/gen_montage.py` from `montage-photos/` (a pre-build script writes an empty montage when it's missing). `ui/bootscene` is rewritten for the new background (static sky y 0–71, 7 animated strips), underglow detail, brake fade, then montage playback with TJpg_Decoder straight to the display. The v2 hero code is removed.

**Tech Stack:** ESP32 Arduino 2.0.17, TFT_eSPI 2.5.43, TJpg_Decoder 1.1.0 (already a dependency), Unity host tests, Python 3 + ffmpeg.

**Spec:** `docs/superpowers/specs/2026-10-09-boot-scene-v3-design.md` (on top of v1 and the kept parts of v2)

## Global Constraints

- Branch `feat/boot-scene` (HEAD `f95ca46`). v1 + v2 tasks 1–4 are committed; v2's hero ending is being removed here.
- No PSRAM; one 320 × 24 16-bit `TFT_eSprite` strip (15 360 B), freed at the end. Write strip pixels through `theme::be()`.
- App partition 3 MB (`huge_app`); firmware ≈ 2.0 MB after removing the hero + ≈ 0.76 MB montage.
- Montage photos: `montage-photos/` (git-ignored, files `07-right-wide-turning.jpg`, `02-front-close.jpg`, `04-left-closer.jpg`, `06-headlight-detail.jpg`); `data/montage.bin` git-ignored. The repo is public: never commit either.
- Host tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` (29 suites now). From bash set `MINGW_BIN` (see v2 plan) first.
- Firmware: `pio run -e esp32dev`; flash `--upload-port COM11`; `pio run -e hwcheck` must keep building.
- Commits: `type(scope): subject` + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **No montage file** (fresh clone / CI): the build must still work and the scene must end right after the brake fade. Pinned by `test_no_frames_finishes_at_once` (Task 1), `test_empty_montage` (Task 3), and Task 4 Step 4 (build with the fallback file).
2. **Corrupt or truncated montage** (offset past the end, wrong magic): no out-of-bounds read; treated as no montage. Pinned by `test_offset_past_end_is_rejected` / `test_bad_magic_is_rejected` / `test_truncated_header` (Task 3).
3. **Montage index past the end**: the scene stops instead of decoding garbage. Pinned by `test_montage_frame_index` (Task 1) and `montage::frame` bounds (Task 3).
4. **Photos missing when building the montage**: the tool names the missing file instead of producing a partial montage. Pinned by Task 3 Step 6 (run with an empty folder).
5. **Brake rendered over the new background with rows < 72**: sky gradient drawn into strips, no stale static sky. Pinned by the deck check in Task 4 Step 6.

---

### Task 1: Montage frame index (`util/bootanim`)

**Files:** Modify `src/util/bootanim.h`, `src/util/bootanim.cpp`; Test `test/test_bootanim/test_bootanim.cpp`

**Interfaces:** Produces `int bootanim::montageFrame(uint32_t u, int fps, int count)` — −1 while `u < BRAKE_MS`, then `(u − BRAKE_MS) × fps / 1000` capped at `count` (= finished).

- [ ] **Step 1: Failing tests** — add before `int main(` in `test/test_bootanim/test_bootanim.cpp`:
```cpp
// ---- v3: photo montage after the brake ----
void test_montage_frame_index() {
    TEST_ASSERT_EQUAL_INT(-1, montageFrame(599, 10, 67));          // still braking
    TEST_ASSERT_EQUAL_INT(0, montageFrame(600, 10, 67));
    TEST_ASSERT_EQUAL_INT(10, montageFrame(1600, 10, 67));
    TEST_ASSERT_EQUAL_INT(66, montageFrame(600 + 6699, 10, 67));
    TEST_ASSERT_EQUAL_INT(67, montageFrame(600 + 6700, 10, 67));   // finished
    TEST_ASSERT_EQUAL_INT(67, montageFrame(999999, 10, 67));
}
// Review Focus 1: no montage -> finished as soon as the brake is over.
void test_no_frames_finishes_at_once() {
    TEST_ASSERT_EQUAL_INT(-1, montageFrame(100, 10, 0));
    TEST_ASSERT_EQUAL_INT(0, montageFrame(600, 10, 0));
}
```
and in `main` before `return UNITY_END();`:
```cpp
    RUN_TEST(test_montage_frame_index);
    RUN_TEST(test_no_frames_finishes_at_once);
```
- [ ] **Step 2:** `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_bootanim\test_bootanim.cpp src\util\bootanim.cpp` → `COMPILE FAILED` (`montageFrame` not declared).
- [ ] **Step 3: Implement** — in `src/util/bootanim.h` before the final `}`:
```cpp

// ---- v3 montage: frame to show u ms after tHero (-1 while braking, count when finished) ----
int montageFrame(uint32_t u, int fps, int count);
```
in `src/util/bootanim.cpp` before the final `}`:
```cpp

int montageFrame(uint32_t u, int fps, int count) {
    if (u < BRAKE_MS) return -1;
    if (count <= 0 || fps <= 0) return count > 0 ? count : 0;
    const uint64_t i = (uint64_t)(u - BRAKE_MS) * (uint64_t)fps / 1000;
    return i >= (uint64_t)count ? count : (int)i;
}
```
- [ ] **Step 4:** same command → `22 Tests 0 Failures 0 Ignored`.
- [ ] **Step 5: Commit**
```bash
git add src/util/bootanim.h src/util/bootanim.cpp test/test_bootanim/test_bootanim.cpp
git commit -m "feat(util): montage frame index for the boot scene

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Reshaped pixel City (`tools/gen_car_sprite.py`)

**Files:** Modify `tools/gen_car_sprite.py`, `src/ui/car_sprite.inc` (regenerated), `test/test_car_sprite/test_car_sprite.cpp`

**Interfaces:** `car_sprite` API unchanged.

- [ ] **Step 1: Update the tests first.** In `test/test_car_sprite/test_car_sprite.cpp`:
  - replace the body of `test_trunk_lip_spoiler` with
```cpp
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(5, 16));      // spoiler on the higher trunk edge
    TEST_ASSERT_TRUE(car_sprite::pixel(5, 15, nullptr, nullptr));
    TEST_ASSERT_FALSE(car_sprite::pixel(5, 14, nullptr, nullptr));
```
  - replace the body of `test_silver_door_handles` with
```cpp
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(99, 26, &c));     // front door, near its rear edge
    TEST_ASSERT_EQUAL_HEX16(0xEF9E, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(99, 27, &c));
    TEST_ASSERT_EQUAL_HEX16(0x7C31, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::UPPER, kindAt(53, 26, &c));     // rear door
    TEST_ASSERT_EQUAL_HEX16(0xEF9E, c);
```
  - add before `int main(`:
```cpp
// v3 geometry from the side-profile reference: B-pillar at x 86-89, fuel door above the shoulder.
void test_side_profile_landmarks() {
    uint16_t c = 0;
    TEST_ASSERT_EQUAL_UINT8(car_sprite::OTHER, kindAt(87, 10, &c));     // black B-pillar
    TEST_ASSERT_EQUAL_HEX16(0x2987, c);
    TEST_ASSERT_EQUAL_UINT8(car_sprite::LOWER, kindAt(32, 21, &c));     // fuel door outline
    TEST_ASSERT_EQUAL_HEX16(0x7C31, c);
}
```
    and `RUN_TEST(test_side_profile_landmarks);` before `return UNITY_END();`.
- [ ] **Step 2:** `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_car_sprite\test_car_sprite.cpp src\ui\car_sprite.cpp` → failures in the spoiler, handle and landmark tests.
- [ ] **Step 3: Generator changes** (exact replacements in `tools/gen_car_sprite.py`):
  1. `"(4, 51), (1, 46), (0, 38), (0, 29), (1, 24), (3, 21), (22, 19.2), (26, 19), (40, 12), (56, 5),\n         (70, 2.2), (86, 1.2),"` → `"(4, 51), (1, 46), (0, 38), (0, 29), (1, 22), (3, 18), (20, 16.4), (24, 16), (40, 9.6), (56, 4.2),\n         (70, 1.9), (86, 1.0),"`
  2. The `GLASS = [...]` list → `GLASS = [(44, 15.0), (56, 6.6), (72, 3.6), (86, 3.0), (100, 3.0), (111, 3.8), (119, 6.0), (133, 15.2),\n         (139, 19.9)]`
  3. `shoulder_y` body → `return 26.6 - (x - 16) * 0.005`; `crease_y` body → `return 47 - (140 - x) * 0.085` (comment: steeper, per the side photo).
  4. Add before `def build():`
```python
def belt_y(x):           # window bottom / beltline: rises toward the rear (side reference photo)
    return 20.0 - (139 - x) * 0.053


```
     and in `paint` replace `if y < 21:` with `if y < belt_y(x):`.
  5. B-pillar loop → `for y in range(3, 21):\n        for x in range(86, 90):\n            if y < belt_y(x) + 1:\n                P(x, y, "trim")`
  6. Quarter-glass divider → `for y in range(5, 17):\n        P(round(56 - (y - 5) * 0.2), y, "trim")`
  7. Chrome belt → `for x in range(45, 139):\n        P(x, round(belt_y(x) + 0.6), "chrome")`
  8. Rear-glass reflection → `for k in range(7):\n        P(66 + k, 6 + k, "glassHi")`
  9. Door shut lines block → 
```python
    for y in range(21, 54):
        P(141, y, "deep", LOWER)
    for y in range(18, 54):
        P(88, y, "deep", LOWER)
    for y in range(16, 34):
        P(43, y, "deep", LOWER)
    for y in range(34, 40):
        P(round(43 + (y - 34) * 1.2), y, "deep", LOWER)
```
  10. Handles: `for hx in (82, 122):` → `for hx in (97, 51):`
  11. Fuel door: `for y in range(28, 34):\n        for x in range(36, 43):\n            if y in (28, 33) or x in (36, 42):` → `for y in range(19, 25):\n        for x in range(32, 39):\n            if y in (19, 24) or x in (32, 38):`
  12. Spoiler block → 
```python
    for x in range(2, 15):
        P(x, 16, "body", UPPER)
        P(x, 17, "shade", UPPER)
    for x in range(2, 9):
        P(x, 15, "hi", UPPER)
    P(1, 16, "deep", UPPER)
```
  13. Docstring: add the line `Side profile measured from "2024 Honda City RS - Sideview" (Wikimedia Commons, CC BY-SA 4.0).`
- [ ] **Step 4:** `python tools/gen_car_sprite.py` → `200x66, 8525 opaque px`; `--png car_preview.png`, view (rising window line, high trunk, B-pillar further back), delete the PNG.
- [ ] **Step 5:** test command → `10 Tests 0 Failures 0 Ignored`.
- [ ] **Step 6: Commit**
```bash
git add tools/gen_car_sprite.py src/ui/car_sprite.inc test/test_car_sprite/test_car_sprite.cpp
git commit -m "feat(ui): reshape the pixel City from a real side profile

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Montage asset and reader

**Files:** Create `tools/gen_montage.py`, `tools/montage_clip.py`, `src/ui/montage.h`, `src/ui/montage.cpp`, `test/test_montage/test_montage.cpp`; Modify `platformio.ini`, `.gitignore`

**Interfaces:** Produces (namespace `montage`): `bool parse(const uint8_t* blob, size_t len)`; `int count()`; `int fps()`; `bool frame(int i, const uint8_t** jpg, size_t* len)`.

- [ ] **Step 1: Failing test** — `test/test_montage/test_montage.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/ui/montage.h"

void setUp() {}
void tearDown() {}

static void u16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static void u32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }

// "MTG1", count, fps, index, payloads
static std::vector<uint8_t> blob(const std::vector<std::vector<uint8_t>>& frames, uint16_t fps = 10) {
    std::vector<uint8_t> b = {'M', 'T', 'G', '1'};
    u16(b, (uint16_t)frames.size());
    u16(b, fps);
    uint32_t off = 8 + 8 * (uint32_t)frames.size();
    for (auto& f : frames) { u32(b, off); u32(b, (uint32_t)f.size()); off += (uint32_t)f.size(); }
    for (auto& f : frames) b.insert(b.end(), f.begin(), f.end());
    return b;
}

void test_parses_two_frames() {
    auto b = blob({{1, 2, 3}, {9, 8}});
    TEST_ASSERT_TRUE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(2, montage::count());
    TEST_ASSERT_EQUAL_INT(10, montage::fps());
    const uint8_t* j = nullptr;
    size_t n = 0;
    TEST_ASSERT_TRUE(montage::frame(1, &j, &n));
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)n);
    TEST_ASSERT_EQUAL_UINT8(9, j[0]);
    TEST_ASSERT_EQUAL_UINT8(8, j[1]);
}
void test_out_of_range_index() {
    auto b = blob({{1, 2, 3}});
    TEST_ASSERT_TRUE(montage::parse(b.data(), b.size()));
    const uint8_t* j;
    size_t n;
    TEST_ASSERT_FALSE(montage::frame(-1, &j, &n));
    TEST_ASSERT_FALSE(montage::frame(1, &j, &n));
}
// Review Focus 1: the build fallback writes an empty montage.
void test_empty_montage() {
    auto b = blob({});
    TEST_ASSERT_TRUE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
// Review Focus 2: corrupt files count as no montage.
void test_bad_magic_is_rejected() {
    auto b = blob({{1}});
    b[0] = 'X';
    TEST_ASSERT_FALSE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
void test_offset_past_end_is_rejected() {
    auto b = blob({{1, 2, 3}});
    b.pop_back();                                        // payload one byte short
    TEST_ASSERT_FALSE(montage::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
void test_truncated_header() {
    auto b = blob({{1}, {2}});
    TEST_ASSERT_FALSE(montage::parse(b.data(), 12));    // index cut off
    TEST_ASSERT_FALSE(montage::parse(b.data(), 3));
    TEST_ASSERT_FALSE(montage::parse(nullptr, 0));
    TEST_ASSERT_EQUAL_INT(0, montage::count());
}
void test_zero_fps_is_rejected() {
    auto b = blob({{1}}, 0);
    TEST_ASSERT_FALSE(montage::parse(b.data(), b.size()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_two_frames);
    RUN_TEST(test_out_of_range_index);
    RUN_TEST(test_empty_montage);
    RUN_TEST(test_bad_magic_is_rejected);
    RUN_TEST(test_offset_past_end_is_rejected);
    RUN_TEST(test_truncated_header);
    RUN_TEST(test_zero_fps_is_rejected);
    return UNITY_END();
}
```
- [ ] **Step 2:** `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_montage\test_montage.cpp` → `COMPILE FAILED`.
- [ ] **Step 3: Reader** — `src/ui/montage.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
// The boot scene's photo montage (data/montage.bin, built by tools/gen_montage.py): "MTG1",
// u16 count, u16 fps, count x (u32 offset, u32 size), JPEG frames. PURE, host-tested.
// A missing, empty or corrupt file reads as 0 frames.
namespace montage {
bool parse(const uint8_t* blob, size_t len);   // false (and count 0) when the blob is invalid
int count();
int fps();
bool frame(int i, const uint8_t** jpg, size_t* len);
}
```
`src/ui/montage.cpp`:
```cpp
#include "montage.h"
#include <cstring>

namespace montage {

static const uint8_t* s_blob = nullptr;
static int s_count = 0, s_fps = 0;

static uint32_t rd16(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool parse(const uint8_t* blob, size_t len) {
    s_blob = nullptr;
    s_count = s_fps = 0;
    if (!blob || len < 8 || memcmp(blob, "MTG1", 4) != 0) return false;
    const uint32_t n = rd16(blob + 4), fps = rd16(blob + 6);
    const uint64_t indexEnd = 8 + 8 * (uint64_t)n;
    if (fps == 0 || indexEnd > len) return false;
    for (uint32_t i = 0; i < n; ++i) {
        const uint64_t off = rd32(blob + 8 + 8 * i), size = rd32(blob + 12 + 8 * i);
        if (off < indexEnd || off + size > len) return false;
    }
    s_blob = blob;
    s_count = (int)n;
    s_fps = (int)fps;
    return true;
}

int count() { return s_count; }
int fps() { return s_fps; }

bool frame(int i, const uint8_t** jpg, size_t* len) {
    if (i < 0 || i >= s_count) return false;
    *jpg = s_blob + rd32(s_blob + 8 + 8 * i);
    *len = rd32(s_blob + 12 + 8 * i);
    return true;
}

}
```
- [ ] **Step 4:** `...ntest.ps1 test\test_montage\test_montage.cpp src\ui\montage.cpp` → `7 Tests 0 Failures 0 Ignored`.
- [ ] **Step 5: Builder and build fallback.**
  `tools/gen_montage.py` — exactly the prototyped script (it reproduced the approved montage: 67 frames, 6.7 s, ≈ 761 KB), with `OUT = os.path.join(ROOT, "data", "montage.bin")` (drop the `MONTAGE_OUT` override):
```python
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
                "d=%d:s=320x240:fps=%d,vignette=PI/8,fade=t=out:st=%.1f:d=0.6,format=yuv420p"
                % (y, GRADE, rate, n, FPS, n / FPS - 0.6), "-frames:v", str(n), "-c:v", "libx264", "-crf", "14", out])
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
        ffmpeg(["-i", joined, "-q:v", "6", "-pix_fmt", "yuvj420p", os.path.join(tmp, "f%03d.jpg")])
        return [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "f*.jpg")))]
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def pack(frames):
    head = b"MTG1" + struct.pack("<HH", len(frames), FPS)
    offset, index = len(head) + 8 * len(frames), b""
    for f in frames:
        index += struct.pack("<II", offset, len(f))
        offset += len(f)
    return head + index + b"".join(frames)


if __name__ == "__main__":
    photos = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "montage-photos")
    blob = pack(build(photos))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    n = struct.unpack("<H", blob[4:6])[0]
    print("gen_montage: %s  %d frames, %.1f s, %d KB" % (os.path.relpath(OUT), n, n / FPS, len(blob) // 1024))
```
  `tools/montage_clip.py`:
```python
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
```
  `platformio.ini`: under `board_build.embed_files` add `    data/montage.bin` after `    data/greeting.pcm`; under `extra_scripts` add
```ini
    ; boot scene photo montage: data/montage.bin from tools/gen_montage.py (git-ignored), else empty
    pre:tools/montage_clip.py
```
  `.gitignore`: append `data/montage.bin` under the montage-photos lines.
- [ ] **Step 6: Build the montage; check the missing-photo error (Review Focus 4).**
  `python tools/gen_montage.py` → `gen_montage: data\montage.bin  67 frames, 6.7 s, 761 KB` (±5 KB).
  `python tools/gen_montage.py <an empty temp folder>` → exits with `gen_montage: missing photo ...07-right-wide-turning.jpg`.
  `git status` must not list `data/montage.bin` or `montage-photos/`.
- [ ] **Step 7: Commit**
```bash
git add tools/gen_montage.py tools/montage_clip.py src/ui/montage.h src/ui/montage.cpp test/test_montage platformio.ini .gitignore
git commit -m "feat(ui): photo montage asset (local, git-ignored) and reader

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Scene v3 and removal of the hero ending

**Files:** Replace `src/ui/bootscene.cpp`; Delete `tools/gen_hero_sprite.py`, `src/ui/hero_sprite.h`, `src/ui/hero_sprite.cpp`, `src/ui/hero_sprite.inc`, `test/test_hero_sprite/`; Modify `src/util/bootanim.h/.cpp`, `test/test_bootanim/test_bootanim.cpp`

**Interfaces:** Consumes `bootanim::{brakeX, brakeTime, brakeLevel, BRAKE_MS, montageFrame, ...v1 functions}`, `car_sprite::*`, `montage::*`, `greeting::{durationMs, level}`. Produces unchanged `bootscene` API.

- [ ] **Step 1: Remove the hero-only maths and tests.** In `test/test_bootanim/test_bootanim.cpp` delete the functions `test_hero_beats`, `test_hero_done_at`, `test_hero_brightness`, `test_headlight_double_flash`, `test_name_fades_in` and their `RUN_TEST` lines (keep the three brake tests). In `src/util/bootanim.h` replace the v2 ending block (from the `// ---- v2 ending` comment to `uint32_t heroDoneAt(uint32_t tHero);`) with:
```cpp
// ---- ending: brake (v2), then the photo montage (v3); u = ms since tHero ----
constexpr uint32_t BRAKE_MS = 600;
int brakeX(int xc, uint32_t u);                  // rolls 40 px from xc and stops (ease-out)
uint32_t brakeTime(uint32_t tHero, uint32_t u);  // braking clock for scroll and spokes
uint8_t brakeLevel(uint32_t u);                  // whole-frame brightness 255 -> 0
```
  In `src/util/bootanim.cpp` delete `BEAT_START`, `heroBeat`, `heroProgress`, `heroLevel`, `lampsOn`, `nameLevel`, `heroDoneAt` (keep `brakeX`, `brakeTime`, `brakeLevel`, `montageFrame`).
  Delete the hero files: `git rm tools/gen_hero_sprite.py src/ui/hero_sprite.h src/ui/hero_sprite.cpp src/ui/hero_sprite.inc test/test_hero_sprite/test_hero_sprite.cpp`.
  Run: `...ntest.ps1 test\test_bootanim\test_bootanim.cpp src\util\bootanim.cpp` → `17 Tests 0 Failures 0 Ignored`.

- [ ] **Step 2: Replace `src/ui/bootscene.cpp`:**
```cpp
#include "bootscene.h"
#include <Arduino.h>
#include <TJpg_Decoder.h>
#include <cmath>
#include <cstdlib>
#include "car_sprite.h"
#include "montage.h"
#include "theme.h"
#include "../audio/greeting.h"
#include "../core/mem.h"
#include "../util/bootanim.h"

extern const uint8_t _binary_data_montage_bin_start[] asm("_binary_data_montage_bin_start");
extern const uint8_t _binary_data_montage_bin_end[] asm("_binary_data_montage_bin_end");

namespace bootscene {
namespace {

using bootanim::hex565;

constexpr int W = 320, H = 240;
constexpr int SKY_H = 72;                        // static sky while cruising, drawn once
constexpr int STRIP_H = 24;
constexpr int STRIPS = (H - SKY_H) / STRIP_H;    // cruising: y 72-239 (7 strips)
constexpr int ALL_STRIPS = H / STRIP_H;          // brake fade: whole screen (10 strips)
static_assert(SKY_H % STRIP_H == 0 && H % STRIP_H == 0, "strips must tile the screen");
constexpr int GROUND_Y = 214;                    // the car's ground line
constexpr int CAR_TOP = GROUND_Y - 65;           // sprite row 0 (without the bob)
constexpr uint32_t FRAME_MS = 40;                // 25 fps target while drawing the pixel scene
constexpr int ROAD_SPEED = 90, FENCE_SPEED = 30;
constexpr int LAMP_PERIOD = 170, DASH_PERIOD = 48, BAY_PERIOD = 60, CARS_PERIOD = 450;
constexpr float TAU = 6.28318530718f;            // Arduino.h #defines TWO_PI
constexpr uint16_t UNDERGLOW = hex565(0x8C46FF), GLOW_CORE = hex565(0xBE8CFF);
constexpr uint16_t WHITE = 0xFFFF;

TFT_eSPI* s_tft = nullptr;
bool (*s_online)() = nullptr;
volatile bool s_active = false;
const char* volatile s_caption = "";
volatile uint32_t s_captionGen = 0;

uint16_t* s_buf = nullptr;   // the strip being rendered (byte-swapped, as TFT_eSprite stores it)
int s_y0 = 0;                // screen y of the strip's first row
uint8_t s_treeTop[W];        // top y of the tree-line canopy per column

// ---- strip drawing (everything clipped to the current strip) ----
inline void put(int x, int y, uint16_t c) { s_buf[(y - s_y0) * W + x] = theme::be(c); }
inline uint16_t get(int x, int y) { return theme::be(s_buf[(y - s_y0) * W + x]); }

void rect(int x, int y, int w, int h, uint16_t c) {
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h;
    if (!bootanim::clip(x0, x1, 0, W) || !bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) return;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) put(xx, yy, c);
}

void blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a) {
    if (!a) return;
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h;
    if (!bootanim::clip(x0, x1, 0, W) || !bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) return;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) put(xx, yy, bootanim::blend565(c, get(xx, yy), a));
}

inline void blendPx(int x, int y, uint16_t c, uint8_t a) {
    if (a && x >= 0 && x < W && y >= s_y0 && y < s_y0 + STRIP_H) put(x, y, bootanim::blend565(c, get(x, y), a));
}

void dimStrip(uint8_t level) {
    if (level == 255) return;
    for (int i = 0; i < W * STRIP_H; ++i) s_buf[i] = theme::be(bootanim::scale565(theme::be(s_buf[i]), level));
}

// Linear blend between two colours over a range of rows.
uint16_t lerp565(uint16_t a, uint16_t b, int num, int den) {
    return bootanim::blend565(b, a, (uint8_t)(255 * num / den));
}

// ---- sky ----
uint16_t skyAt(int y) {                          // #05070C at the top -> #0B1222 at 71 -> #0E1628 at 129
    if (y < SKY_H) return lerp565(hex565(0x05070C), hex565(0x0B1222), y, SKY_H - 1);
    return lerp565(hex565(0x0B1222), hex565(0x0E1628), y - SKY_H, 129 - SKY_H);
}
struct Star { int x, y; };
Star star(int i) { return {(i * 73) % W, (i * 37) % (SKY_H - 6)}; }
constexpr int STARS = 18;
constexpr uint16_t STAR = hex565(0x96A0BE);
bool starSkipped(const Star& s) { return s.y < 24 && s.x < 220; }   // under the caption

// Cruising: sky straight on the display, once (and again when the caption changes).
void drawSky(const char* caption) {
    for (int y = 0; y < SKY_H; ++y) s_tft->drawFastHLine(0, y, W, skyAt(y));
    for (int i = 0; i < STARS; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, STAR);
    }
    s_tft->setTextColor(hex565(0xCFD8FF));       // one argument: transparent background
    s_tft->drawString(caption, 6, 6, 2);
}

void twinkle(uint32_t t) {
    for (int i = 0; i < STARS; ++i) {
        const Star s = star(i);
        if (!starSkipped(s)) s_tft->drawPixel(s.x, s.y, bootanim::starHidden(t, i) ? skyAt(s.y) : STAR);
    }
}

// Brake fade: the sky rendered into the strip (no caption) so it dims with the rest.
void drawSkyIntoStrip(uint32_t t) {
    for (int y = 0; y < SKY_H; ++y) rect(0, y, W, 1, skyAt(y));
    for (int i = 0; i < STARS; ++i) {
        const Star s = star(i);
        if (!starSkipped(s) && !bootanim::starHidden(t, i)) rect(s.x, s.y, 1, 1, STAR);
    }
}

// Tree line: 26 overlapping round canopies, tops y 86-110, deterministic; computed once.
void initTrees() {
    for (int x = 0; x < W; ++x) s_treeTop[x] = 126;
    for (int i = 0; i < 26; ++i) {
        const int cx = i * 13 + (i * 37) % 9 - 4, r = 10 + (i * 7) % 10, top = 86 + (i * 29) % 25;
        const int cy = top + r;
        const int hw = (int)(r * 1.2f);
        for (int x = cx - hw; x <= cx + hw; ++x) {
            if (x < 0 || x >= W) continue;
            const float u = (float)(x - cx) / 1.2f;
            const float d2 = (float)(r * r) - u * u;
            if (d2 < 0) continue;
            const int yt = (int)(cy - sqrtf(d2));
            if (yt < s_treeTop[x]) s_treeTop[x] = (uint8_t)yt;
        }
    }
}

// ---- the car park ----
void drawBackground(uint32_t t) {
    for (int y = SKY_H; y < 130; ++y) rect(0, y, W, 1, skyAt(y));         // sky continued
    const uint16_t tree = hex565(0x0A120E);                                // tree line (static)
    for (int y = s_y0 < 86 ? 86 : s_y0; y < s_y0 + STRIP_H && y < 126; ++y)
        for (int x = 0; x < W; ++x)
            if (y >= s_treeTop[x]) put(x, y, tree);
    rect(0, 126, W, 10, hex565(0x09100C));
    const int fo = bootanim::scroll(t, FENCE_SPEED, 1000);                // chain-link fence
    rect(0, 123, W, 1, hex565(0x32643F));
    for (int y = s_y0 < 124 ? 124 : s_y0; y < s_y0 + STRIP_H && y < 140; ++y)
        for (int x = 0; x < W; ++x)
            if ((x + fo + y) % 5 == 0 || (x + fo - y + 1000) % 5 == 0) put(x, y, bootanim::blend565(hex565(0x285A37), get(x, y), 90));
    const int co = bootanim::scroll(t, FENCE_SPEED, CARS_PERIOD);         // distant parked cars
    for (int k = 0; k < 3; ++k) {
        const int px = ((k * 150 + 60 - co) % CARS_PERIOD + CARS_PERIOD) % CARS_PERIOD - 40;
        rect(px, 132, 34, 8, hex565(0x1E2128));
        rect(px + 6, 127, 20, 6, hex565(0x1A1D24));
        rect(px + 1, 135, 1, 1, hex565(0xC81E1E));
        rect(px + 32, 135, 1, 1, hex565(0xC81E1E));
    }
    for (int y = s_y0 < 140 ? 140 : s_y0; y < s_y0 + STRIP_H; ++y) {      // near-black asphalt
        const uint16_t base = lerp565(hex565(0x0E0F13), hex565(0x12131A), y - 140, 99);
        const uint16_t grain[4] = {bootanim::scale565(base, 225), base,
                                   bootanim::blend565(WHITE, base, 5), bootanim::blend565(WHITE, base, 10)};
        for (int x = 0; x < W; ++x) put(x, y, grain[((uint32_t)(x * 73856093u) ^ (uint32_t)(y * 19349663u)) >> 13 & 3]);
    }
    const int bo = bootanim::scroll(t, ROAD_SPEED, BAY_PERIOD);           // parking-bay lines
    for (int bx = -BAY_PERIOD; bx < W + BAY_PERIOD; bx += BAY_PERIOD)
        for (int y = 150; y < 168; ++y) blendRect(bx - bo + (y - 150) / 3, y, 2, 1, hex565(0x46484E), 140);
    const int d = bootanim::scroll(t, ROAD_SPEED, DASH_PERIOD);           // lane dashes
    for (int x = -DASH_PERIOD; x < W; x += DASH_PERIOD) rect(x - d, 226, 22, 2, hex565(0x3C3E42));
    const int lo = bootanim::scroll(t, ROAD_SPEED, LAMP_PERIOD);          // white LED street lamps
    for (int lx = 40 - lo; lx < W + LAMP_PERIOD; lx += LAMP_PERIOD) {
        rect(lx, 74, 2, 76, hex565(0x242830));
        rect(lx, 72, 14, 2, hex565(0x242830));
        rect(lx + 10, 74, 6, 2, hex565(0xF5FAFF));
        for (int y = 76; y < 154; ++y) {
            const int hw = (y - 76) * 45 / 100;
            blendRect(lx + 13 - hw, y, 2 * hw + 1, 1, hex565(0xE1EBFF), 13);
        }
        for (int y = 150; y < 170; ++y)
            for (int x = lx - 30; x < lx + 56; ++x) {
                const float dx = (float)(x - lx - 13) / 45.0f, dy = (float)(y - 160) / 9.0f, dd = dx * dx + dy * dy;
                if (dd < 1.0f) blendPx(x, y, hex565(0xC8D2E6), (uint8_t)(26.0f * (1.0f - dd)));
            }
    }
}

void wheel(int cx, int cy, float base, float glow) {
    for (int dy = -14; dy <= 14; ++dy) {
        const int y = cy + dy;
        if (y < s_y0 || y >= s_y0 + STRIP_H) continue;
        for (int dx = -14; dx <= 14; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= W) continue;
            const float d = sqrtf((float)(dx * dx + dy * dy));
            if (d > 13.5f) continue;
            uint16_t c;
            if (d > 10.5f) c = hex565(0x141416);             // tyre
            else if (d > 9.5f) c = hex565(0xC9CED4);         // rim lip
            else if (d <= 1.0f) c = hex565(0x3A3E46);        // hub centre
            else if (d <= 2.3f) c = hex565(0xC8CCD2);        // hub
            else {
                c = hex565(0x2B2F36);                        // dark rim face
                if (d >= 2.5f) {                             // 5 twin spokes, two-tone
                    const float a = atan2f((float)dy, (float)dx) - base;
                    float best = 9.0f;
                    for (int k = 0; k < 5; ++k)
                        for (float off : {-0.17f, 0.17f})
                            best = fminf(best, fabsf(remainderf(a - (k * TAU / 5 + off), TAU)));
                    if (best < 0.10f) c = hex565(0xDFE3E8);
                    else if (best < 0.18f) c = hex565(0x6A7078);
                }
            }
            if (dy > 4) c = bootanim::blend565(UNDERGLOW, c, (uint8_t)(fminf(128.0f, (dy - 4) * 13.0f) * glow));
            put(x, y, c);
        }
    }
}

struct Car { uint32_t t; int x; int top; uint8_t e; bool lamp; int lampX; float spoke; };

bool nearestLamp(uint32_t t, int carX, int* lampX) {
    const int lo = bootanim::scroll(t, ROAD_SPEED, LAMP_PERIOD);
    bool found = false;
    for (int lx = 40 - lo; lx < W + LAMP_PERIOD; lx += LAMP_PERIOD) {
        const int hx = lx + 13 - carX;
        if (hx <= -40 || hx >= 240) continue;
        if (!found || abs(hx - 100) < abs(*lampX - 100)) *lampX = hx;
        found = true;
    }
    return found;
}

Car makeCar(uint32_t world, int x, int bob, uint8_t e) {
    Car c{};
    c.t = world;
    c.x = x;
    c.top = CAR_TOP + bob;
    c.e = e;
    c.spoke = bootanim::spokeAngle(world);
    c.lamp = nearestLamp(world, x, &c.lampX);
    return c;
}

void drawCar(const Car& c) {
    const float glow = bootanim::glowAlpha(c.e) / 217.0f;            // 0.53 at rest .. 1.0 loud
    rect(c.x + 24, GROUND_Y, 154, 2, hex565(0x08090C));              // shadow
    const int cx = c.x + 100;                                         // underglow halo + core on the asphalt
    for (int y = 199; y < 228; ++y) {
        if (y < s_y0 || y >= s_y0 + STRIP_H) continue;
        for (int x = c.x - 26; x < c.x + 226; ++x) {
            const float hx = (float)(x - cx) / 126.0f, hy = (float)(y - 213) / 14.0f, d = hx * hx + hy * hy;
            if (d < 1.0f) blendPx(x, y, UNDERGLOW, (uint8_t)(140.0f * (1.0f - d) * sqrtf(1.0f - d) * glow));
            const float kx = (float)(x - cx) / 90.0f, ky = (float)(y - 211) / 5.0f, k = kx * kx + ky * ky;
            if (k < 1.0f) blendPx(x, y, GLOW_CORE, (uint8_t)(115.0f * (1.0f - k) * glow));
        }
    }
    blendRect(c.x + 30, 205, 142, 1, GLOW_CORE, (uint8_t)(230 * glow));   // LED tube under the sills
    blendRect(c.x + 30, 206, 142, 1, UNDERGLOW, (uint8_t)(205 * glow));
    const uint8_t ba = bootanim::beamAlpha(c.e);                      // headlight beam, cool white
    for (int i = 0; i < 90; ++i) {
        const int h = 3 + i * 35 / 100;
        blendRect(c.x + 200 + i, c.top + 31 - h / 3, 1, h, hex565(0xF0F5FF), (uint8_t)(ba * (90 - i) / 90));
    }
    blendRect(c.x - 4, c.top + 21, 6, 9, hex565(0xFF3030), bootanim::tailGlowAlpha(c.e));

    int x0 = c.x, x1 = c.x + car_sprite::width(), y0 = c.top, y1 = c.top + car_sprite::height();
    if (bootanim::clip(x0, x1, 0, W) && bootanim::clip(y0, y1, s_y0, s_y0 + STRIP_H)) {
        const uint16_t head = bootanim::headColor(c.e), refl = hex565(0xE6EEFF);
        const uint8_t tail = bootanim::tailLevel(c.e);
        for (int y = y0; y < y1; ++y) {
            const int row = y - c.top;
            const uint8_t purple = row > 44 ? (uint8_t)(fminf(115.0f, (row - 44) * 8.0f) * glow) : 0;
            for (int x = x0; x < x1; ++x) {
                uint16_t col;
                uint8_t kind;
                if (!car_sprite::pixel(x - c.x, row, &col, &kind)) continue;
                if (kind == car_sprite::HEAD) col = head;
                else if (kind == car_sprite::TAIL) col = bootanim::scale565(col, tail);
                else if (c.lamp && (kind == car_sprite::UPPER || kind == car_sprite::LOWER)) {
                    const uint8_t a = bootanim::reflectAlpha((x - c.x) - c.lampX, kind == car_sprite::UPPER);
                    if (a) col = bootanim::blend565(refl, col, a);
                }
                if (purple) col = bootanim::blend565(UNDERGLOW, col, purple);   // underglow on the lower body
                put(x, y, col);
            }
        }
    }
    const int wy = CAR_TOP + car_sprite::WHEEL_Y;                      // wheels stay on the road (no bob)
    wheel(c.x + car_sprite::REAR_WHEEL_X, wy, c.spoke, glow);
    wheel(c.x + car_sprite::FRONT_WHEEL_X, wy, c.spoke, glow);
}

// ---- montage ----
bool jpgOut(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
    s_tft->pushImage(x, y, w, h, bmp);
    return true;
}

// No strip buffer: caption on black, same exit rule, no animation.
void runWithoutScene(uint32_t t0) {
    uint32_t gen = s_captionGen - 1;
    for (;;) {
        if (gen != s_captionGen) {
            gen = s_captionGen;
            s_tft->fillScreen(TFT_BLACK);
            s_tft->setTextColor(TFT_WHITE, TFT_BLACK);
            s_tft->drawString(s_caption, 10, 10, 2);
        }
        const uint32_t t = millis() - t0;
        if (bootanim::mayExit(t, t >= greeting::durationMs(), s_online())) return;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void task(void*) {
    mem::log("bootscene start");
    const uint32_t t0 = millis();
    montage::parse(_binary_data_montage_bin_start,
                   (size_t)(_binary_data_montage_bin_end - _binary_data_montage_bin_start));
    initTrees();
    TFT_eSprite spr(s_tft);
    spr.setColorDepth(16);
    s_buf = (uint16_t*)spr.createSprite(W, STRIP_H);
    if (!s_buf) {
        Serial.println("[bootscene] no memory for the strip buffer: scene skipped");
        runWithoutScene(t0);
    } else {
        uint32_t gen = s_captionGen;
        drawSky(s_caption);
        uint32_t tHero = bootanim::NO_EXIT;
        int xc = 0, shown = -1;
        uint32_t frames[3] = {0, 0, 0}, busyMs[3] = {0, 0, 0};   // drive, brake, montage
        TickType_t wake = xTaskGetTickCount();
        for (;;) {
            const uint32_t t = millis() - t0;
            if (tHero == bootanim::NO_EXIT && bootanim::mayExit(t, t >= greeting::durationMs(), s_online())) {
                tHero = t;
                xc = bootanim::cruiseX(t);
            }
            const uint32_t u = tHero == bootanim::NO_EXIT ? 0 : t - tHero;
            const uint32_t f0 = millis();
            const uint8_t e = greeting::level(t);
            int mode;
            if (tHero == bootanim::NO_EXIT) {                    // enter + cruise
                mode = 0;
                if (gen != s_captionGen) {
                    gen = s_captionGen;
                    drawSky(s_caption);
                }
                twinkle(t);
                const Car car = makeCar(t, bootanim::carX(t, bootanim::NO_EXIT), bootanim::bob(t), e);
                for (int s = 0; s < STRIPS; ++s) {
                    s_y0 = SKY_H + s * STRIP_H;
                    drawBackground(t);
                    drawCar(car);
                    spr.pushSprite(0, s_y0);
                }
            } else if (u < bootanim::BRAKE_MS) {                 // brake + fade the whole screen
                mode = 1;
                const uint32_t world = bootanim::brakeTime(tHero, u);
                const Car car = makeCar(world, bootanim::brakeX(xc, u), 0, e);
                const uint8_t lvl = bootanim::brakeLevel(u);
                for (int s = 0; s < ALL_STRIPS; ++s) {
                    s_y0 = s * STRIP_H;
                    if (s_y0 < SKY_H) drawSkyIntoStrip(world);
                    drawBackground(world);
                    drawCar(car);
                    dimStrip(lvl);
                    spr.pushSprite(0, s_y0);
                }
            } else {                                             // the owner's photo montage
                mode = 2;
                const int i = bootanim::montageFrame(u, montage::fps(), montage::count());
                if (i >= montage::count()) break;                // finished (or no montage)
                if (i != shown) {
                    if (shown < 0) {
                        spr.deleteSprite();                      // the strip isn't needed any more
                        s_buf = nullptr;
                        TJpgDec.setJpgScale(1);
                        TJpgDec.setSwapBytes(true);
                        TJpgDec.setCallback(jpgOut);
                    }
                    const uint8_t* jpg;
                    size_t len;
                    if (montage::frame(i, &jpg, &len)) TJpgDec.drawJpg(0, 0, jpg, len);
                    shown = i;
                }
            }
            busyMs[mode] += millis() - f0;
            ++frames[mode];
            vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS));
        }
        if (s_buf) spr.deleteSprite();
        s_buf = nullptr;
        Serial.printf("[bootscene] %u ms; render avg drive %u ms (%u frames), brake %u ms (%u), montage %u ms (%u of %d); stack free %u\n",
                      (unsigned)(millis() - t0),
                      (unsigned)(frames[0] ? busyMs[0] / frames[0] : 0), (unsigned)frames[0],
                      (unsigned)(frames[1] ? busyMs[1] / frames[1] : 0), (unsigned)frames[1],
                      (unsigned)(frames[2] ? busyMs[2] / frames[2] : 0), (unsigned)frames[2], montage::count(),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
    mem::log("bootscene end");
    s_active = false;
    vTaskDelete(nullptr);
}

}  // namespace

void start(TFT_eSPI& tft, bool (*online)()) {
    s_tft = &tft;
    s_online = online;
    s_active = true;
    xTaskCreatePinnedToCore(task, "bootscene", 4096, nullptr, 1, nullptr, 1);
}

void setCaption(const char* text) {
    s_caption = text;
    s_captionGen = s_captionGen + 1;
}

bool active() { return s_active; }

void waitDone() {
    while (s_active) delay(20);
}

}
```
- [ ] **Step 3:** `pio run -e esp32dev` → `SUCCESS` (no warnings from `bootscene.cpp`, `montage.cpp`, `bootanim.cpp`); `pio run -e hwcheck` → `SUCCESS`; `run_tests.ps1` → `suites pass=29 fail=0` (hero suite gone, montage suite added).
- [ ] **Step 4: Build without the montage (Review Focus 1).** Move `data/montage.bin` aside (`mv data/montage.bin data/montage.bin.keep`), `pio run -e esp32dev` → the pre-build prints `montage_clip: no data/montage.bin ...` and the build succeeds; delete the generated empty file and move the real one back (`mv data/montage.bin.keep data/montage.bin`).
- [ ] **Step 5: Commit**
```bash
git add -A src/ui/bootscene.cpp src/util/bootanim.h src/util/bootanim.cpp test/test_bootanim/test_bootanim.cpp
git rm -r --cached --ignore-unmatch test/test_hero_sprite
git commit -m "feat(ui): boot scene v3 - car park, reshaped City, underglow detail, photo montage ending

Removes the v2 photo-hero ending (cut-out, sweep, flash, name card).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(`git status` afterwards: clean apart from untracked git-ignored files.)
- [ ] **Step 6: On the deck, owner watching (Review Focus 5).** Flash, reset, capture serial 35 s. Expected: `[bootscene] ... drive D ms, brake B ms (...), montage M ms (67 of 67)`, `[mem] bootscene end` before Spotify. Ask the owner to confirm: dark car park with white lamps, trees, fence, parked cars; reshaped City; purple tube/pool/lower-body glow; brake fading the whole screen (sky included); montage 07 → 02 → 04 → 06 ending in black; then the deck. If D > 70 ms: report (the background grew; strips 6 → 7).

---

### Task 5: Docs

**Files:** `README.md`, the three boot-scene specs' status lines.

- [ ] **Step 1: README** — replace the `### Boot scene` paragraph with:
```markdown
While the greeting plays and WiFi connects, the screen shows a night drive: the owner's silver
Honda City (pixel art reshaped from a real side profile; tinted windows, LED fog lamps, purple
underglow) cruises through a dark car park lit by white LED street lamps, for at least 5 s and
until WiFi is up. Then it brakes as the screen fades to black and a short montage of the owner's
photos plays before the deck starts. If no WiFi is found the car keeps cruising ("WiFi not
found - retrying...").

The montage is built locally from photos in `montage-photos/` (`python tools/gen_montage.py`
→ `data/montage.bin`); both are git-ignored because the photos show the number plate. Without
it the scene ends after the brake fade. The pixel car is generated by
`python tools/gen_car_sprite.py` (`--png out.png` for a preview).
```
  Architecture list: replace the `- **Boot scene**` entry with
```markdown
- **Boot scene** (`ui/bootscene`, `ui/car_sprite`, `ui/montage`, `util/bootanim`,
  `util/loudness`) — own task at boot; cruising: sky drawn once, y 72–239 re-rendered through one
  320×24 strip (15 KB, freed before the montage); brake: whole-screen fade; montage: JPEG frames
  from the embedded `data/montage.bin` decoded straight to the display (~10 fps). `setup()`
  waits for it; `loop()` and the late-WiFi path wait while active.
```
  and in the "Pure, host-tested modules" list replace `hero_sprite` with `montage` (if present) and keep `car_sprite`.
- [ ] **Step 2: Spec status** — v3 spec: `Status: implemented (feat/boot-scene, <Task 1–4 short hashes>).`; v2 spec: `Status: partly implemented; the photo-hero ending (§3–§4) was replaced by v3 (2026-10-09-boot-scene-v3-design.md).`; v1 spec: `Status: implemented (feat/boot-scene); the drive-off was replaced in v2/v3.`
- [ ] **Step 3: Commit**
```bash
git add README.md docs/superpowers/specs/2026-10-09-boot-scene-design.md docs/superpowers/specs/2026-10-09-boot-scene-v2-design.md docs/superpowers/specs/2026-10-09-boot-scene-v3-design.md
git commit -m "docs: boot scene v3 in README, spec status

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
