#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "schedule.h"
#include <cstdint>

// Initialize button pin for wake-up
void power_init_button();

// Check if device was woken by button press
bool power_was_button_wake();

// Synchronize time with NTP server
// Returns Success if time was synchronized, NtpSyncFailed otherwise
ResultCode power_sync_time();

// Calculate seconds until next scheduled wake time
// Returns the number of seconds to sleep
// If no update days are enabled, returns a default (24 hours)
long power_calculate_sleep_time();

// Enter deep sleep for the specified duration (seconds)
// Also enables button wake-up
// This function does not return
void power_enter_sleep(long seconds);

// Enter hibernate mode (ultra-deep sleep, requires manual reset or button)
// This function does not return
void power_enter_hibernate();

// Check if current time was successfully obtained
bool power_has_valid_time();

#endif // POWER_MANAGER_H
