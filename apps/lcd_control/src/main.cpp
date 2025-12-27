/*
 * LCD Control Unit for Church Display Management
 *
 * Waveshare ESP32-S3-LCD-1.47
 *
 * Features:
 *   - BLE scanner for discovering Church Display units
 *   - WiFi provisioning via BLE
 *   - Real-time status monitoring
 *   - Touch UI for configuration
 */

#include <Arduino.h>
#include "config.h"
#include "ble_scanner.h"

// TODO: Add LVGL and display drivers
// #include <lvgl.h>
// #include "ui_manager.h"

// Application state
enum class AppState {
    Home,           // Main dashboard showing known devices
    Scanning,       // Scanning for new devices
    Connecting,     // Connecting to a device
    DeviceStatus,   // Viewing connected device status
    Provisioning,   // Entering WiFi credentials
    Settings        // App settings
};

static AppState currentState = AppState::Home;
static int selectedDeviceIndex = -1;

// Callbacks
void onDeviceFound(const DiscoveredDevice& device) {
    LOGF("UI: Device found - %s\n", device.name);
    // TODO: Update LVGL list
}

void onScanComplete(int count) {
    LOGF("UI: Scan complete, %d device(s) found\n", count);
    if (count == 0) {
        // TODO: Show "No devices found" message
    }
    // TODO: Update UI state
}

void onConnectionChanged(bool connected) {
    LOGF("UI: Connection %s\n", connected ? "established" : "lost");
    if (connected) {
        currentState = AppState::DeviceStatus;
        // Request initial status
        ble_request_status();
    } else {
        currentState = AppState::Home;
    }
    // TODO: Update UI
}

void onStatusUpdate(const DeviceStatus& status) {
    LOGF("UI: Status update - Battery: %d%%, WiFi: %d\n",
         status.batteryPercent, static_cast<int>(status.wifiStatus));
    // TODO: Update status screen
}

void onWifiStatusChange(BleWifiStatus status) {
    const char* statusStr = "Unknown";
    switch (status) {
        case BleWifiStatus::NotConfigured: statusStr = "Not Configured"; break;
        case BleWifiStatus::Disconnected: statusStr = "Disconnected"; break;
        case BleWifiStatus::Connecting: statusStr = "Connecting..."; break;
        case BleWifiStatus::Connected: statusStr = "Connected"; break;
        case BleWifiStatus::ConnectionFailed: statusStr = "Failed"; break;
        case BleWifiStatus::WrongPassword: statusStr = "Wrong Password"; break;
        case BleWifiStatus::NoApFound: statusStr = "Network Not Found"; break;
    }
    LOGF("UI: WiFi status changed - %s\n", statusStr);
    // TODO: Update provisioning screen
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    LOG("\n========================================");
    LOG("Church Display - LCD Control Unit");
    LOG("========================================\n");

    // Initialize BLE
    LOG("Initializing BLE...");
    ble_scanner_init();

    // Set up callbacks
    ble_set_scan_result_callback(onDeviceFound);
    ble_set_scan_complete_callback(onScanComplete);
    ble_set_connection_callback(onConnectionChanged);
    ble_set_status_callback(onStatusUpdate);
    ble_set_wifi_status_callback(onWifiStatusChange);

    // TODO: Initialize display
    // display_init();

    // TODO: Initialize LVGL
    // lv_init();
    // ui_init();

    LOG("Setup complete!");

    // For testing: start a scan immediately
    LOG("\nStarting initial scan...");
    ble_start_scan(BLE_SCAN_DURATION_SEC);
}

void loop() {
    // Process BLE events
    ble_loop();

    // TODO: Process LVGL
    // lv_timer_handler();

    // Simple serial command interface for testing (before UI is ready)
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 's':  // Scan
                LOG("Starting scan...");
                ble_start_scan(BLE_SCAN_DURATION_SEC);
                break;

            case 'l':  // List discovered devices
                {
                    const auto& devices = ble_get_discovered_devices();
                    LOGF("Found %d device(s):\n", devices.size());
                    for (size_t i = 0; i < devices.size(); i++) {
                        LOGF("  [%d] %s (%s) RSSI: %d\n",
                             i, devices[i].name, devices[i].address, devices[i].rssi);
                    }
                }
                break;

            case 'c':  // Connect to first device
                {
                    const auto& devices = ble_get_discovered_devices();
                    if (devices.size() > 0) {
                        LOGF("Connecting to %s...\n", devices[0].name);
                        ble_connect(devices[0].address);
                    } else {
                        LOG("No devices to connect to. Run scan first.");
                    }
                }
                break;

            case 'd':  // Disconnect
                LOG("Disconnecting...");
                ble_disconnect();
                break;

            case 'r':  // Request status
                if (ble_is_connected()) {
                    LOG("Requesting status...");
                    ble_request_status();
                } else {
                    LOG("Not connected");
                }
                break;

            case 'w':  // Send WiFi credentials (test)
                if (ble_is_connected()) {
                    LOG("Sending test WiFi credentials...");
                    ble_send_wifi_credentials("TestNetwork", "TestPassword123");
                    ble_send_command(BleCommand::WifiConnect);
                } else {
                    LOG("Not connected");
                }
                break;

            case 'f':  // Force refresh
                if (ble_is_connected()) {
                    LOG("Sending refresh command...");
                    ble_send_command(BleCommand::RefreshNow);
                } else {
                    LOG("Not connected");
                }
                break;

            case 'h':  // Help
            case '?':
                LOG("\nCommands:");
                LOG("  s - Start BLE scan");
                LOG("  l - List discovered devices");
                LOG("  c - Connect to first device");
                LOG("  d - Disconnect");
                LOG("  r - Request status");
                LOG("  w - Send test WiFi credentials");
                LOG("  f - Force refresh");
                LOG("  h - This help");
                break;
        }
    }

    delay(10);
}
