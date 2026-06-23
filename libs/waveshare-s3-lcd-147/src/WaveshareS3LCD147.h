/*
 * WaveshareS3LCD147.h
 * LovyanGFX Display Driver for Waveshare ESP32-S3-LCD-1.47B
 *
 * Board: https://www.waveshare.com/esp32-s3-lcd-1.47.htm
 * Panel: ST7789 controller, 172x320 pixels (1.47" round-corner LCD)
 * IMU: QMI8658 6-axis accelerometer/gyroscope
 *
 * USAGE:
 *   #include <WaveshareS3LCD147.h>
 *
 *   WaveshareS3LCD147 lcd;
 *
 *   void setup() {
 *       lcd.init();
 *       lcd.setRotation(1);      // Landscape: 320x172
 *       lcd.setBrightness(200);  // 0-255
 *       lcd.fillScreen(TFT_BLACK);
 *   }
 *
 * CRITICAL CONFIGURATION NOTE:
 * ----------------------------
 * The ST7789 controller has 240x320 internal memory, but only 172x320 pixels
 * are visible on this panel. The configuration MUST use:
 *   - memory_width = 240  (controller's full memory width)
 *   - memory_height = 320 (controller's full memory height)
 *   - panel_width = 172   (visible pixels)
 *   - panel_height = 320  (visible pixels)
 *   - offset_x = 34       (centers 172 in 240: (240-172)/2 = 34)
 *   - offset_y = 0        (no vertical offset needed)
 *
 * If you incorrectly set memory_width = 172 (matching panel), the display
 * will clip content after rotation - only half the screen will be usable!
 *
 * PIN ASSIGNMENTS (from Waveshare schematic):
 * -------------------------------------------
 * Display SPI:
 *   - SCLK: GPIO 40
 *   - MOSI: GPIO 45
 *   - DC:   GPIO 41
 *   - CS:   GPIO 42
 *   - RST:  GPIO 39
 *   - BL:   GPIO 46 (PWM backlight)
 *
 * QMI8658 IMU (I2C):
 *   - SDA:  GPIO 48
 *   - SCL:  GPIO 47
 *   - Address: 0x6B
 *
 * Other:
 *   - BOOT Button: GPIO 0
 *   - NeoPixel LED: GPIO 38
 */

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Pin definitions for easy reference
#define WS_LCD_PIN_SCLK     40
#define WS_LCD_PIN_MOSI     45
#define WS_LCD_PIN_DC       41
#define WS_LCD_PIN_CS       42
#define WS_LCD_PIN_RST      39
#define WS_LCD_PIN_BL       46

#define WS_IMU_PIN_SDA      48
#define WS_IMU_PIN_SCL      47
#define WS_IMU_ADDR         0x6B

#define WS_BOOT_BUTTON      0
#define WS_NEOPIXEL         38

// Screen dimensions after rotation(1) - landscape mode
#define WS_SCREEN_WIDTH     320
#define WS_SCREEN_HEIGHT    172

class WaveshareS3LCD147 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;

public:
    WaveshareS3LCD147(void) {
        // SPI bus configuration
        auto cfg = _bus_instance.config();
        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = 80000000;
        cfg.freq_read = 16000000;
        cfg.pin_sclk = WS_LCD_PIN_SCLK;
        cfg.pin_mosi = WS_LCD_PIN_MOSI;
        cfg.pin_miso = -1;
        cfg.pin_dc = WS_LCD_PIN_DC;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);

        // Panel configuration
        // IMPORTANT: memory_width must be 240 (ST7789 controller memory),
        // NOT 172 (panel size), or display will clip after rotation!
        auto cfg_panel = _panel_instance.config();
        cfg_panel.pin_cs = WS_LCD_PIN_CS;
        cfg_panel.pin_rst = WS_LCD_PIN_RST;
        cfg_panel.pin_busy = -1;
        cfg_panel.memory_width = 240;   // ST7789 controller memory (NOT panel!)
        cfg_panel.memory_height = 320;
        cfg_panel.panel_width = 172;    // Actual visible pixels
        cfg_panel.panel_height = 320;
        cfg_panel.offset_x = 34;        // (240-172)/2 = 34
        cfg_panel.offset_y = 0;
        cfg_panel.offset_rotation = 0;
        cfg_panel.invert = true;        // Required for correct colors
        cfg_panel.rgb_order = false;
        _panel_instance.config(cfg_panel);

        // Backlight PWM configuration
        auto cfg_light = _light_instance.config();
        cfg_light.pin_bl = WS_LCD_PIN_BL;
        cfg_light.invert = false;
        cfg_light.freq = 44100;
        cfg_light.pwm_channel = 7;
        _light_instance.config(cfg_light);
        _panel_instance.setLight(&_light_instance);

        setPanel(&_panel_instance);
    }
};
