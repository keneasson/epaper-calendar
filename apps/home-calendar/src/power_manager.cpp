#include "power_manager.h"
#include "config.h"

#include <stdlib.h>  // setenv()

#ifdef NATIVE_BUILD
#include "Arduino.h"
#include "esp_sleep.h"
#include "time_sim.h"
#else
#include <Arduino.h>
#include <esp_sleep.h>
#include <time.h>
#endif

#if DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// Fallback sleep duration if time sync fails
static constexpr long DEFAULT_SLEEP_SECONDS = 3600;  // 1 hour

// Maximum NTP sync wait time
static constexpr int NTP_SYNC_TIMEOUT_MS = 10000;

static bool timeValid = false;

void power_init_button() {
#ifndef NATIVE_BUILD
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
    LOG("Power: Wake button initialized on GPIO " + String(WAKE_BUTTON_PIN));
#else
    LOG("Power: Button initialized (simulation)");
#endif
}

bool power_was_button_wake() {
#ifndef NATIVE_BUILD
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    // ESP32-C6 uses GPIO wakeup
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        LOG("Power: Woken by button press (GPIO)");
        return true;
    }
    return false;
#else
    return false;  // Simulation always returns false
#endif
}

void power_init_timezone() {
    // Set the POSIX TZ rules so localtime_r() applies the correct local time
    // (and DST) during rendering. This is deliberately network-independent: it
    // only configures the timezone, it does not start NTP. Calling it on every
    // boot before any rendering guarantees correct local time even on the
    // offline cache-fallback path (where power_sync_time() never runs and the
    // display would otherwise show raw UTC, i.e. times off by the GMT offset).
    setenv("TZ", TIMEZONE, 1);
    tzset();
    LOGF("Time: Timezone set to %s\n", TIMEZONE);
}

ResultCode power_sync_time() {
    LOG("Time: Starting NTP sync");
    LOGF("Time: Server: %s\n", NTP_SERVER);

    // configTzTime() applies the TIMEZONE rules (DST-aware) AND starts SNTP.
    // This re-applies the same TZ set in power_init_timezone() and kicks off
    // the actual network time sync now that WiFi is connected.
    configTzTime(TIMEZONE, NTP_SERVER);

    // Wait for time to be set
    struct tm timeinfo;
    unsigned long startTime = millis();

    while (!getLocalTime(&timeinfo)) {
        if (millis() - startTime > NTP_SYNC_TIMEOUT_MS) {
            LOG("Time: NTP sync timeout");
            timeValid = false;
            return ResultCode::NtpSyncFailed;
        }
        delay(100);
    }

    timeValid = true;

    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    LOGF("Time: Synced to %s\n", timeStr);

    return ResultCode::Success;
}

bool power_has_valid_time() {
    return timeValid;
}

int power_get_current_hour() {
    if (!timeValid) return -1;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return -1;

    return timeinfo.tm_hour;
}

bool power_is_date_change_wake() {
    int hour = power_get_current_hour();
    // Date change wake is at 1am (or shortly after due to drift)
    // Consider it a date change wake if we're between 1am and 2am
    return (hour >= DATE_CHANGE_HOUR && hour < DATE_CHANGE_HOUR + 1);
}

long power_calculate_sleep_time() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        LOG("Power: Cannot get time, using default sleep");
        return DEFAULT_SLEEP_SECONDS;
    }

    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentSecond = timeinfo.tm_sec;

    long currentSeconds = currentHour * 3600L + currentMinute * 60L + currentSecond;

    LOGF("Power: Current time: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);

    // During daytime hours (6am - 11pm), wake hourly for sync checks
    if (currentHour >= DAY_START_HOUR && currentHour < DAY_END_HOUR) {
        // Sleep until the next hour
        int nextHour = currentHour + 1;
        long targetSeconds = nextHour * 3600L;
        long sleepSeconds = targetSeconds - currentSeconds;
        LOGF("Power: Daytime - next wake at %02d:00 (sleep %ld sec)\n",
             nextHour, sleepSeconds);
        return sleepSeconds;
    }

    // After DAY_END_HOUR (11pm) or before DAY_START_HOUR (6am):
    // Sleep until DATE_CHANGE_HOUR (1am)
    long targetSeconds = DATE_CHANGE_HOUR * 3600L;

    if (currentHour >= DATE_CHANGE_HOUR) {
        // It's after 1am today, so target tomorrow's 1am
        long sleepSeconds = (86400L - currentSeconds) + targetSeconds;
        LOGF("Power: Night - next wake tomorrow at %02d:00 (sleep %ld sec)\n",
             DATE_CHANGE_HOUR, sleepSeconds);
        return sleepSeconds;
    } else {
        // It's before 1am (e.g., midnight), target today's 1am
        long sleepSeconds = targetSeconds - currentSeconds;
        LOGF("Power: Early morning - next wake at %02d:00 (sleep %ld sec)\n",
             DATE_CHANGE_HOUR, sleepSeconds);
        return sleepSeconds;
    }
}

void power_enter_sleep(long seconds) {
    LOGF("Power: Entering deep sleep for %ld seconds\n", seconds);

#if DEBUG_MODE
    Serial.flush();
#endif

#ifndef NATIVE_BUILD
    // Enable timer wake-up
    uint64_t sleepMicros = static_cast<uint64_t>(seconds) * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleepMicros);

    // ESP32-C6: Use GPIO deep sleep wakeup (only GPIO 0-7 supported!)
    esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    LOG("Power: Timer and GPIO wake-up enabled (C6)");

    esp_deep_sleep_start();
#else
    LOGF("Sleeping for %ld seconds\n", seconds);
#endif

    // Never reached
}

void power_enter_hibernate() {
    LOG("Power: Entering hibernate mode");

#if DEBUG_MODE
    Serial.flush();
#endif

#ifndef NATIVE_BUILD
    // ESP32-C6: Use GPIO deep sleep wakeup only (no timer, GPIO 0-7 only!)
    esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    LOG("Power: GPIO wake-up enabled (C6)");
    esp_deep_sleep_start();
#else
    LOG("Power: Hibernate (simulation - exiting)");
#endif

    // Never reached
}
