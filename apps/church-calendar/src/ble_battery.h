#ifndef BLE_BATTERY_H
#define BLE_BATTERY_H

#include <Arduino.h>

// Initialize minimal BLE with just Battery Service
void ble_battery_init(const char* deviceName);

// Start advertising
void ble_battery_start();

// Update battery level (0-100) and voltage
void ble_battery_update(uint8_t percentage, float voltage);

// Check if connected
bool ble_battery_connected();

#endif // BLE_BATTERY_H
