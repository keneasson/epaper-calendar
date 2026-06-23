# Diagnostic utilities

Developer tools for bringing up and debugging the Home Calendar hardware.

## `serial_capture.py`

Captures the ESP32-C6 boot log without an interactive terminal. Unlike
`pio device monitor`, it can run from scripts/CI and survives the C6's USB
re-enumeration by polling for the port to (re)appear before attaching.

```bash
# Auto-detect the port, wait for a reconnect, capture 50s of boot log:
python3 tools/diagnostics/serial_capture.py --out /tmp/boot.log

# Pin a specific port and a longer window:
python3 tools/diagnostics/serial_capture.py --port /dev/cu.usbmodem2101 --seconds 60
```

### ESP32-C6 reconnect procedure (board bug workaround)

The C6's native USB-Serial/JTAG auto-reset is unreliable, so to force a clean
boot you must **power-cycle**:

1. Disconnect the battery.
2. Unplug USB.
3. Hold the **RESET** button.
4. Replug USB (still holding RESET).
5. Release RESET.

Start `serial_capture.py` first; it waits up to 90s for the port, then streams
the log. Durable hardware fix: add a **1µF capacitor between EN and GND**.

Requires `pyserial` (already pulled in by PlatformIO; otherwise `pip install pyserial`).
