# Calendar Display Projects

A monorepo for ESP32-based display projects, including e-paper church schedule displays and LCD development boards.

## Repository Structure

```
Calendar/
├── apps/                    # Application projects
│   ├── church-calendar/     # Church schedule e-paper display
│   ├── ble-idf-test/        # ESP32-S3 BLE WiFi provisioner
│   ├── ble-test/            # ESP32-C6 BLE server
│   └── ...
├── docs/                    # Shared documentation
├── libs/                    # Shared libraries (e.g., WaveshareS3LCD147)
└── reference/               # Reference implementations & vendor demos
```

## Apps

### church-calendar
Church schedule display for 7.5" e-paper. Connects to WiFi, fetches schedule from API, renders it, then enters deep sleep for months of battery life.

**Hardware:** DFRobot FireBeetle 2 ESP32-C6, Waveshare 7.5" E-Paper (800x480)

[Full documentation →](apps/church-calendar/README.md)

### ble-idf-test (S3 WiFi Provisioner)
BLE client on Waveshare ESP32-S3-LCD-1.47B that scans WiFi networks, sends credentials to a C6 via BLE.
- Tilt-based navigation using QMI8658 IMU
- Color LCD display

**Hardware:** Waveshare ESP32-S3-LCD-1.47B (172x320 ST7789 IPS display)

### ble-test (C6 BLE Server)
BLE server on FireBeetle C6 with battery service, WiFi config service, and deep sleep support.

**Hardware:** DFRobot FireBeetle 2 ESP32-C6

## Quick Start

Each app is a standalone PlatformIO project:

```bash
# Build an app
cd apps/church-calendar
pio run

# Upload to device
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
```

## Development

### Prerequisites
- [PlatformIO](https://platformio.org/) (CLI or IDE)
- ESP32 toolchain (installed automatically by PlatformIO)

### Configuration
Apps requiring configuration (WiFi, API keys) use a `config.h` file:
1. Copy `config_template.h` to `config.h`
2. Edit with your settings
3. `config.h` is gitignored for security

## Hardware Platforms

| Platform | Display | MCU | Features |
|----------|---------|-----|----------|
| Waveshare 7.5" E-Paper | 800x480 B/W | ESP32 | Ultra-low power, weeks of battery |
| ESP32-S3-LCD-1.47B | 172x320 IPS | ESP32-S3 | Color LCD, IMU, WiFi/BLE |

## License

MIT License - See individual app directories for specific licensing.
