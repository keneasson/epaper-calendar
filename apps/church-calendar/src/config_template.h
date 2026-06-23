/*
 * Church Schedule Display - Configuration
 *
 * Copy this file to: config.h
 * Fill in your WiFi and API information.
 *
 * IMPORTANT: config.h is gitignored to protect credentials.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WIFI
// ============================================================================

#define WIFI_SSID           "YourWiFiName"
#define WIFI_PASSWORD       "YourWiFiPassword"
#define WIFI_TIMEOUT_MS     20000   // 20 seconds
#define WIFI_MAX_RETRIES    3

// ============================================================================
// API
// ============================================================================

#define API_ENDPOINT        "https://www.tee-admin.com/api/upcoming-program?order=memorial"
#define API_KEY             ""      // Leave empty if no auth required
#define API_TIMEOUT_MS      10000   // 10 seconds
#define API_MAX_RETRIES     2
#define API_RETRY_DELAY_MS  5000

#define JSON_BUFFER_SIZE    4096

// ============================================================================
// SCHEDULE
// ============================================================================

// Wake time (24-hour format)
#define WAKE_HOUR           7
#define WAKE_MINUTE         0

// Days to update (1 = enabled, 0 = disabled)
#define UPDATE_SUNDAY       1
#define UPDATE_MONDAY       0
#define UPDATE_TUESDAY      0
#define UPDATE_WEDNESDAY    1   // Mid-week service
#define UPDATE_THURSDAY     0
#define UPDATE_FRIDAY       0
#define UPDATE_SATURDAY     0

// Sleep duration on error (seconds)
#define SLEEP_ON_ERROR      3600    // 1 hour

// ============================================================================
// DISPLAY
// ============================================================================

// Rotation: 0=normal, 1=90CW, 2=180, 3=270CW
#define DISPLAY_ROTATION    0

// Header text (two lines for display)
#define CHURCH_NAME_LINE1   "Toronto East"
#define CHURCH_NAME_LINE2   "Christadelphians"

// Footer options
#define SHOW_BATTERY        1
#define SHOW_LAST_UPDATE    1

// ============================================================================
// HARDWARE PINS
// ============================================================================

#ifdef ESP32_C6_BUILD
// ---- ESP32-C6 + DESPI-C02 + GDEY075Z08 (3-color) ----

// E-Paper display pins
#define EPD_CS              1
#define EPD_DC              2
#define EPD_RST             3
#define EPD_BUSY            4

// SPI (ESP32-C6)
#define SPI_MOSI            22  // SDI on DESPI-C02
#define SPI_CLK             23  // SCK on DESPI-C02

// Battery ADC (FireBeetle 2 ESP32-C6)
#define BATTERY_PIN         0   // ADC1_CH0 on C6
#define VOLTAGE_DIVIDER     2.0

// Refresh button (GPIO for wake-from-sleep)
#define BUTTON_PIN          9   // Boot button on C6
#define BUTTON_DEBOUNCE_MS  50

// Display type flag
#define DISPLAY_3COLOR      1

#else
// ---- ESP32-E + DESPI-C02 + Waveshare 7.5" B/W ----

// E-Paper display (Waveshare 7.5" via DESPI-C02)
#define EPD_CS              5
#define EPD_DC              17
#define EPD_RST             16
#define EPD_BUSY            4

// SPI (ESP32 default)
#define SPI_MOSI            23
#define SPI_CLK             18

// Battery ADC (FireBeetle 2 ESP32-E specific)
#define BATTERY_PIN         36
#define VOLTAGE_DIVIDER     2.0

// Refresh button (RTC GPIO for wake-from-sleep)
#define BUTTON_PIN          27
#define BUTTON_DEBOUNCE_MS  50

// Display type flag
#define DISPLAY_3COLOR      0

#endif

// ============================================================================
// BATTERY THRESHOLDS
// ============================================================================

#define BATTERY_LOW_WARN    3.3f    // Show warning below this voltage
#define BATTERY_CRITICAL    3.0f    // Enter hibernate below this voltage

// ============================================================================
// TIME
// ============================================================================

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -18000  // -5 hours (EST)
#define DAYLIGHT_OFFSET_SEC 3600    // +1 hour (DST)

// ============================================================================
// DEBUG
// ============================================================================

// Set via build flags in platformio.ini, or override here
#ifndef DEBUG_MODE
#define DEBUG_MODE          1
#endif

#define SERIAL_BAUD         115200

// Test mode: prevents deep sleep (WARNING: drains battery fast)
#define TEST_MODE           0

#endif // CONFIG_H
