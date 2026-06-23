# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working in this repository.

## What this is

A monorepo of low-power ESP32 e-paper / LCD display projects plus a serverless backend. The **flagship product is `apps/home-calendar`** — ESP32-C6 firmware that renders a Google Calendar on a 7.5" three-color e-paper panel and deep-sleeps for months on a LiPo.

This is a **public, go-to-market open-hardware product**. Treat everything — code, commit messages, issues, docs — as customer-facing, and hold an enterprise-quality bar. Fixes for field problems must be device-side; "tell the customer to change their router/hardware" is not acceptable.

## Monorepo layout

```
apps/
  home-calendar/      ★ Production ESP32-C6 firmware (Google Calendar e-paper). The main app.
  home-calendar-api/    AWS Lambda backend (SST, TypeScript) — Google Calendar reader.
  church-calendar/      Church schedule display (ESP32-E + C6); has a native desktop simulator.
  ble-test/             ESP32-C6 BLE server (provisioning bench).
  ble-idf-test/         ESP32-S3 BLE client / WiFi provisioner (Waveshare S3-LCD-1.47B + IMU).
  lcd_control/          ESP32-S3 control unit: BLE scan + WiFi provisioning + touch UI.
  lcd-demo/             ESP32-S3 LCD tilt-navigation demo.
libs/                   Shared libs: firebeetle-battery, waveshare-s3-lcd-147.
docs/                   Assembly, wiring, provisioning, quick-start.
tools/diagnostics/      serial_capture.py — non-interactive serial boot-log capture.
reference/              Vendor SDKs — gitignored, local only. Do not commit.
```

Each `apps/*` firmware project is a **self-contained PlatformIO project** with its own `platformio.ini`. There is no top-level PlatformIO project — `cd` into the app first.

## Build commands

**home-calendar (flagship, ESP32-C6)** — requires **PlatformIO Core ≥ 6.1.19** (pinned pioarduino 55.03.39 = arduino-esp32 3.3.9 / ESP-IDF 5.5.4; `pio upgrade` if older):

```bash
cd apps/home-calendar
pio run -e firebeetle_c6                 # production build (default env)
pio run -e firebeetle_c6 -t upload       # flash
pio run -e firebeetle_c6_diag -t upload  # USB-CDC serial + WIFI_DIAG network scan (bench only)
pio run -e firebeetle_c6_prod            # minimal logging
pio device monitor -b 115200
```

Envs: `firebeetle_c6` (default) · `_diag` (USB-CDC + verbose + WiFi scan) · `_debug` · `_prod`.

**church-calendar (ESP32-E + C6 + native sim):**

```bash
cd apps/church-calendar
pio run -e firebeetle      # ESP32-E production (default)
pio run -e firebeetle_c6   # ESP32-C6 variant
pio run -e native          # desktop simulation (see below)
```

**home-calendar-api (SST / AWS):**

```bash
cd apps/home-calendar-api
npm install
npm run deploy             # = sst deploy --stage keneasson  — do NOT use --stage prod
```

> `--stage prod` creates a second, parallel stack and orphaned duplicates. Always deploy to the `keneasson` stage.

## home-calendar architecture

`main.cpp` orchestrates; each module owns one concern:

| Module | Purpose |
|--------|---------|
| `main.cpp` | Boot → battery → display → WiFi → time → API → render → sleep |
| `calendar_data.h` | Data types: `CalendarData`, `Appointment`, `BatteryStatus`, `ResultCode`, `WifiFailCause` |
| `battery` | ADC read with averaging, voltage → percent |
| `wifi_manager` | Connection-compatibility ladder + disconnect-reason classification |
| `api_client` | HTTPS GET + JSON parse into `CalendarData` (ETag-aware) |
| `cache_manager` | SPIFFS cache of the last-known calendar (offline fallback) |
| `display` | All e-paper rendering, layout, error / footer states |
| `power_manager` | NTP sync, timezone, sleep-time calc, deep sleep |

**Program flow:** read battery (hibernate if critical) → init display → set timezone → `wifi_connect()` → NTP sync (non-fatal) → `api_fetch_calendar()` with ETag → render → deep sleep until the next scheduled wake. On WiFi or API failure it falls back to the SPIFFS cache and shows a cause-specific footer note.

**WiFi resilience (the hard-won part).** `wifi_connect()` walks a profile ladder (802.11ax → b/g/n → low-TX → WPA2-only/PMF-off, plus a diagnostic alt-MAC rung) for modern WiFi-6 / WPA3 gateways, and classifies failures from the 802.11 disconnect **reason code** into `WifiFailCause`. `main.cpp` / `display` turn that into customer-actionable text — e.g. reason-2 `AUTH_EXPIRE` on every rung at good signal ⇒ *"Your router is rejecting your connection."* The winning rung is cached in RTC memory for fast reconnect after deep sleep. See `apps/home-calendar/src/wifi_manager.cpp`.

## Configuration

Firmware credentials live in a **gitignored `config.h`** (per app):

```bash
cd apps/<app>/src && cp config_template.h config.h
```

`home-calendar/config.h` holds `WIFI_SSID`, `WIFI_PASSWORD`, `API_ENDPOINT`, `API_KEY`, the wake schedule, pins, and timezone. The backend keeps secrets in SST (`sst.Secret`), never in source; the Google service-account `*-credentials.json` is gitignored.

## Hardware pins — home-calendar (ESP32-C6 + DESPI-C02 + GDEY075Z08)

| Function | GPIO | | Function | GPIO |
|----------|------|-|----------|------|
| EPD_CS | 1 | | SPI_MOSI (SDI) | 22 |
| EPD_DC | 2 | | SPI_CLK (SCK) | 23 |
| EPD_RST | 3 | | BATTERY (ADC1_CH0) | 0 |
| EPD_BUSY | 4 | | WAKE_BUTTON | 5 (must be GPIO 0–7 for deep-sleep wake) |

(church-calendar on ESP32-E uses different pins — see its `config_template.h`.)

## Secrets & CI — do not regress

- **Never commit real credentials.** Firmware → gitignored `config.h`; backend → SST secrets. `**/config.h`, `*-credentials.json`, `*.pem`/`*.key` are gitignored.
- **gitleaks** runs in CI (`.github/workflows/secrets-scan.yml`) on every push/PR, and locally via `.githooks/pre-commit` (enable once: `git config core.hooksPath .githooks`). Custom rules + allowlist in `.gitleaks.toml`. Run `gitleaks detect` before pushing.

## Conventions & gotchas (learned the hard way)

- **`WiFi.disconnect(true)`** — the bool is `wifioff`; it stops the WiFi driver, silently no-oping any later `esp_wifi_*` call. Use `WiFi.disconnect()` between retries. (church-calendar still has the old pattern.)
- **C6/H2 MAC:** `esp_efuse_mac_get_default()` returns the EUI-64 base (FF:FE infix), not the STA MAC — use `esp_read_mac(mac, ESP_MAC_WIFI_STA)`.
- **`**/config.h` inside a `/* */` block comment** terminates the comment early (the `*/`) and breaks the build — use line comments or reword.
- **Arduino core hijack:** `STA.cpp`'s `first_connect` retries once on first failure *ignoring* `setAutoReconnect(false)`, and can hold the STA in `connecting` (`sta is connecting, cannot set config`). Check `esp_wifi_set_config` return values and force-idle on error.
- **Flashing the C6:** its native USB-Serial/JTAG drops in deep sleep — press RESET to bring the port back. Pin `upload_port`/`monitor_port = /dev/cu.usbmodem*` so PlatformIO doesn't auto-select the wrong serial device. `tools/diagnostics/serial_capture.py` captures boot logs non-interactively.

## Native simulation (church-calendar)

```bash
cd apps/church-calendar && pio run -e native && ./.pio/build/native/program
```

Outputs `display_output.bmp` + `display_text.log`; mock data in `simulation/api_response.json`; WiFi / HTTP / NTP are mocked.

## Debug flags

Set via `platformio.ini` `build_flags` or `config.h`:
- `DEBUG_MODE` — serial output (default 1)
- `TEST_MODE` — prevents deep sleep (default 0; drains battery)
- `WIFI_DIAG` (home-calendar `_diag` env) — scan and log all visible networks before connecting
