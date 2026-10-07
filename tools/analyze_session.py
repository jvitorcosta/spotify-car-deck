"""Analyze a timestamped serial capture from the deck: python tools/analyze_session.py <log>"""
import collections
import re
import statistics
import sys

lines = []
for raw in open(sys.argv[1], encoding="utf-8", errors="replace"):
    m = re.match(r"\s*([\d.]+) (.*)", raw.rstrip("\n"))
    if m:
        lines.append((float(m.group(1)), m.group(2)))
print("lines:", len(lines), " span: %.0fs" % (lines[-1][0] - lines[0][0] if lines else 0))

steps = collections.defaultdict(list)
for t, s in lines:
    m = re.match(r"\[net\] (poll|player|art|art failed|lyrics|walk|prefetch|prefetch failed) (\d+)ms", s)
    if m:
        steps[m.group(1)].append(int(m.group(2)))
print("\n== step durations (ms) ==")
for k, v in sorted(steps.items()):
    v = sorted(v)
    print("%-16s n=%3d min=%5d median=%5d p90=%5d max=%5d" % (
        k, len(v), v[0], statistics.median(v), v[int(0.9 * (len(v) - 1))], v[-1]))

print("\n== tracks (seconds after the track change) ==")
idx = [(i, t, s) for i, (t, s) in enumerate(lines) if s.startswith("[net] track gen=")]
for k, (i, t, s) in enumerate(idx):
    end = idx[k + 1][0] if k + 1 < len(idx) else len(lines)
    got = {}
    for t2, s2 in lines[i + 1:end]:
        for key in ("art", "lyrics", "walk", "prefetch"):
            if key not in got and re.match(r"\[net\] %s \d+ms" % key, s2):
                got[key] = t2 - t
    print("%-40s %s" % (s[:40], "  ".join("%s=%.1f" % kv for kv in got.items())))

mem = [(int(a), int(b)) for _, s in lines for m in [re.search(r"\[mem\].*free=(\d+) largest=(\d+)", s)]
       if m for a, b in [m.groups()]]
if mem:
    print("\n== byte-addressable RAM ==\nfree min=%d max=%d  largest min=%d max=%d" % (
        min(a for a, _ in mem), max(a for a, _ in mem), min(b for _, b in mem), max(b for _, b in mem)))

print("\n== failures ==")
for name, pat in (("allocfail", r"\[allocfail\]"), ("TLS alloc -32512", "-32512"),
                  ("poll HTTP -1", r"poll HTTP -1"), ("fetch errors", r"\[fetch\]"),
                  ("restarts", r"rst:|restarting"), ("deferred steps", r"\[mem\] defer")):
    print("%-18s %d" % (name, sum(1 for _, s in lines if re.search(pat, s))))
