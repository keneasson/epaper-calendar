#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>
#include <vector>

// BLE UUIDs (must match E-Paper unit definitions)
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

// WiFi status enum (matches E-Paper unit)
enum class BleWifiStatus : uint8_t {
    NotConfigured = 0x00,
    Disconnected = 0x01,
    Connecting = 0x02,
    Connected = 0x03,
    ConnectionFailed = 0x04,
    WrongPassword = 0x05,
    NoApFound = 0x06
};

// Commands (matches E-Paper unit)
enum class BleCommand : uint8_t {
    WifiConnect = 0x01,
    WifiDisconnect = 0x02,
    RefreshNow = 0x03,
    EnterSleep = 0x04,
    Reboot = 0x05,
    FactoryReset = 0x06,
    GetStatus = 0x07
};

// Discovered device info
struct DiscoveredDevice {
    char name[32];
    char address[18];   // MAC address as string "XX:XX:XX:XX:XX:XX"
    int rssi;           // Signal strength
    bool isChurchDisplay;
};

// Connected device status
struct DeviceStatus {
    bool connected;
    char name[32];
    char address[18];
    BleWifiStatus wifiStatus;
    uint8_t batteryPercent;
    float batteryVoltage;
    uint32_t lastUpdate;
    uint8_t lastError;
    int8_t wifiRssi;
};

// Callback types
typedef void (*ScanResultCallback)(const DiscoveredDevice& device);
typedef void (*ScanCompleteCallback)(int deviceCount);
typedef void (*ConnectionCallback)(bool connected);
typedef void (*StatusUpdateCallback)(const DeviceStatus& status);
typedef void (*WifiStatusCallback)(BleWifiStatus status);

// Initialize BLE client
void ble_scanner_init();

// Start scanning for Church Display devices
// Returns immediately; results come via callbacks
void ble_start_scan(int durationSeconds = 10);

// Stop an ongoing scan
void ble_stop_scan();

// Check if currently scanning
bool ble_is_scanning();

// Get list of discovered devices from last scan
const std::vector<DiscoveredDevice>& ble_get_discovered_devices();

// Connect to a device by address
bool ble_connect(const char* address);

// Disconnect from current device
void ble_disconnect();

// Check if connected to a device
bool ble_is_connected();

// Get current device status
const DeviceStatus& ble_get_device_status();

// Send WiFi credentials to connected device
bool ble_send_wifi_credentials(const char* ssid, const char* password);

// Send a command to connected device
bool ble_send_command(BleCommand cmd);

// Request status update from connected device
bool ble_request_status();

// Process BLE events (call in loop)
void ble_loop();

// Set callbacks
void ble_set_scan_result_callback(ScanResultCallback callback);
void ble_set_scan_complete_callback(ScanCompleteCallback callback);
void ble_set_connection_callback(ConnectionCallback callback);
void ble_set_status_callback(StatusUpdateCallback callback);
void ble_set_wifi_status_callback(WifiStatusCallback callback);

#endif // BLE_SCANNER_H
