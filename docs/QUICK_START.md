# Quick Start Guide
## Church Schedule E-Ink Display

Get your display up and running in under 30 minutes!

---

## 📦 Step 1: Order Parts (10 minutes)

Open `parts_list.xlsx` and order:

**MINIMUM REQUIRED:**
1. FireBeetle 2 ESP32-E (~$18)
2. Waveshare 7.5" E-Paper (~$45)
3. DESPI-C02 Adapter (~$8)
4. 5000mAh LiPo Battery (~$18) ⚠️ **VERIFY POLARITY!**
5. Jumper wires (~$5)

**Total:** ~$94

**RECOMMENDED ADD:**
- Enclosure (~$15 or 3D print free)

---

## 💻 Step 2: Install Software (5 minutes)

### Arduino IDE:
1. Download Arduino IDE 2.0: https://www.arduino.cc/en/software
2. Install it

### Add ESP32 Support:
1. File → Preferences
2. Paste this in "Additional Board Manager URLs":
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager
4. Search "esp32" → Install "esp32 by Espressif"

### Install Libraries:
Tools → Manage Libraries, search and install:
- `GxEPD2` by Jean-Marc Zingg
- `Adafruit GFX Library`
- `ArduinoJson` (version 6.x)

---

## 🔌 Step 3: Assemble Hardware (10 minutes)

### Battery Safety Check:
```
⚠️ CRITICAL: Check battery polarity first!
1. Set multimeter to DC voltage
2. Red probe → Red wire
3. Black probe → Black wire
4. Should read: +3.7V to +4.2V
5. If NEGATIVE, DO NOT CONNECT! Swap wires in JST connector.
```

### Wiring:
Connect DESPI-C02 → FireBeetle ESP32:

| Adapter | ESP32 Pin |
|---------|-----------|
| VCC     | 3V3       |
| GND     | GND       |
| DIN     | GPIO 23   |
| CLK     | GPIO 18   |
| CS      | GPIO 5    |
| DC      | GPIO 17   |
| RST     | GPIO 16   |
| BUSY    | GPIO 4    |

**⚠️ Set DESPI-C02 switch to 0.47!**

Connect display ribbon cable to DESPI-C02.

(See `wiring_guide.md` for detailed diagrams)

---

## ⚙️ Step 4: Configure Software (3 minutes)

1. Open `config_template.h`
2. Fill in:
   ```cpp
   const char* WIFI_SSID = "YourChurchWiFi";
   const char* WIFI_PASSWORD = "YourPassword";
   const char* API_ENDPOINT = "https://yourchurch.org/api/schedule";
   const int WAKE_HOUR = 7;  // Update at 7 AM
   ```
3. Save as `config.h` (remove "_template")
4. Put `config.h` in same folder as `.ino` file

---

## 📤 Step 5: Upload Code (2 minutes)

1. Plug FireBeetle into computer via USB-C
2. Open `church_schedule_display.ino` in Arduino IDE
3. Select: Tools → Board → FireBeetle-ESP32
4. Select: Tools → Port → (your USB port)
5. Click Upload button (→)
6. Wait for "Done uploading"

---

## 🧪 Step 6: Test (5 minutes)

1. Open Serial Monitor (115200 baud)
2. Press RST button on FireBeetle
3. Watch the output:
   ```
   Church Schedule Display Starting
   Battery Voltage: 4.1V (95%)
   Connecting to WiFi...
   ✓ WiFi Connected!
   Fetching schedule from API...
   ✓ API Response received
   Updating display...
   Display update complete
   Entering deep sleep...
   ```

4. Display should show your schedule!

---

## ✅ Success Checklist

- [ ] All parts received
- [ ] Battery polarity verified (⚠️ CRITICAL!)
- [ ] Arduino IDE + libraries installed
- [ ] Wiring completed correctly
- [ ] DESPI-C02 switch set to 0.47
- [ ] config.h created with your info
- [ ] Code uploaded successfully
- [ ] Serial Monitor shows "✓ WiFi Connected"
- [ ] Display updates with schedule
- [ ] Battery charges via USB-C

---

## 🐛 Quick Troubleshooting

**Display doesn't update:**
- Check DESPI-C02 switch (should be 0.47)
- Verify all 8 wires connected
- Check Serial Monitor for errors

**WiFi won't connect:**
- Double-check SSID and password in config.h
- Ensure display is within WiFi range
- Check if network allows IoT devices

**API fetch fails:**
- Test API URL in web browser
- Check JSON format matches expected
- Verify network allows HTTPS

**Battery drains fast:**
- Check Serial Monitor - should see "deep sleep"
- Verify code isn't stuck in loop
- May need to cut "low power pad" on FireBeetle

---

## 📚 Next Steps

**Working? Great!**
1. Mount in enclosure
2. Hang on bulletin board
3. Enjoy 6+ months battery life!

**Want to customize?**
- Edit display layout in `displaySchedule()` function
- Change fonts/sizes in config.h
- Add church logo (requires image conversion)
- Set up multiple wake times for mid-week services

**Need help?**
- Read full README.md
- Check wiring_guide.md for detailed diagrams
- Search Arduino forums for e-paper projects

---

## 📋 Maintenance Schedule

**Weekly:**
- Verify display updated correctly

**Monthly:**
- Check battery voltage via Serial Monitor

**Every 3-6 months:**
- Recharge battery (even if not low)
- Clean display with soft cloth

---

## 🎓 Your API Format

Your API should return JSON like this:

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
    }
  ]
}
```

Don't have an API yet? You can:
1. Create a simple JSON file hosted on your website
2. Use a Google Sheet with a JSON export plugin
3. Use a simple API service like Firebase

---

## 🚀 You're Ready!

That's it! Your church schedule display should now:
- ✅ Wake up before services
- ✅ Connect to WiFi
- ✅ Fetch latest schedule
- ✅ Update display
- ✅ Go back to sleep for months

**Questions?** Check the full README.md or search the Arduino/ESP32 forums!

---

**Project Version:** 1.0  
**Estimated Setup Time:** 30-45 minutes  
**Battery Life:** 6-12 months with daily updates  
**Cost:** ~$95-110 USD
