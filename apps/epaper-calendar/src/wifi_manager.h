#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "schedule.h"

// Connect to WiFi with retry logic
// Returns Success or appropriate error code
ResultCode wifi_connect();

// Disconnect and power down WiFi radio
void wifi_disconnect();

// Get current signal strength in dBm (only valid when connected)
int wifi_get_rssi();

// Check if currently connected
bool wifi_is_connected();

#endif // WIFI_MANAGER_H
