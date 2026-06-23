#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "calendar_data.h"

// Why the last wifi_connect() failed, classified from the 802.11 disconnect
// reason codes collected across the compatibility ladder. Each value maps to
// a distinct customer-actionable remedy (see issue #6).
enum class WifiFailCause {
    None,                 // last connect succeeded (or none attempted yet)
    NetworkNotFound,      // SSID never seen on 2.4GHz (5GHz-only? out of range?)
    RouterRejecting,      // AP visible but never answers our auth on any profile
                          // (router blocked/wedged this device - reboot/forget it)
    WrongPassword,        // AP answered; the key handshake failed
    SecurityUnsupported,  // AP's security mode outside what we accept
    Unknown,              // mixed/no verdicts (marginal RF, AP flapping, ...)
};

// Connect to WiFi with retry logic
// Returns Success or appropriate error code
ResultCode wifi_connect();

// Classified cause of the most recent wifi_connect() failure.
WifiFailCause wifi_get_fail_cause();

// Disconnect and power down WiFi radio
void wifi_disconnect();

// Get current signal strength in dBm (only valid when connected)
int wifi_get_rssi();

// Check if currently connected
bool wifi_is_connected();

// Diagnostic: scan for nearby networks and log each SSID with its RSSI,
// channel and security type, flagging whether the configured WIFI_SSID is
// visible and how strong it is. Intended for bring-up/debugging over serial
// (e.g. when connection fails with AUTH_EXPIRE). No-op effect on normal flow.
void wifi_scan_diagnostic();

#endif // WIFI_MANAGER_H
