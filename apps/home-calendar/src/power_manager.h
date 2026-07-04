#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "calendar_data.h"
#include <cstdint>

// Initialize button pin for wake-up
void power_init_button();

// Check if device was woken by button press
bool power_was_button_wake();

// Apply the local timezone (TIMEZONE) so localtime_r() renders correct local
// time. Must be called on EVERY boot (deep sleep clears the TZ env var) and on
// ALL render paths, including offline cache-fallback wakes where NTP never
// runs. Independent of network: only sets the TZ rules, does not contact NTP.
void power_init_timezone();

// Synchronize time with NTP server
// Returns Success if time was synchronized, NtpSyncFailed otherwise
ResultCode power_sync_time();

// Calculate seconds until next scheduled wake time
// Wakes at 6am, 12pm, and 6pm daily
long power_calculate_sleep_time();

// Calculate the deep-sleep duration to use after a failed wake (WiFi/API down,
// serving stale cache). Backs off exponentially with the number of consecutive
// failures so a router/internet outage doesn't drain the battery by retrying
// every hour: 1h -> 2h -> 4h -> 8h (capped). Pass the running failure count
// (>=1). On the next success the caller resets its count and returns to the
// normal power_calculate_sleep_time() cadence.
long power_backoff_sleep_seconds(uint32_t consecutiveFailures);

// Enter deep sleep for the specified duration (seconds)
// Also enables button wake-up
// This function does not return
void power_enter_sleep(long seconds);

// Enter hibernate mode (ultra-deep sleep, requires manual reset or button)
// This function does not return
void power_enter_hibernate();

// Check if current time was successfully obtained
bool power_has_valid_time();

// Get current hour (0-23), returns -1 if time not valid
int power_get_current_hour();

// Check if this is the date change wake (1am)
// Used to determine if display should be refreshed even on 304
bool power_is_date_change_wake();

#endif // POWER_MANAGER_H
