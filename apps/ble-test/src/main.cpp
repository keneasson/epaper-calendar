/*
 * ESP32-C6 BLE Server with WiFi Provisioning
 * - Periodic wake from deep sleep
 * - Battery Service (0x180F)
 * - WiFi Config Service (custom)
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <WiFi.h>
#include "esp_sleep.h"

// ============== Configuration ==============
#define DEBUG_MODE 1                  // Set to 0 to enable deep sleep, 1 to stay awake for debugging
#define LED_PIN 15                    // Onboard LED on FireBeetle C6
#define BLE_ADVERTISE_TIME_SEC 30     // How long to advertise before sleeping
#define DEEP_SLEEP_TIME_SEC 300       // 5 minutes between wake-ups
#define IDLE_TIMEOUT_SEC 60           // Sleep after 60s of no activity when connected

// ============== Service UUIDs ==============
#define BATTERY_SERVICE_UUID        "180F"
#define BATTERY_LEVEL_CHAR_UUID     "2A19"

// Custom WiFi Config Service
#define WIFI_SERVICE_UUID           "12345678-1234-1234-1234-123456789abc"
#define WIFI_SSID_CHAR_UUID         "12345678-1234-1234-1234-123456789ab1"
#define WIFI_PASS_CHAR_UUID         "12345678-1234-1234-1234-123456789ab2"
#define WIFI_STATUS_CHAR_UUID       "12345678-1234-1234-1234-123456789ab3"
#define WIFI_COMMAND_CHAR_UUID      "12345678-1234-1234-1234-123456789ab4"
#define WIFI_MESSAGE_CHAR_UUID      "12345678-1234-1234-1234-123456789ab5"

// ============== Global Variables ==============
BLEServer* pServer = nullptr;
BLECharacteristic* pBatteryChar = nullptr;
BLECharacteristic* pSsidChar = nullptr;
BLECharacteristic* pPassChar = nullptr;
BLECharacteristic* pStatusChar = nullptr;
BLECharacteristic* pCommandChar = nullptr;
BLECharacteristic* pMessageChar = nullptr;

// Wake counter for unique messages
RTC_DATA_ATTR int wakeCount = 0;

bool deviceConnected = false;
bool wasConnected = false;
unsigned long lastActivityTime = 0;
unsigned long advertiseStartTime = 0;

String wifiSsid = "";
String wifiPass = "";
bool shouldConnectWifi = false;
bool shouldSleep = false;

// WiFi status codes
enum WifiStatus {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING = 1,
    WIFI_STATUS_CONNECTED = 2,
    WIFI_STATUS_FAILED = 3,
    WIFI_STATUS_DISCONNECTED = 4
};

// Forward declarations
void updateWifiStatus(WifiStatus status);
void goToDeepSleep();

// ============== Battery Reading ==============
// FireBeetle C6: Battery voltage on GPIO0 (A0) with voltage divider
// The board uses a 1:1 voltage divider (two equal resistors)
// So battery voltage = ADC voltage × 2
// Reference: https://github.com/orgs/micropython/discussions/17043
uint8_t getBatteryLevel() {
    const int batteryPin = 0;  // GPIO0 (A0) for battery on FireBeetle C6

    // Configure ADC for full range
    analogSetAttenuation(ADC_11db);  // 0-3.3V range

    // Take multiple readings for accuracy
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(batteryPin);
    }
    uint32_t reading = sum / 16;

    // Convert to millivolts
    // ADC is 12-bit (0-4095), reference 3.3V
    float adcMillivolts = (reading / 4095.0) * 3300.0;

    // Corrected formula: simple 2x multiplier for 1:1 voltage divider
    // OLD (wrong): battery_mV = (adc_mV * 2.1218) + 1000
    // NEW (correct): battery_mV = adc_mV * 2.0
    float batteryMillivolts = adcMillivolts * 2.0;
    float batteryVoltage = batteryMillivolts / 1000.0;

    // Convert voltage to percentage (3.0V = 0%, 4.2V = 100%)
    int percent = (int)((batteryVoltage - 3.0) / (4.2 - 3.0) * 100);
    percent = constrain(percent, 0, 100);

    Serial.printf("Battery: ADC=%lu, ADC_mV=%.0f, Bat=%.2fV = %d%%\n",
                  reading, adcMillivolts, batteryVoltage, percent);
    return (uint8_t)percent;
}

// ============== Deep Sleep ==============
void goToDeepSleep() {
#if DEBUG_MODE
    Serial.println("[DEBUG] Sleep skipped - DEBUG_MODE enabled");
    Serial.println("[DEBUG] Restarting BLE advertising...");
    shouldSleep = false;
    wasConnected = false;
    deviceConnected = false;
    lastActivityTime = millis();  // Reset to prevent immediate re-trigger
    advertiseStartTime = millis();
    BLEDevice::startAdvertising();
    return;
#else
    Serial.printf("Going to deep sleep for %d seconds...\n", DEEP_SLEEP_TIME_SEC);
    Serial.flush();

    // Turn off LED
    digitalWrite(LED_PIN, LOW);

    // Disconnect WiFi if connected
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Stop BLE
    BLEDevice::deinit(true);

    // Configure wake-up timer
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_SEC * 1000000ULL);

    // Enter deep sleep
    esp_deep_sleep_start();
#endif
}

// ============== BLE Callbacks ==============
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        wasConnected = true;
        lastActivityTime = millis();
        Serial.println("BLE: Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE: Client disconnected");
        // After disconnect, schedule sleep soon
        shouldSleep = true;
    }
};

class WifiSsidCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        lastActivityTime = millis();
        String value = pCharacteristic->getValue();
        wifiSsid = value.c_str();
        Serial.printf("Received SSID: %s\n", wifiSsid.c_str());
    }
};

class WifiPassCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        lastActivityTime = millis();
        String value = pCharacteristic->getValue();
        wifiPass = value.c_str();
        Serial.printf("Received Password: %s\n", wifiPass.length() > 0 ? "****" : "(empty)");

        if (wifiSsid.length() > 0 && wifiPass.length() > 0) {
            shouldConnectWifi = true;
            Serial.println("WiFi credentials complete, will connect...");
        }
    }
};

class WifiCommandCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        lastActivityTime = millis();
        String value = pCharacteristic->getValue();
        if (value.length() > 0) {
            uint8_t cmd = value[0];
            Serial.printf("Received command: %d\n", cmd);

            switch (cmd) {
                case 1:  // Connect to WiFi
                    if (wifiSsid.length() > 0) {
                        shouldConnectWifi = true;
                    }
                    break;
                case 2:  // Disconnect WiFi
                    WiFi.disconnect();
                    updateWifiStatus(WIFI_STATUS_DISCONNECTED);
                    break;
                case 3:  // Refresh display (for future use)
                    Serial.println("Refresh command received");
                    break;
                case 4:  // Sleep now
                    Serial.println("Sleep command received");
                    shouldSleep = true;
                    break;
            }
        }
    }
};

// ============== Helper Functions ==============
void updateWifiStatus(WifiStatus status) {
    if (pStatusChar) {
        uint8_t statusByte = (uint8_t)status;
        pStatusChar->setValue(&statusByte, 1);
        if (deviceConnected) {
            pStatusChar->notify();
        }
    }
    Serial.printf("WiFi status updated: %d\n", status);
}

void updateBatteryLevel() {
    if (pBatteryChar) {
        uint8_t level = getBatteryLevel();
        pBatteryChar->setValue(&level, 1);
    }
}

void updateMessage() {
    if (pMessageChar) {
        wakeCount++;
        char msg[32];
        unsigned long uptime = millis() / 1000;
        snprintf(msg, sizeof(msg), "Wake #%d, up %lus", wakeCount, uptime);
        pMessageChar->setValue(msg);
        Serial.printf("Message: %s\n", msg);
    }
}

void connectToWifi() {
    if (wifiSsid.length() == 0) {
        Serial.println("No SSID set");
        updateWifiStatus(WIFI_STATUS_FAILED);
        return;
    }

    Serial.printf("Connecting to WiFi: %s\n", wifiSsid.c_str());
    updateWifiStatus(WIFI_STATUS_CONNECTING);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        updateWifiStatus(WIFI_STATUS_CONNECTED);
        lastActivityTime = millis();
    } else {
        Serial.println("Failed to connect");
        updateWifiStatus(WIFI_STATUS_FAILED);
    }
}

// ============== Setup ==============
void setup() {
    Serial.begin(115200);

    // Wait for serial monitor to connect
    delay(2000);

    Serial.println("\n\n*** C6 STARTING ***\n");

    // Ensure WiFi is in clean state on reset
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // Turn off the onboard LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Check wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    Serial.println("\n\n=================================");
    Serial.println("  ESP32-C6 BLE WiFi Server");
    Serial.println("  Periodic Wake Mode");
#if DEBUG_MODE
    Serial.println("  *** DEBUG MODE - NO SLEEP ***");
#endif
    Serial.println("=================================");

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("  Woke from: TIMER");
    } else {
        Serial.println("  Woke from: POWER ON/RESET");
    }
    Serial.println();

    // Test battery reading BEFORE BLE init
    Serial.println("Testing battery ADC...");
    Serial.flush();
    uint8_t testBat = getBatteryLevel();
    Serial.printf("Initial battery: %d%%\n", testBat);
    Serial.flush();

    // Initialize BLE
    BLEDevice::init("C6-IDF-Test");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // -------- Battery Service --------
    BLEService* pBatteryService = pServer->createService(BATTERY_SERVICE_UUID);
    pBatteryChar = pBatteryService->createCharacteristic(
        BATTERY_LEVEL_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    uint8_t batteryLevel = getBatteryLevel();
    pBatteryChar->setValue(&batteryLevel, 1);
    pBatteryService->start();

    // -------- WiFi Config Service --------
    BLEService* pWifiService = pServer->createService(WIFI_SERVICE_UUID);

    pSsidChar = pWifiService->createCharacteristic(
        WIFI_SSID_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pSsidChar->setCallbacks(new WifiSsidCallback());

    pPassChar = pWifiService->createCharacteristic(
        WIFI_PASS_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pPassChar->setCallbacks(new WifiPassCallback());

    pStatusChar = pWifiService->createCharacteristic(
        WIFI_STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    uint8_t initialStatus = WIFI_STATUS_IDLE;
    pStatusChar->setValue(&initialStatus, 1);

    pCommandChar = pWifiService->createCharacteristic(
        WIFI_COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCommandChar->setCallbacks(new WifiCommandCallback());

    pMessageChar = pWifiService->createCharacteristic(
        WIFI_MESSAGE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    // Set initial message
    wakeCount++;
    char initMsg[32];
    snprintf(initMsg, sizeof(initMsg), "Wake #%d", wakeCount);
    pMessageChar->setValue(initMsg);
    Serial.printf("Initial message: %s\n", initMsg);

    pWifiService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
    pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    BLEDevice::startAdvertising();

    advertiseStartTime = millis();
    lastActivityTime = millis();

    Serial.printf("BLE advertising for %d seconds...\n", BLE_ADVERTISE_TIME_SEC);
    Serial.println("Waiting for S3 connection...\n");
}

// ============== Main Loop ==============
void loop() {
    unsigned long now = millis();

    // Handle WiFi connection request
    if (shouldConnectWifi) {
        shouldConnectWifi = false;
        connectToWifi();
        now = millis();  // Refresh 'now' after blocking WiFi connection
    }

    // Handle sleep request
    if (shouldSleep) {
        delay(500);  // Give BLE time to send final responses
        goToDeepSleep();
    }

    // Check if we should sleep due to no connection during advertise window
    if (!wasConnected && (now - advertiseStartTime > BLE_ADVERTISE_TIME_SEC * 1000)) {
        Serial.println("No connection during advertise window.");
        goToDeepSleep();
    }

    // Check for idle timeout when connected
    if (wasConnected && !deviceConnected && (now - lastActivityTime > 5000)) {
        // Disconnected and idle for 5 seconds - sleep
        Serial.println("Client disconnected, going to sleep.");
        goToDeepSleep();
    }

    // If connected but idle too long, sleep
    if (deviceConnected && (now - lastActivityTime > IDLE_TIMEOUT_SEC * 1000)) {
        Serial.println("Idle timeout, going to sleep.");
        goToDeepSleep();
    }

    // Restart advertising if disconnected but was connected
    static bool advertisingRestarted = false;
    if (!deviceConnected && wasConnected && !advertisingRestarted) {
        delay(500);
        BLEDevice::startAdvertising();
        Serial.println("Restarting advertising briefly...");
        advertisingRestarted = true;
    }

    // Periodically update battery level (every 30 seconds)
    static unsigned long lastBatteryUpdate = 0;
    if (now - lastBatteryUpdate > 30000) {
        updateBatteryLevel();
        lastBatteryUpdate = now;
    }

    // Periodically update message (every 3 minutes for testing)
    static unsigned long lastMessageUpdate = 0;
    if (now - lastMessageUpdate > 180000) {  // 3 minutes
        updateMessage();
        if (deviceConnected && pMessageChar) {
            pMessageChar->notify();  // Push update to S3
        }
        lastMessageUpdate = now;
    }

    delay(100);
}
