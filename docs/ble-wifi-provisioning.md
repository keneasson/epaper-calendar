# BLE WiFi Provisioning System

## Overview

Two-device BLE system for WiFi provisioning:
- **ESP32-S3** (Waveshare LCD-1.47B): BLE client, scans WiFi, sends credentials
- **ESP32-C6** (FireBeetle 2): BLE server, receives credentials, connects to WiFi

## Hardware

### ESP32-S3 (BLE Client / WiFi Scanner)
- **Board**: Waveshare ESP32-S3-LCD-1.47B
- **Display**: ST7789 172x320 IPS LCD
- **IMU**: QMI8658 (tilt navigation)
- **USB Port**: `/dev/cu.usbmodem1101`

### ESP32-C6 (BLE Server / WiFi Connector)
- **Board**: DFRobot FireBeetle 2 ESP32-C6
- **Battery Pin**: GPIO 0 (A0)
- **USB Port**: `/dev/cu.usbmodem2101`

## BLE Service UUIDs

```cpp
// Standard Battery Service
#define BATTERY_SERVICE_UUID        "180F"
#define BATTERY_LEVEL_CHAR_UUID     "2A19"

// Custom WiFi Config Service
#define WIFI_SERVICE_UUID           "12345678-1234-1234-1234-123456789abc"
#define WIFI_SSID_CHAR_UUID         "12345678-1234-1234-1234-123456789ab1"
#define WIFI_PASS_CHAR_UUID         "12345678-1234-1234-1234-123456789ab2"
#define WIFI_STATUS_CHAR_UUID       "12345678-1234-1234-1234-123456789ab3"
#define WIFI_COMMAND_CHAR_UUID      "12345678-1234-1234-1234-123456789ab4"
#define WIFI_MESSAGE_CHAR_UUID      "12345678-1234-1234-1234-123456789ab5"
```

## C6 Battery Reading (FireBeetle 2 ESP32-C6)

**Critical**: Use DFRobot's formula for accurate battery reading:

```cpp
uint8_t getBatteryLevel() {
    const int batteryPin = 0;  // GPIO0 (A0) for FireBeetle C6

    analogSetAttenuation(ADC_11db);  // 0-3.3V range

    // Average 16 readings
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(batteryPin);
    }
    uint32_t reading = sum / 16;

    // Convert to millivolts (12-bit ADC, 3.3V reference)
    float adcMillivolts = (reading / 4095.0) * 3300.0;

    // DFRobot FireBeetle 2 ESP32-C6 formula:
    float batteryMillivolts = (adcMillivolts * 2.1218) + 1000.0;
    float batteryVoltage = batteryMillivolts / 1000.0;

    // Convert to percentage (3.0V = 0%, 4.2V = 100%)
    int percent = (int)((batteryVoltage - 3.0) / (4.2 - 3.0) * 100);
    return (uint8_t)constrain(percent, 0, 100);
}
```

## C6 WiFi Reset Fix

WiFi fails after reset button press but works after firmware upload. Fix by adding cleanup at start of setup():

```cpp
void setup() {
    // Clean WiFi state on reset
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // ... rest of setup
}
```

## C6 Idle Timeout Fix

After blocking WiFi connection, refresh the `now` variable to prevent immediate timeout:

```cpp
if (shouldConnectWifi) {
    shouldConnectWifi = false;
    connectToWifi();
    now = millis();  // CRITICAL: Refresh after blocking operation
}
```

## S3 Display Configuration (LovyanGFX)

**Critical**: ST7789 controller has 240x320 memory, but panel is 172x320. Must set `memory_width=240`:

```cpp
cfg_panel.memory_width = 240;   // Controller memory (NOT panel size!)
cfg_panel.memory_height = 320;
cfg_panel.panel_width = 172;    // Visible pixels
cfg_panel.panel_height = 320;
cfg_panel.offset_x = 34;        // (240-172)/2 = 34
cfg_panel.offset_y = 0;
cfg_panel.invert = true;        // Required for correct colors
```

If you set `memory_width=172`, display will clip after rotation - only half visible!

See: `libs/waveshare-s3-lcd-147/src/WaveshareS3LCD147.h`

## S3 Pin Definitions

```cpp
// Display SPI
#define WS_LCD_PIN_SCLK     40
#define WS_LCD_PIN_MOSI     45
#define WS_LCD_PIN_DC       41
#define WS_LCD_PIN_CS       42
#define WS_LCD_PIN_RST      39
#define WS_LCD_PIN_BL       46

// IMU I2C
#define WS_IMU_PIN_SDA      48
#define WS_IMU_PIN_SCL      47
#define WS_IMU_ADDR         0x6B

// Other
#define WS_BOOT_BUTTON      0
#define WS_NEOPIXEL         38
```

## WiFi Status Codes

```cpp
enum WifiStatus {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING = 1,
    WIFI_STATUS_CONNECTED = 2,
    WIFI_STATUS_FAILED = 3,
    WIFI_STATUS_DISCONNECTED = 4
};
```

## Build Flags

### C6 (ble-test)
```ini
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1  # Enable USB serial
    -DCORE_DEBUG_LEVEL=3
```

### S3 (ble-idf-test)
```ini
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
    -DCORE_DEBUG_LEVEL=3
```

## Project Locations

- S3 BLE Client: `apps/ble-idf-test/`
- C6 BLE Server: `apps/ble-test/`
- S3 Display Library: `libs/waveshare-s3-lcd-147/`
- Church Calendar: `apps/church-calendar/`

## Message Passing (C6 to S3)

C6 sends periodic messages via `WIFI_MESSAGE_CHAR_UUID`:
- Uses `RTC_DATA_ATTR int wakeCount` to persist across deep sleep
- Updates every 3 minutes in DEBUG_MODE
- S3 subscribes to notifications and displays on connected screen
