# home-calendar

Production ESP32-C6 firmware that renders a Google Calendar on a 7.5" three-color e-paper panel and deep-sleeps between hourly sync checks — months of runtime on a single LiPo charge.

It fetches calendar data from the [home-calendar-api](../home-calendar-api) serverless backend over HTTPS, draws upcoming appointments plus a month grid in portrait orientation, then sleeps. It's built for real homes on real routers: the WiFi stack negotiates modern WiFi-6 / WPA3 gateways and turns connection failures into plain-language, fix-it-yourself messages on the panel.

## Hardware

| Part | Notes |
|------|-------|
| DFRobot FireBeetle 2 ESP32-C6 | MCU; deep-sleep timer + GPIO wake |
| GooDisplay GDEY075Z08 | 7.5" 800×480 three-color (black/white/red) e-paper, driven portrait (480×800) |
| DESPI-C02 adapter | e-paper ribbon → SPI breakout |
| 3.7 V LiPo battery | via the FireBeetle's JST connector |
| Momentary button *(optional)* | between GPIO 5 and GND, for force-refresh wake |

## How it works

`main.cpp` is orchestration only; each module owns one concern:

| Module | Responsibility |
|--------|----------------|
| `main.cpp` | Boot → battery → display → WiFi → time → API → render → sleep |
| `calendar_data.h` | Data types: `CalendarData`, `Appointment`, `BatteryStatus`, `ResultCode`, `WifiFailCause` |
| `battery` | ADC read with averaging, voltage → percent, critical-hibernate |
| `wifi_manager` | Connection-compatibility ladder + 802.11 disconnect-reason classification |
| `api_client` | HTTPS GET + JSON parse into `CalendarData`, ETag-aware (HTTP 304) |
| `cache_manager` | SPIFFS cache of the last-known calendar for offline fallback |
| `display` | All e-paper rendering — appointments, month grid, footer, error states |
| `power_manager` | NTP sync, timezone, sleep-time calculation, deep sleep |

**Wake cycle.** The device wakes hourly between `DAY_START_HOUR` and `DAY_END_HOUR` for a *silent* sync check — it only redraws the panel if the calendar actually changed (detected via ETag / HTTP 304), so e-paper refreshes and battery use stay minimal. A dedicated wake at `DATE_CHANGE_HOUR` (1 AM) refreshes the date even when data is unchanged. Pressing the wake button forces an immediate refresh.

**Offline resilience.** If WiFi or the API is unreachable, the firmware renders the last calendar from its SPIFFS cache and shows a cause-specific note in the footer instead of the date. After `CACHE_MAX_AGE_SECONDS` (24 h) the cached view is flagged stale.

## Resilient WiFi

Modern gateways (WiFi-6 / WPA3 transition, band steering) interoperate badly with some of the C6's defaults. `wifi_connect()` walks a compatibility ladder, dropping one capability per rung, and caches the winning rung in RTC memory for fast reconnect after deep sleep:

1. standard — 802.11ax, 19.5 dBm
2. compat — 802.11 b/g/n only (HE stripped)
3. compat low-power — b/g/n, 8.5 dBm
4. WPA2-only — PMF off, forces the legacy WPA2-PSK path
5. *(diagnostic)* alt-MAC — locally-administered MAC

Every failed attempt's 802.11 **disconnect reason code** is logged and classified into a `WifiFailCause`, which the UI turns into a self-service message:

| What happened | On-screen message |
|---------------|-------------------|
| Router never answers our auth on any rung (blocked/wedged device) | **Your router is rejecting your connection** |
| Key handshake failed | WiFi password looks incorrect |
| SSID not visible on 2.4 GHz | WiFi not found — needs 2.4 GHz network |
| AP security below our floor | WiFi security mode not supported |
| WiFi fine, backend unreachable | WiFi OK — calendar service unreachable |

> Background: [docs/esp32c6-firmware-setup.md](../../docs/esp32c6-firmware-setup.md). The full root-cause investigation behind this design is in issue #2.

## Build & flash

A standalone [PlatformIO](https://platformio.org/) project. **Requires PlatformIO Core ≥ 6.1.19** — the pinned pioarduino 55.03.39 platform (arduino-esp32 3.3.9 / ESP-IDF 5.5.4) depends on it (`pio upgrade` if older).

```bash
cd apps/home-calendar
pio run -e firebeetle_c6                 # production build (default env)
pio run -e firebeetle_c6 -t upload       # flash
pio device monitor -b 115200             # serial monitor
```

| Env | Use |
|-----|-----|
| `firebeetle_c6` | Production (default) |
| `firebeetle_c6_diag` | USB-CDC serial + `WIFI_DIAG` network scan — bench bring-up/debug |
| `firebeetle_c6_debug` | Verbose logging, no USB-CDC |
| `firebeetle_c6_prod` | Minimal logging |

> **Flashing tip:** the C6's native USB-Serial/JTAG port disappears in deep sleep — press **RESET** to bring it back before uploading. For capturing boot logs non-interactively (it survives the USB re-enumeration), use [`tools/diagnostics/serial_capture.py`](../../tools/diagnostics/serial_capture.py).

## Configuration

Credentials and settings live in a **gitignored `config.h`**:

```bash
cd apps/home-calendar/src
cp config_template.h config.h        # then edit the values below
```

| Setting | Purpose |
|---------|---------|
| `WIFI_SSID`, `WIFI_PASSWORD` | 2.4 GHz network credentials |
| `API_ENDPOINT`, `API_KEY` | Backend URL + API key (matches the SST `ApiKey` secret) |
| `TIMEZONE` | POSIX TZ string with DST rules, e.g. `EST5EDT,M3.2.0,M11.1.0` |
| `DATE_CHANGE_HOUR`, `DAY_START_HOUR`, `DAY_END_HOUR` | Wake schedule |
| `WIFI_TIMEOUT_MS`, `WIFI_MAX_RETRIES` | Per-attempt timeout = `WIFI_TIMEOUT_MS / WIFI_MAX_RETRIES` |
| `BATTERY_LOW_WARN`, `BATTERY_CRITICAL` | 3.3 V warning / 3.0 V hibernate thresholds |
| `CACHE_FILE_PATH`, `CACHE_MAX_AGE_SECONDS` | SPIFFS cache path + stale-after age |

`config.h` is never committed (`**/config.h` is gitignored repo-wide, and gitleaks blocks accidental credential commits).

## Pins (ESP32-C6 + DESPI-C02 + GDEY075Z08)

| Function | GPIO | | Function | GPIO |
|----------|------|-|----------|------|
| EPD_CS | 1 | | SPI_MOSI (SDI) | 22 |
| EPD_DC | 2 | | SPI_CLK (SCK) | 23 |
| EPD_RST | 3 | | BATTERY (ADC1_CH0) | 0 |
| EPD_BUSY | 4 | | WAKE_BUTTON | 5 |

> **Deep-sleep wake is limited to GPIO 0–7 on the ESP32-C6** — GPIO 9 (the boot button) *cannot* wake the device, so the wake button is wired to GPIO 5. Full wiring: [docs/wiring_guide.md](../../docs/wiring_guide.md) · assembly: [docs/epaper-esp32c6-assembly.md](../../docs/epaper-esp32c6-assembly.md).

## Dependencies

Pulled automatically by PlatformIO (`platformio.ini`): `GxEPD2` (e-paper), `Adafruit GFX` (fonts/primitives), `ArduinoJson` (API parsing). Flash layout: `partitions_c6.csv`.

## Troubleshooting

- **Panel shows "Your router is rejecting your connection."** The gateway sees the device but refuses its auth (common after an ISP gateway firmware update). Reboot the router, or remove/forget the device in the router's app, then let the next wake retry.
- **"WiFi not found — needs 2.4 GHz network."** The C6 is 2.4 GHz-only; make sure the SSID is broadcast on 2.4 GHz (not 5 GHz-only).
- **Upload can't find the board.** Press RESET (the USB port drops in deep sleep); `platformio.ini` pins `upload_port`/`monitor_port` to `/dev/cu.usbmodem*` so it won't grab the wrong device.
- **Want to see what's happening?** Flash `firebeetle_c6_diag` and watch serial — it scans visible networks and logs each WiFi attempt's named reason code.
