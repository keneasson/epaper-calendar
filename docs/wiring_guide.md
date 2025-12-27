# Church Schedule Display - Wiring Guide

## Overview

This guide provides detailed wiring instructions for connecting the Waveshare 7.5" E-Paper display to the FireBeetle ESP32-E using the DESPI-C02 adapter board.

## Safety First!

⚠️ **CRITICAL - Battery Polarity:**
- JST connector polarity is NOT standardized
- ALWAYS verify with multimeter before connecting:
  - Red wire = Positive (+)
  - Black wire = Negative (-)
- Wrong polarity WILL destroy your ESP32 board
- Check voltage should read ~3.7V-4.2V on new battery

## Components Overview

### FireBeetle 2 ESP32-E Pinout Reference
```
                    USB-C Port
                        |
    ┌───────────────────┴────────────────────┐
    │  ┌──┐                          ┌──┐   │
    │  │  │      FireBeetle 2        │  │   │
    │  │  │       ESP32-E            │  │   │
    │  └──┘                          └──┘   │
    │                                        │
    │  3V3  GND  IO23 IO22 IO19 IO18 IO17   │ Right Side
    │  (Power Pins and SPI)                  │
    │                                        │
    │  IO16 IO5 IO4 IO2 IO15 IO14 IO12 IO13 │ Left Side
    │  (GPIO Pins)                           │
    │                                        │
    │          [JST Battery Connector]       │
    └────────────────────────────────────────┘
```

### DESPI-C02 Adapter Pinout
```
    ┌─────────────────────────────┐
    │  DESPI-C02 Adapter Board    │
    │                             │
    │  [24-pin Display Connector] │  ← E-Paper ribbon cable plugs here
    │                             │
    │  RESE Switch: [0.47 | 1.0]  │  ← MUST be set to 0.47
    │                             │
    │  Pin Headers:               │
    │  VCC  GND  DIN  CLK  CS     │
    │  DC   RST  BUSY             │
    └─────────────────────────────┘
```

## Pin Connections

### Required Connections

| DESPI-C02 Pin | ESP32 Pin    | Pin Name      | Function          |
|---------------|--------------|---------------|-------------------|
| VCC           | 3V3          | 3.3V Power    | Power Supply      |
| GND           | GND          | Ground        | Ground            |
| DIN           | GPIO 23      | MOSI          | SPI Data Out      |
| CLK           | GPIO 18      | SCK           | SPI Clock         |
| CS            | GPIO 5       | CS            | Chip Select       |
| DC            | GPIO 17      | DC            | Data/Command      |
| RST           | GPIO 16      | RST           | Reset             |
| BUSY          | GPIO 4       | BUSY          | Busy Signal       |

### Refresh Button (Optional but Recommended)

| Button Pin    | ESP32 Pin    | Function                        |
|---------------|--------------|----------------------------------|
| Pin 1         | GPIO 27      | Button signal (uses internal pull-up) |
| Pin 2         | GND          | Ground                          |

### Physical Connection Layout

```
FireBeetle ESP32-E                    DESPI-C02 Adapter
┌──────────────┐                     ┌──────────────┐
│              │                     │              │
│   3V3 ●------│---------------------│----● VCC     │
│   GND ●------│---------------------│----● GND     │
│ IO 23 ●------│---------------------│----● DIN     │
│ IO 18 ●------│---------------------│----● CLK     │
│  IO 5 ●------│---------------------│----● CS      │
│ IO 17 ●------│---------------------│----● DC      │
│ IO 16 ●------│---------------------│----● RST     │
│  IO 4 ●------│---------------------│----● BUSY    │
│              │                     │              │
│ [JST]◄───────○ Battery             │  [Display]   │
└──────────────┘                     └──────┬───────┘
                                            │
                                      ┌─────▼─────┐
                                      │  E-Paper  │
                                      │  Display  │
                                      └───────────┘
```

## Step-by-Step Assembly

### Step 1: Prepare Components

1. **Unpack all components carefully**
   - E-Paper display is fragile
   - Handle by edges only
   - Keep protective film on until final assembly

2. **Identify all pins on FireBeetle**
   - Locate GPIO pins on both sides
   - Find 3V3 and GND pins
   - Locate JST battery connector

3. **Set DESPI-C02 Switch**
   - **CRITICAL:** Switch must be at position 0.47
   - This sets the RESE (Reset) configuration
   - Wrong setting = display won't work

### Step 2: Battery Verification (DO THIS FIRST!)

1. **Check battery polarity with multimeter:**
   ```
   Multimeter Settings: DC Voltage, 20V range
   
   Red Probe → Red wire of battery
   Black Probe → Black wire of battery
   
   Reading should be: +3.7V to +4.2V
   
   ⚠️ If reading is NEGATIVE, wires are reversed!
   ```

2. **If polarity is wrong:**
   - Do NOT connect to ESP32
   - Carefully remove wires from JST connector
   - Swap wire positions in connector
   - Re-verify with multimeter

### Step 3: Wiring Method Options

Choose ONE of these methods:

#### Option A: Breadboard Prototyping (Recommended for Testing)

**Advantages:**
- No soldering required
- Easy to troubleshoot
- Can move wires if needed

**Materials Needed:**
- Half-size breadboard
- Male-to-Female jumper wires (8 wires)
- Male-to-Male jumper wires (if needed)

**Instructions:**
1. Insert FireBeetle into breadboard
2. Insert DESPI-C02 adapter pins into breadboard
3. Connect with jumper wires following pin table above

#### Option B: Direct Soldering (Permanent Installation)

**Advantages:**
- Most reliable connections
- Compact installation
- Best for final enclosure

**Materials Needed:**
- Soldering iron
- Lead-free solder
- Wire stripper
- Heat shrink tubing
- 22 AWG stranded wire

**Instructions:**
1. Cut 8 wires to appropriate lengths (10-15cm each)
2. Strip 5mm from each end
3. Tin both wire ends with solder
4. Solder wires to DESPI-C02 adapter pins
5. Solder wires to FireBeetle GPIO pins
6. Apply heat shrink to each connection
7. Test continuity with multimeter

#### Option C: Female Headers (Compromise Solution)

**Advantages:**
- Removable connections
- No breadboard needed
- Relatively permanent

**Materials Needed:**
- Female header pins (solder to DESPI-C02)
- Male-to-Female jumper wires

**Instructions:**
1. Solder female headers to DESPI-C02 adapter
2. Connect jumper wires from headers to ESP32

### Step 4: Making Connections

**Connection Order (Important!):**

1. **First: Connect Ground**
   ```
   GND pin (ESP32) → GND pin (DESPI-C02)
   ```
   This establishes common ground reference

2. **Second: Connect Power**
   ```
   3V3 pin (ESP32) → VCC pin (DESPI-C02)
   ```
   
3. **Third: Connect SPI Signals**
   ```
   GPIO 23 → DIN
   GPIO 18 → CLK
   GPIO 5  → CS
   ```

4. **Fourth: Connect Control Signals**
   ```
   GPIO 17 → DC
   GPIO 16 → RST
   GPIO 4  → BUSY
   ```

5. **Finally: Connect Display Ribbon Cable**
   - Gently insert ribbon cable into DESPI-C02
   - Ensure connector is fully seated
   - Lock connector (if it has locking mechanism)

### Step 5: Battery Connection

⚠️ **Only after verifying polarity!**

1. **Plug battery into JST connector on FireBeetle**
   - Connector only fits one way
   - Don't force it
   - Should click gently into place

2. **Board should power on**
   - You may see a brief LED flash
   - Board is now powered

3. **Measure battery voltage at ESP32**
   - Multimeter between BAT and GND pins
   - Should read 3.7V-4.2V

### Step 6: Refresh Button (Optional)

The refresh button allows manual triggering of a schedule update, useful for:
- WiFi failures during automatic updates
- Last-minute schedule changes
- Testing and debugging

**Components Needed:**
- 1x Momentary push button (normally open, any size)
- 2x Short wires (if not using breadboard)

**Wiring Diagram:**
```
                    ┌─────────┐
   GPIO 27 ────────┤  Button ├──────── GND
                    └─────────┘

   (Internal pull-up resistor is used, no external resistor needed)
```

**Button Behavior:**
- **During sleep:** Button press wakes the device and triggers immediate refresh
- **During operation:** Button press is ignored (update already in progress)

**Installation Options:**

1. **Panel-mount button** (recommended for enclosures)
   - Drill hole in enclosure
   - Mount button with included nut/washer
   - Solder wires to button terminals
   - Connect to GPIO 27 and GND

2. **PCB tactile button** (compact option)
   - Solder directly to a small perfboard
   - Wire to FireBeetle

3. **Breadboard button** (testing only)
   - Insert button into breadboard
   - Connect with jumper wires

## Wiring Verification Checklist

Before powering on, verify:

- [ ] DESPI-C02 switch is set to 0.47
- [ ] All 8 display signal wires are connected correctly
- [ ] Refresh button connected to GPIO 27 and GND (if installed)
- [ ] No loose connections
- [ ] No short circuits (wires touching)
- [ ] Battery polarity is correct
- [ ] Display ribbon cable is firmly seated
- [ ] Battery is charged (>3.5V)

## Testing Connections

### Visual Inspection
1. Check all solder joints (if soldered)
2. Verify no exposed wire is touching other connections
3. Ensure display cable is not kinked or damaged

### Continuity Testing (Before Power On)
```
Use multimeter in continuity mode (beep):

Test each connection:
ESP32 GPIO 23 ↔ DESPI-C02 DIN  (should beep)
ESP32 GPIO 18 ↔ DESPI-C02 CLK  (should beep)
ESP32 GPIO 5  ↔ DESPI-C02 CS   (should beep)
ESP32 GPIO 17 ↔ DESPI-C02 DC   (should beep)
ESP32 GPIO 16 ↔ DESPI-C02 RST  (should beep)
ESP32 GPIO 4  ↔ DESPI-C02 BUSY (should beep)
ESP32 3V3     ↔ DESPI-C02 VCC  (should beep)
ESP32 GND     ↔ DESPI-C02 GND  (should beep)
```

### First Power-On Test
1. Connect USB-C to computer
2. Open Arduino Serial Monitor (115200 baud)
3. You should see boot messages
4. Upload a simple blink sketch first
5. If LED blinks, ESP32 is working
6. Then upload the display test code

## Troubleshooting

### Display Doesn't Update

**Check:**
- DESPI-C02 switch position (should be 0.47)
- Display ribbon cable is fully inserted
- All 8 signal wires are connected
- Power supply is adequate (check battery voltage)

**Test:**
```cpp
// Add this to setup() to verify SPI is working:
SPI.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, SS
Serial.println("SPI initialized");
```

### ESP32 Won't Boot

**Check:**
- Battery is charged (>3.5V)
- Battery polarity is correct
- JST connector is fully inserted
- USB cable is good (try different cable)

### Display Shows Only Part of Image

**Check:**
- Increase SPI clock speed in code
- Verify all ground connections
- Ensure BUSY pin is connected (tells ESP32 when display is ready)

### Ghosting or Image Artifacts

**Solution:**
- Always do full screen refresh (not partial)
- Clear screen between updates
- Use the display's built-in clear function

### Battery Drains Too Fast

**Check:**
- Verify deep sleep is working (use Serial.println before sleep)
- Cut the low-power pad on FireBeetle (see board docs)
- Check for WiFi connection loops in Serial Monitor
- Verify RGB LED is turned off in code

## Alternative Wiring for Different Pins

If you need to use different GPIO pins, here are alternatives:

| Function | Default | Alternatives     | Notes                    |
|----------|---------|------------------|--------------------------|
| MOSI     | GPIO 23 | GPIO 13          | SPI MOSI pin             |
| SCK      | GPIO 18 | GPIO 14          | SPI Clock pin            |
| CS       | GPIO 5  | Any GPIO         | Can be any available pin |
| DC       | GPIO 17 | Any GPIO         | Can be any available pin |
| RST      | GPIO 16 | Any GPIO         | Can be any available pin |
| BUSY     | GPIO 4  | Any GPIO         | Can be any available pin |

**To change pins:** Modify these defines in the Arduino sketch:
```cpp
#define EPD_CS     5   // Change this
#define EPD_DC     17  // Change this
#define EPD_RST    16  // Change this
#define EPD_BUSY   4   // Change this
```

## Cable Management Tips

1. **Keep wires organized:**
   - Bundle similar wires together
   - Use zip ties or twist ties
   - Label wires if needed

2. **Strain relief:**
   - Don't pull on display ribbon cable
   - Secure wires near connections
   - Use hot glue for permanent installations

3. **Enclosure routing:**
   - Plan wire paths before closing case
   - Leave access to USB port for charging
   - Consider rear-access USB extension cable

## Photos and Diagrams

*Note: For detailed photos of the assembly, see the GitHub repository or check community projects:*

- ESP32 Weather EPD: https://github.com/lmarzen/esp32-weather-epd
- Waveshare Wiki: https://www.waveshare.com/wiki/E-Paper_ESP32_Driver_Board

## Final Assembly Checklist

Before closing the enclosure:

- [ ] All connections are secure
- [ ] No loose wires
- [ ] Display updates correctly
- [ ] Battery charges via USB-C
- [ ] Serial Monitor shows successful API fetch
- [ ] Display shows schedule correctly
- [ ] Deep sleep activates after update
- [ ] Battery voltage is good (3.7V-4.2V)
- [ ] No error messages in Serial Monitor

## Getting Help

If you encounter issues:

1. **Check Serial Monitor output** - Most errors will show here
2. **Verify all connections** with multimeter continuity test
3. **Test components individually:**
   - ESP32 alone (blink test)
   - Display with simple test sketch
   - Battery charging circuit

4. **Common mistakes:**
   - Wrong DESPI-C02 switch position
   - Battery polarity reversed
   - Loose ribbon cable connection
   - Wrong GPIO pin numbers in code

---

**Document Version:** 1.0  
**Last Updated:** November 2024
