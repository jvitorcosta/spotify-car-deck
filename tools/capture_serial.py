"""Capture timestamped serial output:
    python tools/capture_serial.py <port> <seconds> <out.log> [--no-reset]

<port> is the board's serial port (see `pio device list`). Needs pyserial (tools/requirements.txt).
On the CYD (USB-UART bridge) opening the port resets the board. --no-reset keeps DTR/RTS low,
for the ESP32-S3's native USB. A native-USB port disappears while the board resets; the capture
reopens it when it comes back.
"""
import sys
import time

import serial

args = [a for a in sys.argv[1:] if a != "--no-reset"]
no_reset = len(args) != len(sys.argv) - 1
port, secs, path = args[0], float(args[1]), args[2]


def open_port():
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 1
    if no_reset:
        s.dtr = False
        s.rts = False
    s.open()
    return s


t0 = time.time()
with open(path, "w", encoding="utf-8") as out:
    s = None
    while time.time() - t0 < secs:
        try:
            if s is None:
                s = open_port()
            line = s.readline()
            if line:
                out.write("%7.1f %s" % (time.time() - t0, line.decode(errors="replace")))
                out.flush()
        except (serial.SerialException, OSError):
            if s is not None:
                s.close()
            s = None
            time.sleep(0.2)                 # port gone (native USB re-enumerating): retry
    if s is not None:
        s.close()
print("wrote", path)
