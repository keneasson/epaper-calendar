#include "ble_provisioning.h"

#if !defined(NATIVE_BUILD) && !defined(DISABLE_BLE_PROVISIONING)

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// State
static BLEServer* pServer = nullptr;
static BLECharacteristic* pWifiStatusChar = nullptr;
static BLECharacteristic* pBatteryLevelChar = nullptr;
static BLECharacteristic* pBatteryVoltageChar = nullptr;
static BLECharacteristic* pLastUpdateChar = nullptr;
static BLECharacteristic* pErrorCodeChar = nullptr;
static BLECharacteristic* pRssiChar = nullptr;
static BLECharacteristic* pResponseChar = nullptr;

static bool deviceConnected = false;
static bool advertising = false;
static unsigned long advertisingStartTime = 0;

static char receivedSSID[64] = "";
static char receivedPassword[128] = "";

static BleCredentialsCallback credentialsCallback = nullptr;
static BleCommandCallback commandCallback = nullptr;

// Server callbacks
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        deviceConnected = true;
    }

    void onDisconnect(BLEServer* server) override {
        deviceConnected = false;
        // Restart advertising if still in provisioning mode
        if (advertising) {
            BLEDevice::startAdvertising();
        }
    }
};

// Characteristic callbacks for WiFi SSID
class SSIDCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        String value = pChar->getValue();
        if (value.length() > 0 && value.length() < sizeof(receivedSSID)) {
            strncpy(receivedSSID, value.c_str(), sizeof(receivedSSID) - 1);
            receivedSSID[sizeof(receivedSSID) - 1] = '\0';
        }
    }

    void onRead(BLECharacteristic* pChar) override {
        pChar->setValue(receivedSSID);
    }
};

// Characteristic callbacks for WiFi Password
class PasswordCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        String value = pChar->getValue();
        if (value.length() < sizeof(receivedPassword)) {
            strncpy(receivedPassword, value.c_str(), sizeof(receivedPassword) - 1);
            receivedPassword[sizeof(receivedPassword) - 1] = '\0';

            // Notify that credentials are ready
            if (credentialsCallback && strlen(receivedSSID) > 0) {
                credentialsCallback(receivedSSID, receivedPassword);
            }
        }
    }
};

// Characteristic callbacks for Command
class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        String value = pChar->getValue();
        if (value.length() >= 1) {
            BleCommand cmd = static_cast<BleCommand>(value[0]);
            if (commandCallback) {
                commandCallback(cmd);
            }
        }
    }
};

void ble_init(const char* deviceName) {
    BLEDevice::init(deviceName);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create main service
    BLEService* pService = pServer->createService(BLEUUID(BLE_SERVICE_UUID), 30);

    // WiFi SSID (Read + Write)
    BLECharacteristic* pSSIDChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_WIFI_SSID_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    pSSIDChar->setCallbacks(new SSIDCallbacks());

    // WiFi Password (Write only - never expose)
    BLECharacteristic* pPassChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_WIFI_PASSWORD_UUID),
        BLECharacteristic::PROPERTY_WRITE
    );
    pPassChar->setCallbacks(new PasswordCallbacks());

    // WiFi Status (Read + Notify)
    pWifiStatusChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_WIFI_STATUS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pWifiStatusChar->addDescriptor(new BLE2902());
    uint8_t initialStatus = static_cast<uint8_t>(BleWifiStatus::NotConfigured);
    pWifiStatusChar->setValue(&initialStatus, 1);

    // Device Name (Read + Write)
    BLECharacteristic* pNameChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_DEVICE_NAME_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    pNameChar->setValue(deviceName);

    // Battery Level (Read + Notify)
    pBatteryLevelChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_BATTERY_LEVEL_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pBatteryLevelChar->addDescriptor(new BLE2902());

    // Battery Voltage (Read)
    pBatteryVoltageChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_BATTERY_VOLTAGE_UUID),
        BLECharacteristic::PROPERTY_READ
    );

    // Last Update (Read)
    pLastUpdateChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_LAST_UPDATE_UUID),
        BLECharacteristic::PROPERTY_READ
    );

    // Force Refresh (Write)
    BLECharacteristic* pRefreshChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_FORCE_REFRESH_UUID),
        BLECharacteristic::PROPERTY_WRITE
    );
    pRefreshChar->setCallbacks(new CommandCallbacks());

    // Error Code (Read + Notify)
    pErrorCodeChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_ERROR_CODE_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pErrorCodeChar->addDescriptor(new BLE2902());

    // RSSI (Read)
    pRssiChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_RSSI_UUID),
        BLECharacteristic::PROPERTY_READ
    );

    // API Endpoint (Read + Write)
    BLECharacteristic* pApiChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_API_ENDPOINT_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    // Command (Write)
    BLECharacteristic* pCmdChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_COMMAND_UUID),
        BLECharacteristic::PROPERTY_WRITE
    );
    pCmdChar->setCallbacks(new CommandCallbacks());

    // Response (Read + Notify)
    pResponseChar = pService->createCharacteristic(
        BLEUUID(BLE_CHAR_RESPONSE_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pResponseChar->addDescriptor(new BLE2902());

    pService->start();

    // Configure advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
}

void ble_start_advertising() {
    BLEDevice::startAdvertising();
    advertising = true;
    advertisingStartTime = millis();
}

void ble_stop() {
    advertising = false;
    if (pServer) {
        BLEDevice::stopAdvertising();
    }
}

bool ble_is_connected() {
    return deviceConnected;
}

bool ble_has_timed_out() {
    if (!advertising) return false;
    return (millis() - advertisingStartTime) > BLE_PROV_TIMEOUT_MS;
}

void ble_loop() {
    // Currently no periodic processing needed
    // Future: could handle command queue, timeouts, etc.
}

void ble_update_wifi_status(BleWifiStatus status) {
    if (pWifiStatusChar) {
        uint8_t val = static_cast<uint8_t>(status);
        pWifiStatusChar->setValue(&val, 1);
        if (deviceConnected) {
            pWifiStatusChar->notify();
        }
    }
}

void ble_update_battery(uint8_t percentage, float voltage) {
    if (pBatteryLevelChar) {
        pBatteryLevelChar->setValue(&percentage, 1);
        if (deviceConnected) {
            pBatteryLevelChar->notify();
        }
    }
    if (pBatteryVoltageChar) {
        uint16_t voltageInt = static_cast<uint16_t>(voltage * 100);
        pBatteryVoltageChar->setValue(voltageInt);
    }
}

void ble_update_last_update(uint32_t timestamp) {
    if (pLastUpdateChar) {
        pLastUpdateChar->setValue(timestamp);
    }
}

void ble_update_error(ResultCode error) {
    if (pErrorCodeChar) {
        uint8_t val = static_cast<uint8_t>(error);
        pErrorCodeChar->setValue(&val, 1);
        if (deviceConnected) {
            pErrorCodeChar->notify();
        }
    }
}

void ble_update_rssi(int8_t rssi) {
    if (pRssiChar) {
        pRssiChar->setValue((uint8_t*)&rssi, 1);
    }
}

void ble_set_credentials_callback(BleCredentialsCallback callback) {
    credentialsCallback = callback;
}

void ble_set_command_callback(BleCommandCallback callback) {
    commandCallback = callback;
}

const char* ble_get_received_ssid() {
    return receivedSSID;
}

const char* ble_get_received_password() {
    return receivedPassword;
}

#else
// Stub implementations for native build or when BLE is disabled

void ble_init(const char* deviceName) { (void)deviceName; }
void ble_start_advertising() {}
void ble_stop() {}
bool ble_is_connected() { return false; }
bool ble_has_timed_out() { return true; }
void ble_loop() {}
void ble_update_wifi_status(BleWifiStatus status) { (void)status; }
void ble_update_battery(uint8_t percentage, float voltage) { (void)percentage; (void)voltage; }
void ble_update_last_update(uint32_t timestamp) { (void)timestamp; }
void ble_update_error(ResultCode error) { (void)error; }
void ble_update_rssi(int8_t rssi) { (void)rssi; }
void ble_set_credentials_callback(BleCredentialsCallback callback) { (void)callback; }
void ble_set_command_callback(BleCommandCallback callback) { (void)callback; }
const char* ble_get_received_ssid() { return ""; }
const char* ble_get_received_password() { return ""; }

#endif // !NATIVE_BUILD && !DISABLE_BLE_PROVISIONING
