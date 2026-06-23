#include "ble_battery.h"

#if !defined(NATIVE_BUILD)

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertisedDevice.h>

// Standard Battery Service UUID
#define BATTERY_SERVICE_UUID        "180F"
#define BATTERY_LEVEL_CHAR_UUID     "2A19"

static BLEServer* pServer = nullptr;
static BLECharacteristic* pBatteryLevelChar = nullptr;
static bool deviceConnected = false;

class BatteryServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        deviceConnected = true;
        Serial.println("BLE: Client connected!");
    }

    void onDisconnect(BLEServer* server) override {
        deviceConnected = false;
        Serial.println("BLE: Client disconnected");
        // Restart advertising after a short delay
        delay(500);
        BLEDevice::startAdvertising();
    }
};

void ble_battery_init(const char* deviceName) {
    Serial.println("BLE: Initializing...");

    BLEDevice::init(deviceName);

    // Set MTU
    BLEDevice::setMTU(256);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BatteryServerCallbacks());

    // Create Battery Service (standard UUID 0x180F)
    BLEService* pService = pServer->createService(BATTERY_SERVICE_UUID);

    // Battery Level (standard UUID 0x2A19) - Read + Notify
    pBatteryLevelChar = pService->createCharacteristic(
        BATTERY_LEVEL_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );

    uint8_t initialLevel = 50;
    pBatteryLevelChar->setValue(&initialLevel, 1);

    pService->start();
    Serial.println("BLE: Service started");
}

void ble_battery_start() {
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    // Set advertisement data with name
    BLEAdvertisementData advData;
    advData.setFlags(0x06);  // LE General Discoverable, BR/EDR not supported
    advData.setCompleteServices(BLEUUID(BATTERY_SERVICE_UUID));
    advData.setName("BattTest");
    pAdvertising->setAdvertisementData(advData);

    BLEDevice::startAdvertising();
    Serial.println("BLE: Advertising started as 'BattTest'");
}

void ble_battery_update(uint8_t percentage, float voltage) {
    (void)voltage;

    if (pBatteryLevelChar) {
        pBatteryLevelChar->setValue(&percentage, 1);
        if (deviceConnected) {
            pBatteryLevelChar->notify();
        }
    }
}

bool ble_battery_connected() {
    return deviceConnected;
}

#else
// Stubs for native build
void ble_battery_init(const char* deviceName) { (void)deviceName; }
void ble_battery_start() {}
void ble_battery_update(uint8_t percentage, float voltage) { (void)percentage; (void)voltage; }
bool ble_battery_connected() { return false; }
#endif
