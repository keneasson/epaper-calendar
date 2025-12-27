# BLE Provisioning Architecture

## Overview

This document describes the architecture for using a Waveshare ESP32-S3 LCD control unit to provision and manage multiple FireBeetle + E-Paper display units via Bluetooth Low Energy (BLE).

```
┌─────────────────────────────┐           ┌─────────────────────────────┐
│   LCD Control Unit          │           │   E-Paper Display Unit      │
│   (ESP32-S3-LCD-1.47)       │           │   (FireBeetle ESP32-C6)     │
├─────────────────────────────┤           ├─────────────────────────────┤
│                             │    BLE    │                             │
│  • Touch UI for config      │◄─────────►│  • BLE Server (GATT)        │
│  • BLE Client (Central)     │           │  • WiFi for API fetch       │
│  • Real-time status display │           │  • 7.5" E-Paper display     │
│  • Multi-device management  │           │  • Deep sleep between wakes │
│                             │           │                             │
└─────────────────────────────┘           └─────────────────────────────┘
```

## System States

### E-Paper Display Unit States

```
                    ┌──────────────┐
                    │  POWER ON    │
                    └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
              ┌─────│ CHECK CONFIG │─────┐
              │     └──────────────┘     │
              │                          │
         Has WiFi                   No WiFi Config
         Config                          │
              │                          ▼
              │                   ┌──────────────┐
              │                   │  BLE_PROV    │◄──── Advertising
              │                   │  MODE        │      "ChurchDisplay-XX"
              │                   └──────┬───────┘
              │                          │
              │                    Credentials
              │                    Received
              │                          │
              ▼                          ▼
       ┌──────────────┐           ┌──────────────┐
       │ NORMAL_OP    │◄──────────│ WIFI_TEST    │
       │              │  Success  │              │
       └──────┬───────┘           └──────────────┘
              │
              ▼
       ┌──────────────┐
       │ DEEP_SLEEP   │───────► Wake on timer/button
       └──────────────┘
```

### LCD Control Unit States

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   IDLE       │────►│  SCANNING    │────►│  CONNECTED   │
│   (Home UI)  │     │  (Find EPDs) │     │  (Manage)    │
└──────────────┘     └──────────────┘     └──────────────┘
```

## BLE Service Definition

### Service UUID
```
Church Display Service: 0x1900 (custom)
Full UUID: 00001900-0000-1000-8000-00805f9b34fb
```

### Characteristics

| Characteristic | UUID | Properties | Description |
|----------------|------|------------|-------------|
| WiFi SSID | 0x1901 | Read, Write | Network name (max 32 chars) |
| WiFi Password | 0x1902 | Write | Network password (max 64 chars) |
| WiFi Status | 0x1903 | Read, Notify | Connection status enum |
| Device Name | 0x1904 | Read, Write | Display identifier |
| Battery Level | 0x1905 | Read, Notify | 0-100 percentage |
| Battery Voltage | 0x1906 | Read | Voltage * 100 (e.g., 370 = 3.70V) |
| Last Update | 0x1907 | Read | Unix timestamp |
| Force Refresh | 0x1908 | Write | Write 0x01 to trigger |
| Error Code | 0x1909 | Read, Notify | Last error (ResultCode) |
| RSSI | 0x190A | Read | WiFi signal strength |
| API Endpoint | 0x190B | Read, Write | API URL (max 128 chars) |
| Command | 0x190C | Write | Command byte (see below) |
| Response | 0x190D | Read, Notify | Command response |

### Command Codes (0x190C)

| Code | Command | Description |
|------|---------|-------------|
| 0x01 | WIFI_CONNECT | Attempt WiFi connection |
| 0x02 | WIFI_DISCONNECT | Disconnect WiFi |
| 0x03 | REFRESH_NOW | Force display refresh |
| 0x04 | ENTER_SLEEP | Enter deep sleep |
| 0x05 | REBOOT | Restart device |
| 0x06 | FACTORY_RESET | Clear all config |
| 0x07 | GET_STATUS | Request full status update |

### WiFi Status Enum (0x1903)

| Value | Status |
|-------|--------|
| 0x00 | NOT_CONFIGURED |
| 0x01 | DISCONNECTED |
| 0x02 | CONNECTING |
| 0x03 | CONNECTED |
| 0x04 | CONNECTION_FAILED |
| 0x05 | WRONG_PASSWORD |
| 0x06 | NO_AP_FOUND |

## Data Flow

### Initial Provisioning Flow

```
LCD Control Unit                    E-Paper Display Unit
      │                                    │
      │  1. Scan for BLE devices           │
      │────────────────────────────────────►
      │                                    │
      │  2. Find "ChurchDisplay-XX"        │
      │◄────────────────────────────────────
      │                                    │
      │  3. Connect                        │
      │────────────────────────────────────►
      │                                    │
      │  4. Read Device Name (0x1904)      │
      │◄────────────────────────────────────
      │                                    │
      │  5. Read WiFi Status (0x1903)      │
      │◄──────────── 0x00 (NOT_CONFIGURED) │
      │                                    │
      │  6. Write SSID (0x1901)            │
      │────────────────────────────────────►
      │                                    │
      │  7. Write Password (0x1902)        │
      │────────────────────────────────────►
      │                                    │
      │  8. Write Command: WIFI_CONNECT    │
      │────────────────────────────────────►
      │                                    │
      │  9. Notify: WiFi Status = CONNECTING
      │◄────────────────────────────────────
      │                                    │
      │  10. Notify: WiFi Status = CONNECTED
      │◄────────────────────────────────────
      │                                    │
      │  11. Disconnect BLE                │
      │────────────────────────────────────►
      │                                    │
      │                                    │ 12. Save config to NVS
      │                                    │ 13. Continue normal operation
```

### Status Check Flow (Device Already Configured)

```
LCD Control Unit                    E-Paper Display Unit
      │                                    │
      │  1. Connect to known device        │
      │────────────────────────────────────►
      │                                    │
      │  2. Write Command: GET_STATUS      │
      │────────────────────────────────────►
      │                                    │
      │  3. Notify: Battery (0x1905)       │
      │◄────────────────────────────────────
      │                                    │
      │  4. Notify: Last Update (0x1907)   │
      │◄────────────────────────────────────
      │                                    │
      │  5. Notify: Error Code (0x1909)    │
      │◄────────────────────────────────────
      │                                    │
      │  6. Notify: RSSI (0x190A)          │
      │◄────────────────────────────────────
      │                                    │
      │  7. Display status on LCD          │
      │                                    │
```

## File Structure

### E-Paper Display Unit (FireBeetle)

```
src/
├── main.cpp                 # Existing - add BLE check at startup
├── config.h                 # Existing - add BLE settings
├── schedule.h               # Existing
├── battery.h/cpp            # Existing
├── wifi_manager.h/cpp       # Existing
├── api_client.h/cpp         # Existing
├── display.h/cpp            # Existing
├── power_manager.h/cpp      # Existing
├── ble_provisioning.h       # NEW - BLE server interface
├── ble_provisioning.cpp     # NEW - BLE GATT server implementation
├── config_storage.h         # NEW - NVS storage interface
└── config_storage.cpp       # NEW - Save/load WiFi credentials
```

### LCD Control Unit (ESP32-S3)

```
lcd_control/
├── src/
│   ├── main.cpp             # Entry point, UI state machine
│   ├── config.h             # Pin definitions, settings
│   ├── ui_manager.h/cpp     # LVGL UI screens
│   ├── ble_scanner.h/cpp    # BLE central/client
│   ├── device_manager.h/cpp # Track known displays
│   └── screens/
│       ├── home_screen.cpp      # Main dashboard
│       ├── scan_screen.cpp      # Device discovery
│       ├── provision_screen.cpp # WiFi setup UI
│       ├── status_screen.cpp    # Device status view
│       └── settings_screen.cpp  # App settings
├── platformio.ini
└── README.md
```

## Implementation Notes

### E-Paper Unit: When to Enable BLE

BLE should only be active when needed to save power:

```cpp
void setup() {
    // Check if WiFi is configured
    if (!config_has_wifi_credentials()) {
        // No WiFi config - enter provisioning mode
        ble_start_advertising();
        // Stay awake waiting for provisioning
        return;
    }

    // Check if button was held during boot (force provisioning)
    if (button_held_on_boot()) {
        ble_start_advertising();
        return;
    }

    // Normal operation - no BLE needed
    // ... existing flow ...
}
```

### LCD Unit: Multi-Device Support

Store known devices in NVS:

```cpp
struct KnownDevice {
    char name[32];
    uint8_t mac[6];
    uint32_t lastSeen;      // Unix timestamp
    uint8_t lastBattery;    // 0-100
    uint8_t lastError;      // ResultCode
};

// Store up to 10 known displays
#define MAX_KNOWN_DEVICES 10
```

### Security Considerations

1. **BLE Pairing**: Use "Just Works" pairing for simplicity, or implement passkey if security is critical
2. **WiFi Password**: Never expose via BLE Read - only Write
3. **Advertising**: Only advertise when unconfigured or button-triggered
4. **Timeout**: Exit provisioning mode after 5 minutes if no connection

### Power Considerations

| State | Current Draw | Notes |
|-------|--------------|-------|
| BLE Advertising | ~100mA | Only during provisioning |
| BLE Connected | ~80mA | Brief during config |
| WiFi Active | ~150mA | Only during API fetch |
| Deep Sleep | ~36µA | Most of the time |

Typical provisioning session: ~30 seconds = 0.8mAh
Normal daily operation: ~30 seconds active = 1.3mAh

## UI Mockups (LCD Control Unit)

### Home Screen
```
┌────────────────────┐
│  Church Displays   │
├────────────────────┤
│                    │
│  [Display 1]  98%  │
│  Last: 2h ago  OK  │
│                    │
│  [Display 2]  45%  │
│  Last: 1d ago  !   │
│                    │
│  [+ Add Display]   │
│                    │
└────────────────────┘
```

### Provisioning Screen
```
┌────────────────────┐
│  Setup Display     │
├────────────────────┤
│                    │
│  WiFi Network:     │
│  [ChurchWiFi    ]  │
│                    │
│  Password:         │
│  [••••••••••••• ]  │
│                    │
│  [Connect]         │
│                    │
│  Status: Ready     │
└────────────────────┘
```

### Status Screen
```
┌────────────────────┐
│  Display 1         │
├────────────────────┤
│                    │
│  Battery:  98%     │
│  WiFi:     -65dBm  │
│  Last:     7:00 AM │
│  Status:   OK      │
│                    │
│  [Refresh Now]     │
│                    │
│  [Settings]        │
└────────────────────┘
```

## Dependencies

### E-Paper Unit
```ini
; platformio.ini additions
lib_deps =
    ; ... existing deps ...
    ESP32 BLE Arduino   ; or use built-in ESP-IDF BLE
```

### LCD Control Unit
```ini
; platformio.ini
[env:lcd_control]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    lvgl/lvgl @ ^8.3.0
    lovyan03/LovyanGFX @ ^1.1.8
    ESP32 BLE Arduino
```

## Next Steps

1. **Phase 1**: Implement `ble_provisioning.h/cpp` for E-Paper unit
2. **Phase 2**: Implement `config_storage.h/cpp` for NVS persistence
3. **Phase 3**: Modify `main.cpp` to check config and enter BLE mode
4. **Phase 4**: Create LCD Control Unit project structure
5. **Phase 5**: Implement BLE scanner and basic UI
6. **Phase 6**: Add multi-device management
7. **Phase 7**: Polish UI with LVGL widgets
