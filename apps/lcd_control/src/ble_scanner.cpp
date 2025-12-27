#include "ble_scanner.h"
#include "config.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>

// State
static BLEScan* pBLEScan = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteService* pRemoteService = nullptr;

static std::vector<DiscoveredDevice> discoveredDevices;
static DeviceStatus currentStatus = {0};
static bool scanning = false;
static bool connected = false;

// Callbacks
static ScanResultCallback onScanResult = nullptr;
static ScanCompleteCallback onScanComplete = nullptr;
static ConnectionCallback onConnection = nullptr;
static StatusUpdateCallback onStatusUpdate = nullptr;
static WifiStatusCallback onWifiStatus = nullptr;

// Remote characteristics (cached after connection)
static BLERemoteCharacteristic* pWifiSSIDChar = nullptr;
static BLERemoteCharacteristic* pWifiPassChar = nullptr;
static BLERemoteCharacteristic* pWifiStatusChar = nullptr;
static BLERemoteCharacteristic* pBatteryLevelChar = nullptr;
static BLERemoteCharacteristic* pBatteryVoltageChar = nullptr;
static BLERemoteCharacteristic* pLastUpdateChar = nullptr;
static BLERemoteCharacteristic* pErrorCodeChar = nullptr;
static BLERemoteCharacteristic* pRssiChar = nullptr;
static BLERemoteCharacteristic* pCommandChar = nullptr;

// Notification callback for WiFi status changes
static void wifiStatusNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 1) {
        BleWifiStatus status = static_cast<BleWifiStatus>(pData[0]);
        currentStatus.wifiStatus = status;
        if (onWifiStatus) {
            onWifiStatus(status);
        }
        if (onStatusUpdate) {
            onStatusUpdate(currentStatus);
        }
    }
}

// Notification callback for battery level
static void batteryNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 1) {
        currentStatus.batteryPercent = pData[0];
        if (onStatusUpdate) {
            onStatusUpdate(currentStatus);
        }
    }
}

// Scan callbacks
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        DiscoveredDevice dev = {0};

        // Get device name
        if (advertisedDevice.haveName()) {
            strncpy(dev.name, advertisedDevice.getName().c_str(), sizeof(dev.name) - 1);
        } else {
            strcpy(dev.name, "(Unknown)");
        }

        // Get address
        strncpy(dev.address, advertisedDevice.getAddress().toString().c_str(), sizeof(dev.address) - 1);

        // Get RSSI
        dev.rssi = advertisedDevice.getRSSI();

        // Check if it's a Church Display (by service UUID or name prefix)
        dev.isChurchDisplay = false;
        if (advertisedDevice.haveServiceUUID()) {
            if (advertisedDevice.isAdvertisingService(BLEUUID(BLE_SERVICE_UUID))) {
                dev.isChurchDisplay = true;
            }
        }
        // Also check by name prefix
        if (strncmp(dev.name, DEVICE_NAME_PREFIX, strlen(DEVICE_NAME_PREFIX)) == 0) {
            dev.isChurchDisplay = true;
        }

        // Add to discovered list if it's a church display
        if (dev.isChurchDisplay) {
            // Check if already in list
            bool found = false;
            for (auto& existing : discoveredDevices) {
                if (strcmp(existing.address, dev.address) == 0) {
                    found = true;
                    existing.rssi = dev.rssi;  // Update RSSI
                    break;
                }
            }
            if (!found) {
                discoveredDevices.push_back(dev);
                LOGF("Found Church Display: %s [%s] RSSI: %d\n", dev.name, dev.address, dev.rssi);
            }

            if (onScanResult) {
                onScanResult(dev);
            }
        }
    }
};

// Client callbacks
class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* client) override {
        connected = true;
        currentStatus.connected = true;
        LOG("BLE Connected");
        if (onConnection) {
            onConnection(true);
        }
    }

    void onDisconnect(BLEClient* client) override {
        connected = false;
        currentStatus.connected = false;
        LOG("BLE Disconnected");

        // Clear characteristic pointers
        pWifiSSIDChar = nullptr;
        pWifiPassChar = nullptr;
        pWifiStatusChar = nullptr;
        pBatteryLevelChar = nullptr;
        pBatteryVoltageChar = nullptr;
        pLastUpdateChar = nullptr;
        pErrorCodeChar = nullptr;
        pRssiChar = nullptr;
        pCommandChar = nullptr;
        pRemoteService = nullptr;

        if (onConnection) {
            onConnection(false);
        }
    }
};

static ClientCallbacks clientCallbacks;

void ble_scanner_init() {
    BLEDevice::init("ChurchDisplayController");

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    LOG("BLE Scanner initialized");
}

void ble_start_scan(int durationSeconds) {
    if (scanning) return;

    discoveredDevices.clear();
    scanning = true;

    LOG("Starting BLE scan...");
    pBLEScan->start(durationSeconds, [](BLEScanResults results) {
        scanning = false;
        LOGF("Scan complete. Found %d Church Display(s)\n", discoveredDevices.size());
        if (onScanComplete) {
            onScanComplete(discoveredDevices.size());
        }
    }, false);
}

void ble_stop_scan() {
    if (scanning) {
        pBLEScan->stop();
        scanning = false;
    }
}

bool ble_is_scanning() {
    return scanning;
}

const std::vector<DiscoveredDevice>& ble_get_discovered_devices() {
    return discoveredDevices;
}

bool ble_connect(const char* address) {
    if (connected) {
        ble_disconnect();
    }

    LOGF("Connecting to %s...\n", address);

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(&clientCallbacks);

    BLEAddress bleAddress(address);
    if (!pClient->connect(bleAddress)) {
        LOG("Connection failed");
        return false;
    }

    // Store address in status
    strncpy(currentStatus.address, address, sizeof(currentStatus.address) - 1);

    // Get service
    pRemoteService = pClient->getService(BLEUUID(BLE_SERVICE_UUID));
    if (!pRemoteService) {
        LOG("Failed to find service");
        pClient->disconnect();
        return false;
    }

    // Get characteristics
    pWifiSSIDChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_WIFI_SSID_UUID));
    pWifiPassChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_WIFI_PASSWORD_UUID));
    pWifiStatusChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_WIFI_STATUS_UUID));
    pBatteryLevelChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_BATTERY_LEVEL_UUID));
    pBatteryVoltageChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_BATTERY_VOLTAGE_UUID));
    pLastUpdateChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_LAST_UPDATE_UUID));
    pErrorCodeChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_ERROR_CODE_UUID));
    pRssiChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_RSSI_UUID));
    pCommandChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_COMMAND_UUID));

    // Subscribe to notifications
    if (pWifiStatusChar && pWifiStatusChar->canNotify()) {
        pWifiStatusChar->registerForNotify(wifiStatusNotifyCallback);
    }
    if (pBatteryLevelChar && pBatteryLevelChar->canNotify()) {
        pBatteryLevelChar->registerForNotify(batteryNotifyCallback);
    }

    // Read device name
    BLERemoteCharacteristic* pNameChar = pRemoteService->getCharacteristic(BLEUUID(BLE_CHAR_DEVICE_NAME_UUID));
    if (pNameChar && pNameChar->canRead()) {
        String name = pNameChar->readValue();
        strncpy(currentStatus.name, name.c_str(), sizeof(currentStatus.name) - 1);
    }

    LOG("Connected and service discovered");
    return true;
}

void ble_disconnect() {
    if (pClient && connected) {
        pClient->disconnect();
    }
    connected = false;
}

bool ble_is_connected() {
    return connected;
}

const DeviceStatus& ble_get_device_status() {
    return currentStatus;
}

bool ble_send_wifi_credentials(const char* ssid, const char* password) {
    if (!connected || !pWifiSSIDChar || !pWifiPassChar) {
        return false;
    }

    LOGF("Sending WiFi credentials: SSID=%s\n", ssid);

    // Write SSID
    pWifiSSIDChar->writeValue(ssid, strlen(ssid));

    // Write password (triggers credential callback on device)
    pWifiPassChar->writeValue(password, strlen(password));

    return true;
}

bool ble_send_command(BleCommand cmd) {
    if (!connected || !pCommandChar) {
        return false;
    }

    LOGF("Sending command: 0x%02X\n", static_cast<uint8_t>(cmd));

    uint8_t cmdByte = static_cast<uint8_t>(cmd);
    pCommandChar->writeValue(&cmdByte, 1);

    return true;
}

bool ble_request_status() {
    if (!connected) return false;

    // Read all status values
    if (pWifiStatusChar && pWifiStatusChar->canRead()) {
        String val = pWifiStatusChar->readValue();
        if (val.length() >= 1) {
            currentStatus.wifiStatus = static_cast<BleWifiStatus>(val[0]);
        }
    }

    if (pBatteryLevelChar && pBatteryLevelChar->canRead()) {
        String val = pBatteryLevelChar->readValue();
        if (val.length() >= 1) {
            currentStatus.batteryPercent = static_cast<uint8_t>(val[0]);
        }
    }

    if (pBatteryVoltageChar && pBatteryVoltageChar->canRead()) {
        String val = pBatteryVoltageChar->readValue();
        if (val.length() >= 2) {
            uint16_t voltInt = val[0] | (val[1] << 8);
            currentStatus.batteryVoltage = voltInt / 100.0f;
        }
    }

    if (pLastUpdateChar && pLastUpdateChar->canRead()) {
        String val = pLastUpdateChar->readValue();
        if (val.length() >= 4) {
            currentStatus.lastUpdate = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
        }
    }

    if (pErrorCodeChar && pErrorCodeChar->canRead()) {
        String val = pErrorCodeChar->readValue();
        if (val.length() >= 1) {
            currentStatus.lastError = static_cast<uint8_t>(val[0]);
        }
    }

    if (pRssiChar && pRssiChar->canRead()) {
        String val = pRssiChar->readValue();
        if (val.length() >= 1) {
            currentStatus.wifiRssi = static_cast<int8_t>(val[0]);
        }
    }

    if (onStatusUpdate) {
        onStatusUpdate(currentStatus);
    }

    return true;
}

void ble_loop() {
    // Process any pending BLE events
    // Most handling is done via callbacks
}

void ble_set_scan_result_callback(ScanResultCallback callback) {
    onScanResult = callback;
}

void ble_set_scan_complete_callback(ScanCompleteCallback callback) {
    onScanComplete = callback;
}

void ble_set_connection_callback(ConnectionCallback callback) {
    onConnection = callback;
}

void ble_set_status_callback(StatusUpdateCallback callback) {
    onStatusUpdate = callback;
}

void ble_set_wifi_status_callback(WifiStatusCallback callback) {
    onWifiStatus = callback;
}
