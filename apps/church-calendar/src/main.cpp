/*
 * Church Schedule E-Ink Display
 *
 * A battery-powered e-paper display that fetches and shows
 * your church's schedule from an API endpoint.
 *
 * Hardware:
 *   - DFRobot FireBeetle 2 ESP32-E
 *   - Waveshare 7.5" E-Paper (800x480)
 *   - DESPI-C02 Adapter
 *   - 3.7V LiPo Battery
 */

#ifdef NATIVE_BUILD
#include "Arduino.h"
#else
#include <Arduino.h>
#endif

#include "config.h"
#include "schedule.h"
#include "battery.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "display.h"
#include "power_manager.h"
#ifdef BLE_BATTERY_TEST
#include "ble_battery.h"
#else
#include "ble_provisioning.h"
#endif

// Use ESP-IDF logging on C6 (Serial doesn't work reliably with USB CDC)
#if defined(ESP32_C6_BUILD) && DEBUG_MODE
#include "esp_log.h"
static const char* TAG = "APP";
#define LOG(msg) ESP_LOGI(TAG, "%s", msg)
#define LOGF(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#elif DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// Application state
static Schedule schedule;
static BatteryStatus battery;

// ============================================================================
// ACCELERATED TEST MODE - Daily Sunday Simulation
// Every day acts like Sunday for testing the weekly flip-flop:
// - Before 12:00pm → "This Week" mode (weekOffset=0)
// - After 12:00pm → "Next Week" mode (weekOffset=1)
// - Midnight → back to "This Week"
//
// Special 10:30-11:00am window: Update every 5 minutes to catch last-minute
// schedule changes. Only refreshes display if data actually changed.
// ============================================================================
#ifdef ACCELERATED_TEST_MODE

// Persist across deep sleep
RTC_DATA_ATTR int testUpdateCount = 0;
RTC_DATA_ATTR Schedule cachedSchedule;      // Last displayed schedule
RTC_DATA_ATTR bool hasCachedSchedule = false;
RTC_DATA_ATTR int cachedWeekOffset = -1;    // Which week was cached (0 or 1)

// Mock names for testing
static const char* MOCK_NAMES[] = {
    "Alice Anderson", "Bob Baker", "Carol Chen", "David Davis",
    "Emma Evans", "Frank Foster", "Grace Green", "Henry Harris",
    "Ivy Irving", "Jack Johnson", "Kate King", "Leo Lewis"
};
static const int MOCK_NAME_COUNT = 12;

// Override role names in schedule with mock data (keeps real date from API)
static void apply_mock_roles(Schedule& sched, int updateNum) {
    // Use update number to rotate through mock names
    int nameOffset = (updateNum - 1) % MOCK_NAME_COUNT;

    // Format: "Update N - Name" for presiding to show update count
    snprintf(sched.presiding, MAX_NAME_LENGTH, "Update %d - %s", updateNum, MOCK_NAMES[(nameOffset + 0) % MOCK_NAME_COUNT]);
    snprintf(sched.exhorting, MAX_NAME_LENGTH, "%s", MOCK_NAMES[(nameOffset + 1) % MOCK_NAME_COUNT]);
    snprintf(sched.keyboard, MAX_NAME_LENGTH, "%s", MOCK_NAMES[(nameOffset + 2) % MOCK_NAME_COUNT]);
    snprintf(sched.steward, MAX_NAME_LENGTH, "%s", MOCK_NAMES[(nameOffset + 3) % MOCK_NAME_COUNT]);
    snprintf(sched.doorkeeper, MAX_NAME_LENGTH, "%s", MOCK_NAMES[(nameOffset + 4) % MOCK_NAME_COUNT]);

    // Alternate collection
    if (updateNum % 2 == 0) {
        snprintf(sched.collection, MAX_COLLECTION_LENGTH, "Test Collection %d", updateNum);
    } else {
        sched.collection[0] = '\0';  // Clear collection on odd updates
    }

    sched.hasLunch = (updateNum % 3 == 0);
}

// Determine display mode based on hour (every day is Sunday)
// Before 12pm → This Week, After 12pm → Next Week
static DisplayMode get_test_display_mode(int hour) {
    if (hour < 12) {
        return DISPLAY_MODE_CURRENT;
    } else {
        return DISPLAY_MODE_NEXT_WEEK;
    }
}

// Check if we're in the frequent update window (10:30-11:00am)
static bool is_frequent_update_window(int hour, int minute) {
    if (hour == 10 && minute >= 30) return true;
    if (hour == 11 && minute == 0) return true;  // Include 11:00 exactly
    return false;
}

// Calculate sleep time for daily Sunday simulation
// Returns seconds until next wake event
static long calculate_test_sleep_time(int hour, int minute) {
    // Convert current time to minutes since midnight
    int currentMinutes = hour * 60 + minute;

    // Key times (in minutes since midnight):
    // 10:30 = 630, 11:00 = 660, 12:00 = 720, 24:00 = 1440

    long sleepSeconds;

    if (currentMinutes < 630) {
        // Before 10:30am → sleep until 10:30am
        sleepSeconds = (630 - currentMinutes) * 60;
        LOGF("Sleep until 10:30am: %ld seconds\n", sleepSeconds);
    } else if (currentMinutes < 660) {
        // 10:30-11:00am → frequent updates every 5 minutes
        sleepSeconds = 5 * 60;  // 5 minutes
        LOGF("Frequent update window: sleeping 5 minutes\n");
    } else if (currentMinutes < 720) {
        // 11:00am-12:00pm → sleep until 12:00pm (mode change)
        sleepSeconds = (720 - currentMinutes) * 60;
        LOGF("Sleep until 12:00pm: %ld seconds\n", sleepSeconds);
    } else {
        // After 12:00pm → sleep until midnight (mode change)
        sleepSeconds = (1440 - currentMinutes) * 60;
        LOGF("Sleep until midnight: %ld seconds\n", sleepSeconds);
    }

    // Minimum 60 seconds to prevent rapid cycling
    if (sleepSeconds < 60) sleepSeconds = 60;

    return sleepSeconds;
}

// Compare two schedules to see if relevant fields changed
static bool schedule_changed(const Schedule& oldSched, const Schedule& newSched) {
    if (strcmp(oldSched.serviceDate, newSched.serviceDate) != 0) return true;
    if (strcmp(oldSched.presiding, newSched.presiding) != 0) return true;
    if (strcmp(oldSched.exhorting, newSched.exhorting) != 0) return true;
    if (strcmp(oldSched.keyboard, newSched.keyboard) != 0) return true;
    if (strcmp(oldSched.steward, newSched.steward) != 0) return true;
    if (strcmp(oldSched.doorkeeper, newSched.doorkeeper) != 0) return true;
    if (strcmp(oldSched.collection, newSched.collection) != 0) return true;
    if (oldSched.hasLunch != newSched.hasLunch) return true;
    return false;
}

#endif // ACCELERATED_TEST_MODE

// Determine which display mode to use based on current time
// Sunday 12:00pm onwards until Saturday 11:59pm -> Next week mode
// Sunday 12:00am to 11:59am -> Current week mode
static DisplayMode get_display_mode() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        LOG("Time not available, defaulting to current week mode");
        return DISPLAY_MODE_CURRENT;
    }

    int dayOfWeek = timeinfo.tm_wday;  // 0=Sunday, 1=Monday, ... 6=Saturday
    int hour = timeinfo.tm_hour;

    LOGF("Time check: day=%d hour=%d\n", dayOfWeek, hour);

    // Sunday 12:00pm (noon) onwards -> Next week mode
    if (dayOfWeek == 0 && hour >= 12) {
        LOG("Sunday afternoon -> Next week mode");
        return DISPLAY_MODE_NEXT_WEEK;
    }

    // Monday through Saturday -> Next week mode
    if (dayOfWeek >= 1 && dayOfWeek <= 6) {
        LOG("Weekday/Saturday -> Next week mode");
        return DISPLAY_MODE_NEXT_WEEK;
    }

    // Sunday before noon -> Current week mode
    LOG("Sunday morning -> Current week mode");
    return DISPLAY_MODE_CURRENT;
}

static void handle_error(const char* message, ResultCode code) {
    LOGF("Error: %s (code %d)\n", message, static_cast<int>(code));

    // Build detailed error message
    char detail[100];
    const char* reason = "";
    switch(code) {
        case ResultCode::WifiConnectionFailed: reason = "Check SSID/password"; break;
        case ResultCode::WifiTimeout: reason = "Network not found"; break;
        case ResultCode::ApiRequestFailed: reason = "HTTP request failed"; break;
        case ResultCode::ApiTimeout: reason = "API timed out"; break;
        case ResultCode::JsonParseError: reason = "Invalid API response"; break;
        case ResultCode::InvalidResponse: reason = "Empty/bad response"; break;
        case ResultCode::NtpSyncFailed: reason = "Time sync failed"; break;
        case ResultCode::BatteryCritical: reason = "Battery too low"; break;
        default: reason = "Unknown error"; break;
    }
    snprintf(detail, sizeof(detail), "%s - %s", message, reason);

    display_error(detail);
    power_enter_sleep(SLEEP_ON_ERROR);
}

void setup() {
#if DEBUG_MODE
    Serial.begin(SERIAL_BAUD);
    delay(100);
    LOG("\n========================================");
    LOG("Church Schedule Display - Starting");
    LOG("========================================\n");
#endif

    // 0. Initialize display FIRST so we can show status
    display_init();

    // Brief startup delay for firmware updates (reduced from 10s)
#ifdef ESP32_C6_BUILD
    LOG("Startup delay: 2 seconds for firmware update window...");
    delay(2000);
#endif

#ifdef BLE_BATTERY_TEST
    // BLE Battery Test Mode - skips normal operation
    // Allows testing battery ADC via BLE without USB connected
    LOG("=== BLE BATTERY TEST MODE ===");
    LOG("Connect with nRF Connect app to read battery");

    battery_init();
    ble_battery_init("BatteryTest");
    ble_battery_start();
    LOG("BLE advertising started - look for 'BatteryTest'");
    LOG("Uses standard Battery Service (0x180F)");

    // Stay in loop() for continuous battery updates
    return;
#endif

    // 1. Initialize button and check wake reason
    power_init_button();
    if (power_was_button_wake()) {
        LOG("Manual refresh triggered by button");
    }

    // 2. Initialize battery monitoring and check level
    LOG("Checking battery...");
    battery_init();
    battery = battery_read();
    LOGF("Battery: %.2fV (%d%%)\n", battery.voltage, battery.percentage);

    if (battery.isCritical) {
        LOG("Battery critically low - entering hibernate");
#ifndef ESP32_C6_BUILD
        // Skip hibernate on C6 during testing (USB power shows as 0V)
        power_enter_hibernate();
        // Never returns
#else
        LOG("C6 DEBUG: Skipping hibernate for USB-only testing");
#endif
    }

    // 3. Connect to WiFi
    LOG("Connecting to WiFi...");
    LOGF("SSID: %s\n", WIFI_SSID);
    ResultCode result = wifi_connect();

    if (result != ResultCode::Success) {
        handle_error("WiFi Failed", result);
        // Never returns
    }
    LOG("WiFi connected!");

#ifdef ACCELERATED_TEST_MODE
    // ========================================================================
    // ACCELERATED TEST MODE - Daily Sunday Simulation
    // Every day acts like Sunday. Real dates from API, mock role names.
    // 10:30-11:00am: Frequent updates (5 min) with change detection.
    // ========================================================================
    LOG("*** ACCELERATED TEST MODE - Daily Sunday ***");

    // 4. Sync time via NTP
    LOG("Synchronizing time...");
    result = power_sync_time();
    if (result != ResultCode::Success) {
        LOG("Warning: NTP sync failed, continuing anyway");
    }

    // 5. Get current time
    struct tm timeinfo;
    int hour = 0, minute = 0;
    if (getLocalTime(&timeinfo)) {
        hour = timeinfo.tm_hour;
        minute = timeinfo.tm_min;
        LOGF("Current time: %02d:%02d (treating as Sunday)\n", hour, minute);
    }

    // 6. Determine display mode based on hour (daily Sunday simulation)
    DisplayMode displayMode = get_test_display_mode(hour);
    int weekOffset = (displayMode == DISPLAY_MODE_NEXT_WEEK) ? 1 : 0;
    LOGF("Display mode: %s (week offset: %d)\n",
         displayMode == DISPLAY_MODE_NEXT_WEEK ? "NEXT_WEEK" : "CURRENT", weekOffset);

    // 7. Fetch REAL schedule from API
    LOG("Fetching schedule from API...");
    LOGF("API: %s\n", API_ENDPOINT);
    result = api_fetch_schedule(schedule, weekOffset);

    if (result != ResultCode::Success) {
        wifi_disconnect();
        handle_error("API Failed", result);
        // Never returns
    }
    LOGF("API schedule fetched for: %s\n", schedule.serviceDate);

    // 8. Disconnect WiFi (done with network)
    wifi_disconnect();

    // 9. Check if we need to update the display
    // Cache the RAW API response BEFORE applying mock roles for comparison
    bool needsDisplayUpdate = true;
    bool inFrequentWindow = is_frequent_update_window(hour, minute);

    if (inFrequentWindow && hasCachedSchedule && cachedWeekOffset == weekOffset) {
        // In frequent update window - check if API data changed
        // Compare raw API response to cached raw API response
        if (!schedule_changed(cachedSchedule, schedule)) {
            LOG("Schedule unchanged - skipping display update");
            needsDisplayUpdate = false;
        } else {
            LOG("Schedule CHANGED - updating display");
        }
    }

    // Force update if week offset changed (mode switch at 12pm or midnight)
    if (cachedWeekOffset != weekOffset) {
        LOG("Week offset changed - forcing display update");
        needsDisplayUpdate = true;
    }

    if (needsDisplayUpdate) {
        // Cache the API response for future comparisons
        memcpy(&cachedSchedule, &schedule, sizeof(Schedule));
        hasCachedSchedule = true;
        cachedWeekOffset = weekOffset;

        // Increment update counter only when we actually update
        testUpdateCount++;
        LOGF("Test update #%d\n", testUpdateCount);
        LOGF("Presiding: %s\n", schedule.presiding);

        // 10. Update display with REAL API data
        LOG("Updating display...");
        display_schedule(schedule, battery, displayMode);

        LOG("\n========================================");
        LOGF("Display Update #%d complete!\n", testUpdateCount);
        LOG("========================================\n");
    } else {
        LOG("\n========================================");
        LOG("No changes detected - display not updated");
        LOG("========================================\n");
    }

    // 11. Calculate sleep time and enter deep sleep
    long sleepTime = calculate_test_sleep_time(hour, minute);
    LOGF("Sleeping for %ld seconds\n", sleepTime);
    display_sleep();
    power_enter_sleep(sleepTime);
    // Never returns

#else
    // ========================================================================
    // NORMAL PRODUCTION MODE
    // ========================================================================

    // 4. Sync time via NTP
    LOG("Synchronizing time...");
    result = power_sync_time();

    if (result != ResultCode::Success) {
        LOG("Warning: NTP sync failed, continuing anyway");
        // Don't fail here - we can still show the schedule
    }

    // 5. Determine display mode based on time
    DisplayMode displayMode = get_display_mode();
    int weekOffset = (displayMode == DISPLAY_MODE_NEXT_WEEK) ? 1 : 0;
    LOGF("Display mode: %s (week offset: %d)\n",
         displayMode == DISPLAY_MODE_NEXT_WEEK ? "NEXT_WEEK" : "CURRENT", weekOffset);

    // 6. Fetch schedule from API
    LOG("Fetching schedule...");
    LOGF("API: %s\n", API_ENDPOINT);
    result = api_fetch_schedule(schedule, weekOffset);

    if (result != ResultCode::Success) {
        wifi_disconnect();
        handle_error("API Failed", result);
        // Never returns
    }
    LOGF("Schedule fetched for: %s\n", schedule.serviceDate);

    // 7. Disconnect WiFi (save power)
    wifi_disconnect();

    // 8. Update display
    LOG("Updating display...");
    display_schedule(schedule, battery, displayMode);

    LOG("\n========================================");
    LOG("Update complete!");
    LOG("========================================\n");

    // 9. Enter deep sleep (unless in test mode)
#if TEST_MODE
    LOG("TEST MODE: Not entering sleep");
#else
    long sleepTime = power_calculate_sleep_time();
    LOGF("Sleeping for %ld seconds\n", sleepTime);
    display_sleep();
    power_enter_sleep(sleepTime);
    // Never returns
#endif

#endif // ACCELERATED_TEST_MODE
}

void loop() {
    // Normal operation never reaches loop (deep sleep resets)

#ifdef BLE_BATTERY_TEST
    // BLE Battery Test Mode - continuously read and broadcast battery
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    if (now - lastUpdate >= 2000) {  // Update every 2 seconds
        lastUpdate = now;
        battery = battery_read();
        ble_battery_update(battery.percentage, battery.voltage);
        // LOGF("BLE Battery Update: %.2fV (%d%%)\n", battery.voltage, battery.percentage);
    }

    delay(100);
#elif TEST_MODE
    delay(1000);
#endif
}

#ifdef NATIVE_BUILD
// Native builds need a main() entry point
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    setup();

    // In normal operation, setup() calls power_enter_sleep() which exits
    // This loop only runs in TEST_MODE
    while (true) {
        loop();
    }

    return 0;
}
#endif
