#!/usr/bin/env python3
"""Serial boot-log capture for the ESP32-C6 Home Calendar board.

Why this exists
---------------
`pio device monitor` needs an interactive TTY, so it can't be driven from
scripts/CI or background tasks. It also can't survive the ESP32-C6's USB drop.
This utility instead *waits* for the board's USB-serial port to appear, attaches
as soon as it does, and streams the boot log to stdout (and optionally a file).

Board quirk it works around
---------------------------
The C6's native USB-Serial/JTAG auto-reset is unreliable. To get a clean boot
you often must power-cycle: disconnect battery -> unplug USB -> hold RESET ->
replug USB -> release RESET. That re-enumerates USB, so a normal monitor loses
the port. This script polls for the port to reappear and re-attaches.
(Durable hardware fix: add a 1uF cap between EN and GND.)

Usage
-----
    python3 tools/diagnostics/serial_capture.py                 # auto-detect port
    python3 tools/diagnostics/serial_capture.py --port /dev/cu.usbmodem2101
    python3 tools/diagnostics/serial_capture.py --seconds 60 --out boot.log

Then perform the board reconnect; the boot log is captured automatically.
"""
from __future__ import annotations

import argparse
import glob
import sys
import time

try:
    import serial  # pyserial — already a PlatformIO dependency
except ImportError:
    sys.exit("pyserial not found. Install with: pip install pyserial")

DEFAULT_PORT_GLOBS = ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/ttyUSB*", "/dev/ttyACM*")
EXCLUDE = ("debug-console", "Bluetooth", "OnePlus")


def find_port(globs: tuple[str, ...]) -> str | None:
    for pattern in globs:
        for path in sorted(glob.glob(pattern)):
            if not any(token in path for token in EXCLUDE):
                return path
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="Serial port (default: auto-detect)")
    ap.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    ap.add_argument("--seconds", type=int, default=50, help="Capture window once attached (default: 50)")
    ap.add_argument("--wait", type=int, default=90, help="Seconds to wait for the port to appear (default: 90)")
    ap.add_argument("--out", help="Also tee the captured log to this file")
    args = ap.parse_args()

    sink = open(args.out, "w") if args.out else None

    def emit(text: str) -> None:
        sys.stdout.write(text)
        sys.stdout.flush()
        if sink:
            sink.write(text)
            sink.flush()

    emit("[waiting for board USB port — do your reconnect now: "
         "unplug USB, hold RESET, replug USB, release RESET]\n")

    deadline = time.time() + args.wait
    ser = None
    while time.time() < deadline and ser is None:
        port = args.port or find_port(DEFAULT_PORT_GLOBS)
        if port:
            try:
                ser = serial.Serial(port, args.baud, timeout=1)
            except Exception:
                ser = None
                time.sleep(0.3)
        else:
            time.sleep(0.3)

    if ser is None:
        emit(f"[port never appeared within {args.wait}s]\n")
        return 1

    emit(f"[attached to {ser.port} @{args.baud} — capturing for {args.seconds}s]\n")
    end = time.time() + args.seconds
    while time.time() < end:
        try:
            line = ser.readline()
        except Exception as exc:  # USB dropped (e.g. deep sleep) — end cleanly
            emit(f"\n[read ended: {exc}]\n")
            break
        if line:
            emit(line.decode("utf-8", "replace"))

    emit("\n[reader done]\n")
    try:
        ser.close()
    except Exception:
        pass
    if sink:
        sink.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
