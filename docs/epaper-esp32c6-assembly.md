# Assembly Guide: GooDisplay 7.5" E-Paper + DESPI-C02 + FireBeetle 2 ESP32-C6

## Your Components

| Component | Description |
|-----------|-------------|
| GooDisplay 7.5" E-Paper Display | 800x480 e-paper panel with 24-pin FPC ribbon cable |
| GooDisplay DESPI-C02 Adapter | Universal e-paper adapter board with 8-pin header |
| DFRobot FireBeetle 2 ESP32-C6 | Low-power IoT board (160MHz RISC-V, WiFi 6, BLE 5, 16uA deep sleep) |
| LiPo Battery | 3.7V lithium polymer battery (optional for portable use) |

## What You Need

**Required:**
- 8x Female-to-Female jumper wires (Dupont cables)
- Soldering iron + solder (to solder headers onto FireBeetle)

**Header pins to solder:**
- FireBeetle: Solder the male header pins pointing DOWN (away from components)
- DESPI-C02: Has 8-pin header already installed

## Pin Mapping

### DESPI-C02 Pin Functions

| Pin | Function |
|-----|----------|
| BUSY | Busy status output pin |
| RES | External reset pin (low = reset) |
| D/C | Data/Command control (high = data, low = command) |
| CS | SPI chip select (active low) |
| SCK | SPI clock signal |
| SDI | SPI data input (MOSI) |
| GND | Ground |
| 3.3V | Power supply |

### FireBeetle 2 ESP32-C6 SPI Pins

| Function | ESP32-C6 GPIO |
|----------|---------------|
| SCK (Clock) | GPIO 23 |
| MOSI (Data Out) | GPIO 22 |
| MISO (Data In) | GPIO 21 |
| Default CS | GPIO 1 |

### Wiring: DESPI-C02 to FireBeetle ESP32-C6

| DESPI-C02 Pin | Wire to | ESP32-C6 Pin | Notes |
|---------------|---------|--------------|-------|
| 3.3V | --> | 3V3 | Power |
| GND | --> | GND | Ground |
| SDI | --> | GPIO 22 | MOSI (SPI data) |
| SCK | --> | GPIO 23 | SPI clock |
| CS | --> | GPIO 1 | Chip Select |
| D/C | --> | GPIO 2 | Data/Command |
| RES | --> | GPIO 3 | Reset |
| BUSY | --> | GPIO 4 | Busy signal |

## Assembly Steps

### Step 1: Solder Headers onto FireBeetle

1. Insert male header pins into the FireBeetle holes
2. Pins should point **downward** (away from components)
3. Solder from the top side
4. Ensure good solder joints on all pins

### Step 2: Connect E-Paper Display to DESPI-C02

**Important: The FPC connector is fragile!**

1. Locate the FPC connector on the DESPI-C02 (24-pin slot)
2. **Gently lift** the black locking tab - it only tilts ~2-3mm, do NOT force it
3. Slide the ribbon cable in straight (zero force required)
4. Gold contacts typically face **UP** away from the PCB
5. Push the locking tab back down until it clicks
6. The "0.47" marking on the board refers to the RESE resistor option (for voltage/current settings), move the switch to the 2.7

### Step 3: Connect Jumper Wires

Use female-to-female jumper wires:

```text
DESPI-C02                    FireBeetle ESP32-C6
┌─────────────┐               ┌───────────────┐
│ 3.3V        │───────────────│ 3V3           │
│ GND         │───────────────│ GND           │
│ SDI         │───────────────│ GPIO 22       │
│ SCK         │───────────────│ GPIO 23       │
│ CS          │───────────────│ GPIO 1        │
│ D/C         │───────────────│ GPIO 2        │
│ RES         │───────────────│ GPIO 3        │
│ BUSY        │───────────────│ GPIO 4        │
└─────────────┘               └───────────────┘
```

### Step 4: Connect Battery (Optional)

1. Plug the JST connector into the FireBeetle's battery port
2. **Verify polarity matches** - red wire to (+), black to (-)
3. If polarity is reversed, swap the pins in the JST connector

## Software Configuration

For your `apps/epaper-calendar` project, create or update the pin definitions:

```cpp
// ESP32-C6 Pin Definitions for Waveshare 7.5" E-Paper
#define EPD_CS    1
#define EPD_DC    2
#define EPD_RST   3
#define EPD_BUSY  4
#define EPD_MOSI  22
#define EPD_SCK   23
```

### PlatformIO Configuration

Update `platformio.ini` for ESP32-C6:

```ini
[env:firebeetle_c6]
platform = espressif32
board = dfrobot_firebeetle2_esp32c6
framework = arduino
monitor_speed = 115200

lib_deps =
    zinggjm/GxEPD2@^1.5.0
    adafruit/Adafruit GFX Library@^1.11.0
    bblanchon/ArduinoJson@^6.21.0
```

## Important Notes

1. **BUSY Pin:** On some display versions, may need to be inverted in software
2. **Version Check:** Displays sold after Sept 2023 use different initialization
3. **Power:** FireBeetle can be powered via USB-C, solar, or LiPo battery
4. **Deep Sleep:** ESP32-C6 draws only 16uA in deep sleep - perfect for battery operation

## Verify Before Powering On

- [ ] All 8 wires connected (3.3V, GND, SDI, SCK, CS, D/C, RES, BUSY)
- [ ] No shorts between adjacent pins
- [ ] E-paper ribbon cable fully seated in DESPI-C02
- [ ] Headers soldered securely
- [ ] Battery polarity correct (if using battery)

## Troubleshooting

### Display stays blank

- Verify ribbon cable is fully inserted and locked
- Check all SPI connections with multimeter
- Verify 3.3V power is reaching the DESPI-C02

### Display shows garbage or partial update

- Verify you're using the correct display version in code (V2 vs V2_old)
- Check BUSY pin connection and polarity setting in software

### ESP32 crashes or resets

- Check for shorts between pins
- Verify 3.3V power is stable
- Try powering from USB-C without battery first

### Cannot upload - device stuck in deep sleep/hibernate

If firmware goes into deep sleep immediately (e.g., due to low battery check), the USB disconnects and normal upload fails. To recover:

**Method 1: Timed USB plug-in (most reliable)**
1. Disconnect USB from the FireBeetle
2. Run the upload command: `pio run -t upload`
3. Wait for "Looking for upload port..." message
4. Immediately plug in USB
5. The upload should catch the device during enumeration

**Method 2: Bootloader mode**
1. Hold both BOOT and RESET buttons
2. Release RESET (keep holding BOOT)
3. Wait 1 second, then release BOOT
4. Quickly run upload command

**Prevention:** Always include a startup delay in firmware before any deep sleep decisions:
```cpp
void setup() {
    delay(10000);  // 10-second window for firmware updates
    // ... rest of setup
}
```

### DESPI-C02 switch position

The switch on the DESPI-C02 board sets the series resistor value:
- **2.2** position: For 7.5" and larger displays (correct for this setup)
- **0.47** position: For smaller displays (1.54" - 4.2")

## References

- [DFRobot FireBeetle 2 ESP32-C6 Wiki](https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6)
- [Waveshare 7.5inch e-Paper HAT Manual](https://www.waveshare.com/wiki/7.5inch_e-Paper_HAT_Manual)
- [GxEPD2 Library & Hardware Guide](https://github.com/ZinggJM/GxEPD2)
- [GooDisplay DESPI-C02 on AliExpress](https://www.aliexpress.com/item/1005004633084221.html)
- [GooDisplay Website](https://www.good-display.com/)
