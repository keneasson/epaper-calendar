# Assembly Guide: Waveshare 7.5" E-Paper HAT + DFRobot FireBeetle 2 ESP32-C6

## Your Components

| Component | Description |
|-----------|-------------|
| Waveshare 7.5" E-Paper HAT | 800x480 e-paper display with driver board (40-pin Raspberry Pi header) |
| DFRobot FireBeetle 2 ESP32-C6 | Low-power IoT board (160MHz RISC-V, WiFi 6, BLE 5, 16uA deep sleep) |

## Important: Direct Plug-in is NOT Possible

The HAT has a **40-pin Raspberry Pi header** - it will NOT plug directly into the FireBeetle. You need **jumper wires** to connect them.

## What You Need

**Required:**
- 8-9x Female-to-Female jumper wires (Dupont cables)
- Soldering iron + solder (to solder headers onto both boards)

**Header pins to solder:**
- FireBeetle: Solder the male header pins pointing DOWN
- E-Paper HAT: The 40-pin header should already be present; if not, solder it

## Pin Mapping

### FireBeetle 2 ESP32-C6 SPI Pins

| Function | ESP32-C6 GPIO |
|----------|---------------|
| SCK (Clock) | GPIO 23 |
| MOSI (Data Out) | GPIO 22 |
| MISO (Data In) | GPIO 21 |
| Default CS | GPIO 1 |

### Wiring: E-Paper HAT to FireBeetle ESP32-C6

| E-Paper HAT Pin | Wire to | ESP32-C6 Pin | Notes |
|-----------------|---------|--------------|-------|
| VCC | --> | 3V3 | 3.3V power |
| GND | --> | GND | Ground |
| DIN | --> | GPIO 22 | MOSI (SPI data) |
| CLK | --> | GPIO 23 | SCK (SPI clock) |
| CS | --> | GPIO 1 | Chip Select |
| DC | --> | GPIO 2 | Data/Command (any GPIO) |
| RST | --> | GPIO 3 | Reset (any GPIO) |
| BUSY | --> | GPIO 4 | Busy signal (any GPIO) |
| PWR | --> | 3V3 | **Important for Rev 2.3+** |

## Assembly Steps

### Step 1: Solder Headers

**FireBeetle ESP32-C6:**
1. Insert male header pins into the holes
2. Pins should point **downward** (away from components)
3. Solder from the top side

**E-Paper HAT:**
- Should already have the 40-pin header
- If separate header pins came with it, solder female header on the underside

### Step 2: Locate E-Paper HAT Pins

The E-Paper HAT uses a Raspberry Pi 40-pin header layout. The pins you need are:

```
E-Paper HAT 40-Pin Header (top view, USB port at bottom)
         ┌─────────────────────────────────┐
   3V3   │ (1)  (2) │ 5V                   │
   SDA   │ (3)  (4) │ 5V                   │
   SCL   │ (5)  (6) │ GND  <-- Use this    │
         │ (7)  (8) │ TXD                  │
   GND   │ (9) (10) │ RXD                  │
   RST   │(11) (12) │ PWR  <-- Use this    │
         │(13) (14) │ GND                  │
         │(15) (16) │                      │
   3V3   │(17) (18) │ BUSY <-- Use this    │
   DIN   │(19) (20) │ GND                  │
         │(21) (22) │ DC   <-- Use this    │
   CLK   │(23) (24) │ CS   <-- Use this    │
   GND   │(25) (26) │                      │
         │    ...   │                      │
         └─────────────────────────────────┘

Pins to use:
- Pin 1 or 17: VCC (3.3V)
- Pin 6: GND
- Pin 11: RST
- Pin 12: PWR
- Pin 18: BUSY
- Pin 19: DIN (MOSI)
- Pin 22: DC
- Pin 23: CLK
- Pin 24: CS
```

### Step 3: Connect Jumper Wires

Use female-to-female jumper wires:

```
E-Paper HAT (40-pin)          FireBeetle ESP32-C6
    ┌─────────────┐               ┌───────────┐
    │ VCC (Pin 1) │───────────────│ 3V3       │
    │ GND (Pin 6) │───────────────│ GND       │
    │ DIN (Pin 19)│───────────────│ GPIO 22   │
    │ CLK (Pin 23)│───────────────│ GPIO 23   │
    │ CS  (Pin 24)│───────────────│ GPIO 1    │
    │ DC  (Pin 22)│───────────────│ GPIO 2    │
    │ RST (Pin 11)│───────────────│ GPIO 3    │
    │ BUSY(Pin 18)│───────────────│ GPIO 4    │
    │ PWR (Pin 12)│───────────────│ 3V3       │
    └─────────────┘               └───────────┘
```

### Step 4: Connect E-Paper Display

1. Gently insert the e-paper ribbon cable into the HAT's FPC connector
2. Lift the black latch, insert cable (gold contacts facing down typically), press latch down
3. **Handle carefully** - the display is fragile!

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

1. **PWR Pin (Rev 2.3+):** Must be connected to VCC or the display won't power on
2. **BUSY Pin:** On some V2 models, may need to be inverted in software
3. **Version Check:** Displays sold after Sept 2023 use different initialization - check which version you have
4. **Power:** The FireBeetle can be powered via USB-C, solar, or LiPo battery
5. **Deep Sleep:** ESP32-C6 draws only 16uA in deep sleep - perfect for battery operation

## Verify Before Powering On

- [ ] All 9 wires connected (VCC, GND, DIN, CLK, CS, DC, RST, BUSY, PWR)
- [ ] No shorts between adjacent pins
- [ ] E-paper ribbon cable fully seated
- [ ] Headers soldered securely

## Troubleshooting

### Display stays blank
- Check PWR pin is connected to 3V3
- Verify ribbon cable is fully inserted
- Check all SPI connections

### Display shows garbage or partial update
- Verify you're using the correct display version in code (V2 vs V2_old)
- Check BUSY pin connection and polarity setting

### ESP32 crashes or resets
- Check for shorts between pins
- Verify 3.3V power is stable
- Add decoupling capacitor if needed

## References

- [DFRobot FireBeetle 2 ESP32-C6 Wiki](https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6)
- [Waveshare 7.5inch e-Paper HAT Manual](https://www.waveshare.com/wiki/7.5inch_e-Paper_HAT_Manual)
- [GxEPD2 Library & Hardware Guide](https://github.com/ZinggJM/GxEPD2)
- [E-Paper Driver HAT Wiki](https://www.waveshare.com/wiki/E-Paper_Driver_HAT)
