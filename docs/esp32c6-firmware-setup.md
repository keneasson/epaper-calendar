# ESP32-C6 Firmware Setup Guide

This guide documents the working configuration for the Church Schedule E-Paper Display on the DFRobot FireBeetle 2 ESP32-C6.

## Platform Requirements

Use **Pioarduino platform 55.03.35 or later** (based on ESP-IDF 5.5+). Earlier platforms and the standard `espressif32` platform have broken or missing SSL/TLS support for the C6.

```ini
[env:firebeetle_c6]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.35/platform-espressif32.zip
board = esp32-c6-devkitc-1
framework = arduino
```

### Why Pioarduino?

- **NetworkClientSecure** - Full TLS/SSL support
- **ESP-IDF 5.5+** - Proper mbedTLS integration for C6
- **Arduino compatibility** - Works with standard Arduino libraries

### Platforms That Don't Work

| Platform | Issue |
|----------|-------|
| `espressif32@6.x` | Missing `WiFiClientSecure.h` for C6 |
| `platformio/espressif32` | Old ESP-IDF, SSL broken |
| `tasmota/espressif32` | `WiFiClientSecure.h` not found |

## Full PlatformIO Configuration

```ini
[env:firebeetle_c6]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.35/platform-espressif32.zip
board = esp32-c6-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = partitions_c6.csv

lib_deps =
    zinggjm/GxEPD2@^1.5.8
    adafruit/Adafruit GFX Library@^1.11.9
    bblanchon/ArduinoJson@^6.21.5

build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=0
    -DESP32_C6_BUILD=1
    -DDEBUG_MODE=1
    -Wall
```

## Custom Partition Table

The firmware requires ~1.04MB, exceeding the default 1MB app partition. Create `partitions_c6.csv`:

```csv
# ESP32-C6 Custom Partitions - 4MB Flash with larger app partition
# Name,   Type, SubType, Offset,   Size,    Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1E0000,
spiffs,   data, spiffs,  0x1F0000, 0x10000,
```

This provides 1.9MB for the application.

## HTTPS Client Code

Use `NetworkClientSecure` for HTTPS connections:

```cpp
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

ResultCode fetch_data() {
    HTTPClient http;
    NetworkClientSecure client;
    client.setInsecure();  // Skip cert validation (or use setCACert)

    if (!http.begin(client, "https://api.example.com/data")) {
        return ResultCode::ApiRequestFailed;
    }

    http.setTimeout(10000);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return ResultCode::ApiRequestFailed;
    }

    String response = http.getString();
    http.end();
    return ResultCode::Success;
}
```

**Key:** Use `NetworkClientSecure` not `WiFiClientSecure` on ESP32-C6.

## Deep Sleep Wake Pins

**Only GPIO 0-7 can wake from deep sleep on ESP32-C6.** GPIO 9 (boot button) does not work.

```cpp
#define BUTTON_PIN  5  // Must be GPIO 0-7

esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);
esp_deep_sleep_start();
```

## Firmware Upload

### Normal Upload

```bash
pio run -e firebeetle_c6 -t upload
```

### If Device is in Deep Sleep

1. Disconnect battery
2. Hold BOOT, press and release RESET
3. Release BOOT after 1 second
4. Run upload command immediately

## Build Flags

| Flag | Purpose |
|------|---------|
| `ESP32_C6_BUILD` | Enables C6-specific pin definitions |
| `DEBUG_MODE` | Enables serial logging |
| `TEST_MODE` | Prevents deep sleep (for testing) |
| `DISABLE_BLE_PROVISIONING` | Reduces firmware size |
| `ARDUINO_USB_CDC_ON_BOOT=0` | Uses hardware UART for serial |

## Display Configuration

For the GooDisplay GDEY075Z08 3-color display in portrait mode:

```cpp
#define DISPLAY_ROTATION  3  // 90 CCW for portrait
#define DISPLAY_3COLOR    1  // Enable red color support
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Firmware too large | Add `board_build.partitions = partitions_c6.csv` |
| HTTPS fails | Use Pioarduino platform 55.03.35+ |
| Invalid deep sleep GPIO | Use GPIO 0-7 only |
| Serial garbage | Set `ARDUINO_USB_CDC_ON_BOOT=0` |

## References

- [Pioarduino Platform](https://github.com/pioarduino/platform-espressif32)
- [GxEPD2 Library](https://github.com/ZinggJM/GxEPD2)
- [DFRobot FireBeetle 2 ESP32-C6](https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6)
- [GooDisplay](https://www.good-display.com/)
