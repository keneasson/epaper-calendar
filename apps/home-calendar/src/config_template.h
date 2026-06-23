/*
 * Home Calendar Display - Configuration
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
// API (AWS Lambda endpoint)
// ============================================================================

#define API_ENDPOINT        "https://api.kene.info/calendar"
#define API_KEY             ""      // Leave empty if no auth required
#define API_TIMEOUT_MS      15000   // 15 seconds
#define API_MAX_RETRIES     2
#define API_RETRY_DELAY_MS  5000

#define JSON_BUFFER_SIZE    8192    // Larger for calendar data

// ============================================================================
// WAKE SCHEDULE
// ============================================================================

// Date change wake - early morning to ensure we're past midnight
#define DATE_CHANGE_HOUR    1       // 1:00 AM - reliable date change

// Hourly wake range - wake every hour during these hours for sync checks
// These are "silent" checks - display only updates if data changed
#define DAY_START_HOUR      6       // First hourly wake (6:00 AM)
#define DAY_END_HOUR        23      // Last hourly wake (11:00 PM)

// Sleep durations
#define SLEEP_ON_ERROR      1800    // 30 minutes on error

// ============================================================================
// DISPLAY (Portrait Mode: 480 wide x 800 tall)
// ============================================================================

// Rotation: 0=normal, 1=90CW, 2=180, 3=270CW
// Portrait mode uses rotation 3 (270 CW from landscape)
#define DISPLAY_ROTATION    3

// Layout split (total height = 800)
// Note: Layout constants defined in display.cpp

// Footer options
#define SHOW_BATTERY        1
#define SHOW_LAST_UPDATE    1

// ============================================================================
// HARDWARE PINS (ESP32-C6 + DESPI-C02 + GDEY075Z08)
// ============================================================================

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

// Wake button (GPIO for wake-from-sleep)
// NOTE: ESP32-C6 only supports GPIO 0-7 for deep sleep wake!
// GPIO 9 (boot button) CANNOT wake from deep sleep.
// Wire an external button between GPIO 5 and GND.
#define WAKE_BUTTON_PIN     5   // Must be GPIO 0-7 for deep sleep wake
#define BUTTON_DEBOUNCE_MS  50

// Display type flag
#define DISPLAY_3COLOR      1

// ============================================================================
// BATTERY THRESHOLDS
// ============================================================================

#define BATTERY_LOW_WARN    3.3f    // Show warning below this voltage
#define BATTERY_CRITICAL    3.0f    // Enter hibernate below this voltage

// ============================================================================
// TIME
// ============================================================================

#define NTP_SERVER          "pool.ntp.org"

// POSIX timezone string with explicit DST transition rules (handles DST
// automatically). Examples:
//   US Eastern:  "EST5EDT,M3.2.0,M11.1.0"
//   US Central:  "CST6CDT,M3.2.0,M11.1.0"
//   US Mountain: "MST7MDT,M3.2.0,M11.1.0"
//   US Pacific:  "PST8PDT,M3.2.0,M11.1.0"
#define TIMEZONE            "EST5EDT,M3.2.0,M11.1.0"

// ============================================================================
// CACHE (SPIFFS)
// ============================================================================

#define CACHE_FILE_PATH        "/calendar.json"
#define CACHE_MAX_AGE_SECONDS  86400   // 24 hours - show "stale" indicator after this

// ============================================================================
// DEBUG
// ============================================================================

// Set via build flags in platformio.ini, or override here
#ifndef DEBUG_MODE
#define DEBUG_MODE          1
#endif

#define SERIAL_BAUD         115200

// Test mode: prevents deep sleep (WARNING: drains battery fast)
#ifndef TEST_MODE
#define TEST_MODE           0
#endif

#endif // CONFIG_H
