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

#if DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// Application state
static Schedule schedule;
static BatteryStatus battery;

static void handle_error(const char* message, ResultCode code) {
    LOGF("Error: %s (code %d)\n", message, static_cast<int>(code));
    display_error(message);
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

    // 0. Initialize button and check wake reason
    power_init_button();
    if (power_was_button_wake()) {
        LOG("Manual refresh triggered by button");
    }

    // 1. Initialize battery monitoring and check level
    battery_init();
    battery = battery_read();

    LOGF("Battery: %.2fV (%d%%)\n", battery.voltage, battery.percentage);

    if (battery.isCritical) {
        LOG("Battery critically low - entering hibernate");
        power_enter_hibernate();
        // Never returns
    }

    // 2. Initialize display
    display_init();

    // 3. Connect to WiFi
    LOG("Connecting to WiFi...");
    ResultCode result = wifi_connect();

    if (result != ResultCode::Success) {
        handle_error("WiFi Connection Failed", result);
        // Never returns
    }

    // 4. Sync time via NTP
    LOG("Synchronizing time...");
    result = power_sync_time();

    if (result != ResultCode::Success) {
        LOG("Warning: NTP sync failed, continuing anyway");
        // Don't fail here - we can still show the schedule
    }

    // 5. Fetch schedule from API
    LOG("Fetching schedule...");
    result = api_fetch_schedule(schedule);

    if (result != ResultCode::Success) {
        wifi_disconnect();
        handle_error("API Fetch Failed", result);
        // Never returns
    }

    // 6. Disconnect WiFi (save power)
    wifi_disconnect();

    // 7. Update display
    LOG("Updating display...");
    display_schedule(schedule, battery);

    LOG("\n========================================");
    LOG("Update complete!");
    LOG("========================================\n");

    // 8. Enter deep sleep (unless in test mode)
#if TEST_MODE
    LOG("TEST MODE: Not entering sleep");
#else
    long sleepTime = power_calculate_sleep_time();
    LOGF("Sleeping for %ld seconds\n", sleepTime);
    display_sleep();
    power_enter_sleep(sleepTime);
    // Never returns
#endif
}

void loop() {
    // Normal operation never reaches loop (deep sleep resets)
    // Only runs in TEST_MODE
#if TEST_MODE
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
