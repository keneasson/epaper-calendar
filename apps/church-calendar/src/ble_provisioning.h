#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#include <Arduino.h>
#include "schedule.h"

// BLE Service and Characteristic UUIDs
#define BLE_SERVICE_UUID              "00001900-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_WIFI_SSID_UUID       "00001901-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_WIFI_PASSWORD_UUID   "00001902-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_WIFI_STATUS_UUID     "00001903-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_DEVICE_NAME_UUID     "00001904-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_BATTERY_LEVEL_UUID   "00001905-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_BATTERY_VOLTAGE_UUID "00001906-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_LAST_UPDATE_UUID     "00001907-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_FORCE_REFRESH_UUID   "00001908-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_ERROR_CODE_UUID      "00001909-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_RSSI_UUID            "0000190a-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_API_ENDPOINT_UUID    "0000190b-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_COMMAND_UUID         "0000190c-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_RESPONSE_UUID        "0000190d-0000-1000-8000-00805f9b34fb"

// WiFi status values (reported via BLE)
enum class BleWifiStatus : uint8_t {
    NotConfigured = 0x00,
    Disconnected = 0x01,
    Connecting = 0x02,
    Connected = 0x03,
    ConnectionFailed = 0x04,
    WrongPassword = 0x05,
    NoApFound = 0x06
};

// Commands that can be sent via BLE
enum class BleCommand : uint8_t {
    WifiConnect = 0x01,
    WifiDisconnect = 0x02,
    RefreshNow = 0x03,
    EnterSleep = 0x04,
    Reboot = 0x05,
    FactoryReset = 0x06,
    GetStatus = 0x07
};

// Provisioning mode timeout (milliseconds)
#define BLE_PROV_TIMEOUT_MS 300000  // 5 minutes

// Callback function types for events
typedef void (*BleCredentialsCallback)(const char* ssid, const char* password);
typedef void (*BleCommandCallback)(BleCommand cmd);

// Initialize BLE provisioning server
// deviceName: Advertised name (e.g., "ChurchDisplay-01")
void ble_init(const char* deviceName);

// Start BLE advertising (call when entering provisioning mode)
void ble_start_advertising();

// Stop BLE advertising and disconnect any clients
void ble_stop();

// Check if a client is currently connected
bool ble_is_connected();

// Check if provisioning has timed out
bool ble_has_timed_out();

// Process BLE events (call in loop during provisioning)
void ble_loop();

// Update status values (for notifying connected clients)
void ble_update_wifi_status(BleWifiStatus status);
void ble_update_battery(uint8_t percentage, float voltage);
void ble_update_last_update(uint32_t timestamp);
void ble_update_error(ResultCode error);
void ble_update_rssi(int8_t rssi);

// Set callback for when WiFi credentials are received
void ble_set_credentials_callback(BleCredentialsCallback callback);

// Set callback for when a command is received
void ble_set_command_callback(BleCommandCallback callback);

// Get received credentials (valid after callback is invoked)
const char* ble_get_received_ssid();
const char* ble_get_received_password();

#endif // BLE_PROVISIONING_H
