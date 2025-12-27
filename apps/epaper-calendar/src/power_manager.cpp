#include "power_manager.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#include "esp_sleep.h"
#include "time_sim.h"
#else
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <time.h>
#endif

#if DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// Fallback sleep duration if no days enabled or time sync fails
static constexpr long DEFAULT_SLEEP_SECONDS = 86400;  // 24 hours

// Maximum NTP sync wait time
static constexpr int NTP_SYNC_TIMEOUT_MS = 10000;

static bool timeValid = false;

void power_init_button() {
#ifndef NATIVE_BUILD
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    LOG("Power: Button initialized on GPIO " + String(BUTTON_PIN));
#else
    LOG("Power: Button initialized (simulation)");
#endif
}

bool power_was_button_wake() {
#ifndef NATIVE_BUILD
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        LOG("Power: Woken by button press");
        return true;
    }
    return false;
#else
    return false;  // Simulation always returns false
#endif
}

// Check if updates are enabled for a given day (0=Sunday, 6=Saturday)
static bool is_update_day(int day) {
    switch (day) {
        case 0: return UPDATE_SUNDAY;
        case 1: return UPDATE_MONDAY;
        case 2: return UPDATE_TUESDAY;
        case 3: return UPDATE_WEDNESDAY;
        case 4: return UPDATE_THURSDAY;
        case 5: return UPDATE_FRIDAY;
        case 6: return UPDATE_SATURDAY;
        default: return false;
    }
}

// Check if any update days are configured
static bool has_any_update_day() {
    return UPDATE_SUNDAY || UPDATE_MONDAY || UPDATE_TUESDAY ||
           UPDATE_WEDNESDAY || UPDATE_THURSDAY || UPDATE_FRIDAY ||
           UPDATE_SATURDAY;
}

ResultCode power_sync_time() {
    LOG("Time: Starting NTP sync");
    LOGF("Time: Server: %s\n", NTP_SERVER);

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

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

long power_calculate_sleep_time() {
    // Guard: if no update days configured, use default
    if (!has_any_update_day()) {
        LOG("Power: No update days configured, using default sleep");
        return DEFAULT_SLEEP_SECONDS;
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        LOG("Power: Cannot get time, using default sleep");
        return DEFAULT_SLEEP_SECONDS;
    }

    int currentDay = timeinfo.tm_wday;  // 0 = Sunday
    long currentSeconds = timeinfo.tm_hour * 3600L + timeinfo.tm_min * 60L + timeinfo.tm_sec;
    long targetSeconds = WAKE_HOUR * 3600L + WAKE_MINUTE * 60L;

    LOGF("Power: Current day=%d, time=%ld sec since midnight\n", currentDay, currentSeconds);
    LOGF("Power: Target wake=%02d:%02d (%ld sec)\n", WAKE_HOUR, WAKE_MINUTE, targetSeconds);

    // Check if we should update later today
    if (is_update_day(currentDay) && currentSeconds < targetSeconds) {
        long sleepSeconds = targetSeconds - currentSeconds;
        LOGF("Power: Sleeping until later today: %ld seconds\n", sleepSeconds);
        return sleepSeconds;
    }

    // Find next update day
    int daysToNext = 0;
    int checkDay = currentDay;

    for (int i = 0; i < 7; i++) {
        checkDay = (checkDay + 1) % 7;
        daysToNext++;

        if (is_update_day(checkDay)) {
            break;
        }
    }

    // Calculate sleep time to next update day at target time
    long sleepSeconds = (86400L - currentSeconds)  // Rest of today
                      + (daysToNext - 1) * 86400L  // Full days in between
                      + targetSeconds;              // Time into target day

    LOGF("Power: Next update in %d days, sleeping %ld seconds\n", daysToNext, sleepSeconds);

    return sleepSeconds;
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

    // Enable button wake-up (EXT0 on GPIO, wake on LOW)
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BUTTON_PIN), 0);

    LOG("Power: Timer and button wake-up enabled");
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
    // Enable button wake-up only (no timer)
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BUTTON_PIN), 0);
    LOG("Power: Button wake-up enabled");
    esp_deep_sleep_start();
#else
    LOG("Power: Hibernate (simulation - exiting)");
#endif

    // Never reached
}
