"""Capture timestamped serial output: python tools/capture_serial.py COM11 600 out.log"""
import sys
import time

import serial

port, secs, path = sys.argv[1], float(sys.argv[2]), sys.argv[3]
with serial.Serial(port, 115200, timeout=1) as s, open(path, "w", encoding="utf-8") as out:
    t0 = time.time()
    while time.time() - t0 < secs:
        line = s.readline()
        if line:
            out.write("%7.1f %s" % (time.time() - t0, line.decode(errors="replace")))
            out.flush()
print("wrote", path)
