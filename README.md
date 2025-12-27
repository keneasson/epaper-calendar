# E-Paper Calendar

A monorepo for ESP32-based display projects, including e-paper church schedule displays and LCD development boards.

## Repository Structure

```
epaper-calendar/
├── apps/                    # Application projects
│   ├── epaper-calendar/     # Church schedule e-paper display
│   ├── lcd-demo/            # ESP32-S3-LCD-1.47B demo with LovyanGFX
│   └── lcd_control/         # LCD control experiments
├── docs/                    # Shared documentation
├── libs/                    # Shared libraries
└── reference/               # Reference implementations & vendor demos
    └── waveshare-esp32s3-lcd-demo/
```

## Apps

### epaper-calendar
Church schedule display for 7.5" e-paper. Connects to WiFi, fetches schedule from API, renders it, then enters deep sleep for months of battery life.

**Hardware:** DFRobot FireBeetle 2 ESP32-E, Waveshare 7.5" E-Paper (800x480)

[Full documentation →](apps/epaper-calendar/README.md)

### lcd-demo
Demo application for the Waveshare ESP32-S3-LCD-1.47B development board. Features:
- Smooth anti-aliased fonts with LovyanGFX
- Tilt-based navigation using QMI8658 IMU
- Auto-sleep after 5 minutes of inactivity

**Hardware:** Waveshare ESP32-S3-LCD-1.47B (172x320 ST7789 IPS display)

## Quick Start

Each app is a standalone PlatformIO project:

```bash
# Build an app
cd apps/epaper-calendar
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
