# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Church Schedule E-Ink Display - an ESP32-based system that displays church schedules on a low-power 7.5" e-paper display. Connects to WiFi, fetches schedule from an API, renders it, then enters deep sleep for months of battery life.

**Hardware:** DFRobot FireBeetle 2 ESP32-E, Waveshare 7.5" E-Paper (800x480), DESPI-C02 adapter, 3.7V LiPo battery.

## Build Commands

```bash
# Build for hardware
pio run -e firebeetle

# Build with debug output
pio run -e firebeetle_debug

# Upload to device
pio run -e firebeetle -t upload

# Monitor serial output
pio device monitor -b 115200

# Build for native simulation
pio run -e native

# Run tests
pio test -e native
```

## Project Structure

```
Calendar/
├── platformio.ini          # Build config, dependencies
├── src/
│   ├── main.cpp            # Entry point, orchestration
│   ├── config.h            # User config (gitignored)
│   ├── config_template.h   # Config template
│   ├── schedule.h          # Data structures (Schedule, Event, ResultCode)
│   ├── battery.h/.cpp      # Battery monitoring
│   ├── wifi_manager.h/.cpp # WiFi connection
│   ├── api_client.h/.cpp   # HTTP/JSON fetching
│   ├── display.h/.cpp      # E-paper rendering
│   └── power_manager.h/.cpp# Sleep/wake scheduling
├── docs/                   # Documentation
├── test/                   # Unit tests
└── .pio/                   # Build output (gitignored)
```

## Architecture

### Module Responsibilities

| Module | Purpose |
|--------|---------|
| `main.cpp` | Orchestration only - calls modules in sequence |
| `schedule.h` | Data structures: `Schedule`, `Event`, `BatteryStatus`, `ResultCode` |
| `battery` | ADC reading with averaging, voltage-to-percent conversion |
| `wifi_manager` | Connect/disconnect with proper retry logic |
| `api_client` | HTTP GET, JSON parsing into `Schedule` struct |
| `display` | All e-paper rendering, layout constants |
| `power_manager` | NTP sync, sleep time calculation, deep sleep entry |

### Program Flow

1. `battery_init()` + `battery_read()` - Check battery, hibernate if critical
2. `display_init()` - Initialize e-paper
3. `wifi_connect()` - Connect with retries
4. `power_sync_time()` - NTP sync (non-fatal if fails)
5. `api_fetch_schedule()` - GET + parse JSON
6. `wifi_disconnect()` - Power down radio
7. `display_schedule()` - Render to e-paper
8. `power_calculate_sleep_time()` + `power_enter_sleep()` - Deep sleep until next scheduled wake

### Error Handling

All operations return `ResultCode` enum. Failures display an error message and sleep for `SLEEP_ON_ERROR` seconds before retry.

## Configuration

Copy `src/config_template.h` to `src/config.h` and edit:

- `WIFI_SSID`, `WIFI_PASSWORD` - Network credentials
- `API_ENDPOINT`, `API_KEY` - Schedule API
- `WAKE_HOUR`, `WAKE_MINUTE` - When to update
- `UPDATE_SUNDAY` through `UPDATE_SATURDAY` - Which days to update
- `CHURCH_NAME`, `HEADER_SUBTITLE` - Display header text
- `JSON_*` defines - Field mapping if your API uses different keys

## Hardware Pins

| Function | GPIO | Notes |
|----------|------|-------|
| EPD_CS | 5 | Chip Select |
| EPD_DC | 17 | Data/Command |
| EPD_RST | 16 | Reset |
| EPD_BUSY | 4 | Busy Signal |
| SPI_MOSI | 23 | SPI Data |
| SPI_CLK | 18 | SPI Clock |
| BATTERY_PIN | 36 | ADC input |

**Important:** DESPI-C02 adapter switch must be set to "0.47".

## Build Environments

- `firebeetle` - Production build for hardware
- `firebeetle_debug` - Debug build with verbose logging
- `native` - Desktop simulation (runs on macOS/Linux)
- `wokwi` - Wokwi browser simulator build

## Native Simulation

Run the display logic without hardware:

```bash
# Build
pio run -e native

# Run simulation
./.pio/build/native/program
```

**Output files:**
- `display_output.bmp` - Visual render of the display (800x480 grayscale)
- `display_text.log` - Text positions and content for verification

**Mock data:**
- Edit `simulation/api_response.json` to change the schedule data
- Simulation uses system time instead of NTP
- WiFi/HTTP operations are mocked

## Debug Flags

Set via `platformio.ini` build_flags or in config.h:
- `DEBUG_MODE` - Serial output (default: 1)
- `TEST_MODE` - Prevents deep sleep (default: 0)
