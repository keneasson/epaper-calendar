# Church Schedule E-Ink Display Project

A low-power, WiFi-enabled electronic schedule display for your church bulletin board using a 7.5" e-paper display and ESP32 microcontroller.

## Project Overview

This project creates a battery-powered e-ink display that:
- Connects to your church WiFi
- Fetches schedule data from your website API
- Updates the display automatically before services
- Runs for 6+ months on a single battery charge
- Can be recharged via USB-C without removing from the wall

## Features

- **Ultra-Low Power:** ~14μA in sleep mode, 6+ months battery life
- **WiFi Enabled:** Automatically fetches latest schedule from your API
- **Paper-Like Display:** 7.5" e-ink screen (800×480 resolution)
- **Scheduled Updates:** Wake up automatically before services to update
- **Battery Monitoring:** Track battery level and charging status
- **USB-C Charging:** Easy recharging without disassembly

## Hardware Components

See `parts_list.xlsx` for complete shopping list with links and prices.

### Core Components:
1. **DFRobot FireBeetle 2 ESP32-E** - Ultra-low power ESP32 board with built-in battery management
2. **Waveshare 7.5" E-Paper Display** - 800×480 black/white e-ink screen
3. **DESPI-C02 Adapter** - Connects display to ESP32
4. **3.7V 5000mAh LiPo Battery** - JST-PH2.0 connector (6+ month runtime)
5. **Enclosure** - 3D printed or Waveshare case

**Total Cost:** ~$95-110 USD

## Software Requirements

### Arduino IDE Setup:
1. Install Arduino IDE 2.0 or later
2. Add ESP32 board support:
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search for "esp32" and install "esp32 by Espressif Systems"

### Required Libraries:
Install via Arduino Library Manager (Tools → Manage Libraries):
- `GxEPD2` - E-paper display driver (by Jean-Marc Zingg)
- `Adafruit GFX Library` - Graphics functions
- `ArduinoJson` - JSON parsing (version 6.x)
- `WiFi` - Built-in ESP32 WiFi (no install needed)
- `HTTPClient` - HTTP requests (no install needed)

## Wiring

See `wiring_guide.md` for detailed connection diagrams.

### Quick Reference:
**E-Paper Display → DESPI-C02 Adapter → FireBeetle ESP32**

The DESPI-C02 adapter simplifies wiring - just plug the display's ribbon cable into the adapter, then connect adapter pins to ESP32:

| DESPI-C02 Pin | ESP32 Pin | Function |
|---------------|-----------|----------|
| VCC | 3V3 | Power |
| GND | GND | Ground |
| DIN (MOSI) | GPIO 23 (MOSI) | Data |
| CLK (SCK) | GPIO 18 (SCK) | Clock |
| CS | GPIO 5 | Chip Select |
| DC | GPIO 17 | Data/Command |
| RST | GPIO 16 | Reset |
| BUSY | GPIO 4 | Busy Signal |

⚠️ **Important:** Set the DESPI-C02 switch to position "0.47" for RESE setting.

## Installation Steps

### 1. Hardware Assembly
1. Connect battery to FireBeetle JST connector (verify polarity!)
2. Wire DESPI-C02 adapter to ESP32 following wiring guide
3. Connect e-paper display ribbon cable to adapter
4. Mount in enclosure (optional for testing)

### 2. Software Configuration
1. Open `church_schedule_display.ino` in Arduino IDE
2. Edit the configuration section:
   ```cpp
   // WiFi Configuration
   const char* ssid = "YOUR_CHURCH_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";
   
   // API Configuration
   const char* apiEndpoint = "https://yourchurch.org/api/schedule";
   
   // Update Schedule (24-hour format)
   const int wakeHour = 7;      // Wake at 7:00 AM
   const int wakeMinute = 0;
   ```

3. Select board:
   - Tools → Board → ESP32 Arduino → FireBeetle-ESP32

4. Select port:
   - Tools → Port → (select your USB port)

5. Upload the sketch

### 3. First-Time Setup
1. After uploading, open Serial Monitor (115200 baud)
2. Watch the boot sequence - it will:
   - Connect to WiFi
   - Fetch schedule from your API
   - Display on e-paper
   - Show battery voltage
   - Enter deep sleep

3. Verify display updates correctly
4. Check battery voltage is within 3.7V-4.2V range

## API Format

Your API endpoint should return JSON in this format:

```json
{
  "service_date": "Sunday, December 1, 2024",
  "service_time": "11:00 AM",
  "events": [
    {
      "time": "9:30 AM",
      "title": "Sunday School",
      "location": "Fellowship Hall"
    },
    {
      "time": "11:00 AM",
      "title": "Worship Service",
      "location": "Main Sanctuary",
      "speaker": "Pastor John Smith"
    },
    {
      "time": "6:00 PM",
      "title": "Evening Prayer",
      "location": "Chapel"
    }
  ]
}
```

The sketch will parse this JSON and format it nicely on the e-ink display.

## Power Management

### Battery Life:
- **Deep Sleep Current:** ~10-14μA
- **Active Refresh:** ~83mA for ~15 seconds
- **Daily Update:** ~0.35mAh per update
- **5000mAh Battery Life:** 6-12 months with daily updates

### Charging:
1. Remove display from wall (or use rear USB access if enclosure allows)
2. Plug USB-C cable into FireBeetle
3. Red LED indicates charging
4. Charging complete when LED turns off (~3-4 hours for full charge)

### Battery Monitoring:
The sketch monitors battery voltage and will:
- Show battery level on display during updates
- Warn when battery is low (<3.3V)
- Protect battery by entering hibernate mode if critically low (<3.0V)

## Customization

### Display Layout:
Edit the `displaySchedule()` function in the sketch to customize:
- Font sizes
- Text positioning
- Header/footer information
- Logo or church name display

### Update Schedule:
Modify the `wakeHour` and `wakeMinute` variables to change when the display updates. Can also add multiple wake times for mid-week services.

### Display Rotation:
Change the rotation in `setup()`:
```cpp
display.setRotation(0);  // 0, 1, 2, or 3 for different orientations
```

## Troubleshooting

### Display Not Updating:
- Check all wiring connections
- Verify DESPI-C02 switch is set to 0.47
- Check Serial Monitor for error messages
- Ensure battery voltage is adequate (>3.5V)

### WiFi Connection Issues:
- Verify SSID and password are correct
- Check that church WiFi allows IoT devices
- Ensure display is within WiFi range
- Try using WiFi.setAutoReconnect(true)

### API Fetch Failures:
- Test API URL in web browser first
- Check JSON format matches expected structure
- Verify church network allows outbound HTTPS
- Increase WiFi timeout if needed

### Battery Draining Fast:
- Verify you've cut the low-power pad on FireBeetle (see board documentation)
- Check for WiFi connection loops in Serial Monitor
- Ensure deep sleep is actually being entered
- Verify RGB LED is turned off in code

### Display Shows Ghosting:
- Run the display refresh/clear routine
- Avoid partial refreshes
- Full refresh every time is recommended for e-paper longevity

## Enclosure Options

### 3D Printed Cases (Free):
- **Thingiverse:** Search "Waveshare 7.5 e-paper case"
- **Printables:** "WaveShare 7.5" e-Paper case - symmetrical borders"
- **MakerWorld:** "Waveshare 7.5 inch e-paper case"

### Commercial Case:
- Waveshare 7.5inch e-Paper Protection Case (~$15)
- Clean professional look
- Easy mounting holes

### DIY Picture Frame:
- Standard 8×10" or 9×12" picture frame
- Remove glass
- Mount display with foam spacers
- Access battery through back

## Maintenance

### Weekly:
- Check display is updating correctly
- Verify schedule information is accurate

### Monthly:
- Check battery voltage via Serial Monitor
- Clean display with soft, dry cloth if needed

### Every 3-6 Months:
- Recharge battery (even if not low)
- Verify WiFi credentials haven't changed
- Update firmware if needed

## Support & Resources

### Documentation:
- FireBeetle 2 ESP32-E: https://www.dfrobot.com/product-1590.html
- Waveshare E-Paper: https://www.waveshare.com/wiki/7.5inch_e-Paper
- GxEPD2 Library: https://github.com/ZinggJM/GxEPD2

### Community Projects:
- ESP32 Weather E-Paper: https://github.com/lmarzen/esp32-weather-epd
- E-Paper Examples: https://github.com/Xinyuan-LilyGO/LilyGo-T5-Epaper-Series

### Getting Help:
1. Check Serial Monitor output for error messages
2. Review troubleshooting section above
3. Search Arduino forums for similar e-paper projects
4. Post questions with error details and photos if needed

## Future Enhancements

Possible additions to the project:
- Add manual refresh button
- Include weather forecast from API
- Show announcements or special events
- Add church logo or custom graphics
- Multiple displays for different locations
- OTA (Over-The-Air) firmware updates
- QR code for online schedule/giving

## License

This project is open source. Feel free to modify and adapt for your church's needs.

## Credits

Based on the esp32-weather-epd project by lmarzen and examples from the GxEPD2 library by Jean-Marc Zingg.

---

**Project Version:** 1.0  
**Last Updated:** November 2024  
**Tested On:** FireBeetle 2 ESP32-E + Waveshare 7.5" E-Paper
