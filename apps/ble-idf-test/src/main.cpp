/*
 * ESP32-S3 BLE Client with WiFi Scanner
 * - Waveshare ESP32-S3-LCD-1.47B
 * - QMI8658 IMU for tilt navigation
 * - BOOT button for selection
 * - Sends WiFi credentials to C6 via BLE
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <WiFi.h>
#include <Wire.h>
#include <WaveshareS3LCD147.h>
#include "config.h"  // known-network credentials (gitignored; see config_template.h)

// Use the shared library for display configuration
// See libs/waveshare-s3-lcd-147/src/WaveshareS3LCD147.h for pin definitions and notes

WaveshareS3LCD147 lcd;
LGFX_Sprite sprite(&lcd);  // Sprite buffer for flicker-free drawing
const int SCREEN_WIDTH = WS_SCREEN_WIDTH;
const int SCREEN_HEIGHT = WS_SCREEN_HEIGHT;

// ============== BLE UUIDs ==============
#define BATTERY_SERVICE_UUID        "180F"
#define BATTERY_LEVEL_CHAR_UUID     "2A19"
#define WIFI_SERVICE_UUID           "12345678-1234-1234-1234-123456789abc"
#define WIFI_SSID_CHAR_UUID         "12345678-1234-1234-1234-123456789ab1"
#define WIFI_PASS_CHAR_UUID         "12345678-1234-1234-1234-123456789ab2"
#define WIFI_STATUS_CHAR_UUID       "12345678-1234-1234-1234-123456789ab3"
#define WIFI_COMMAND_CHAR_UUID      "12345678-1234-1234-1234-123456789ab4"
#define WIFI_MESSAGE_CHAR_UUID      "12345678-1234-1234-1234-123456789ab5"

const char* TARGET_NAME = "C6-IDF-Test";

// Known-network credentials live in config.h (gitignored). Copy
// config_template.h to config.h and fill in real values.
const char* getPasswordForSSID(const String& ssid) {
    if (ssid == KNOWN_SSID_1) return KNOWN_PASSWORD_1;
    if (ssid == KNOWN_SSID_2) return KNOWN_PASSWORD_2;
    return DEFAULT_PASSWORD;
}

// ============== State Machine ==============
enum AppState {
    STATE_INIT,
    STATE_SCANNING_WIFI,
    STATE_SHOW_WIFI_LIST,
    STATE_SCANNING_BLE,
    STATE_CONNECTING_BLE,
    STATE_SENDING_CREDENTIALS,
    STATE_WAITING_WIFI_STATUS,
    STATE_CONNECTED,
    STATE_RETRY_C6,  // New: retry C6 connection without re-selecting WiFi
    STATE_ERROR
};

AppState currentState = STATE_INIT;
String statusMessage = "";

// ============== Saved WiFi Credentials ==============
// Once selected, keep these so we don't have to re-select
String savedSsid = "";
String savedPassword = "";
bool wifiCredentialsSaved = false;

// ============== WiFi Scan Results ==============
struct WifiNetwork {
    String ssid;
    int rssi;
    bool encrypted;
};
std::vector<WifiNetwork> wifiNetworks;
int selectedNetworkIndex = 0;
int displayOffset = 0;
const int MAX_VISIBLE_NETWORKS = 6;

// ============== BLE Variables ==============
static BLEAdvertisedDevice* targetDevice = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteCharacteristic* pSsidChar = nullptr;
static BLERemoteCharacteristic* pPassChar = nullptr;
static BLERemoteCharacteristic* pStatusChar = nullptr;
static BLERemoteCharacteristic* pCommandChar = nullptr;
static BLERemoteCharacteristic* pBatteryChar = nullptr;
static BLERemoteCharacteristic* pMessageChar = nullptr;
bool bleConnected = false;
int c6BatteryLevel = -1;
int c6WifiStatus = -1;
String c6Message = "";

// ============== IMU Variables ==============
float accelX = 0, accelY = 0, accelZ = 0;
float smoothAccelX = 0;
const float SMOOTH_FACTOR = 0.3;
const float ACCEL_SCALE = 4.0 / 32768.0;

// Jolt gesture detection (quick movement spike)
float prevAccelX = 0;
const float JOLT_THRESHOLD = 0.4;    // G force change to register jolt
unsigned long lastGestureTime = 0;
const unsigned long GESTURE_COOLDOWN = 400;  // ms between gestures

// ============== Button Variables ==============
bool buttonPressed = false;
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_DEBOUNCE = 200;  // ms
const unsigned long LONG_PRESS_TIME = 800;  // ms for long press
unsigned long buttonPressStart = 0;
bool imuAvailable = false;

// ============== Retry State Variables ==============
const unsigned long RETRY_TIMEOUT_MS = 10 * 60 * 1000;  // 10 minutes
unsigned long retryStartTime = 0;
int lastDisplayedCountdown = -1;  // For display optimization

// ============== Display State (to prevent flicker) ==============
bool displayNeedsUpdate = true;
int lastSelectedIndex = -1;
AppState lastDisplayedState = STATE_INIT;

// ============== QMI8658 IMU Functions ==============
void writeQMI8658(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(WS_IMU_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readQMI8658(uint8_t reg) {
    Wire.beginTransmission(WS_IMU_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(WS_IMU_ADDR, (uint8_t)1);
    return Wire.read();
}

bool initQMI8658() {
    Wire.begin(WS_IMU_PIN_SDA, WS_IMU_PIN_SCL);
    delay(10);

    // Check WHO_AM_I
    uint8_t whoami = readQMI8658(0x00);
    Serial.printf("QMI8658 WHO_AM_I: 0x%02X\n", whoami);
    if (whoami != 0x05) {
        Serial.println("QMI8658 not found!");
        return false;
    }

    // Configure (from working demo)
    writeQMI8658(0x02, 0x40);  // CTRL1: auto-increment
    writeQMI8658(0x03, 0x13);  // CTRL2: accel 4G, 1000Hz
    writeQMI8658(0x04, 0x33);  // CTRL3: gyro config
    writeQMI8658(0x08, 0x03);  // CTRL7: enable accel + gyro
    delay(50);

    Serial.println("QMI8658 initialized");
    return true;
}

void readAccelerometer() {
    Wire.beginTransmission(WS_IMU_ADDR);
    Wire.write(0x35);  // AX_L register
    Wire.endTransmission(false);
    Wire.requestFrom(WS_IMU_ADDR, (uint8_t)6);

    int16_t rawX = Wire.read() | (Wire.read() << 8);
    int16_t rawY = Wire.read() | (Wire.read() << 8);
    int16_t rawZ = Wire.read() | (Wire.read() << 8);

    accelX = rawX * ACCEL_SCALE;
    accelY = rawY * ACCEL_SCALE;
    accelZ = rawZ * ACCEL_SCALE;

    // Smooth the X value for gesture detection
    smoothAccelX = (SMOOTH_FACTOR * accelX) + ((1.0 - SMOOTH_FACTOR) * smoothAccelX);
}

// ============== Navigation Functions ==============
void navigateUp() {
    if (selectedNetworkIndex > 0) {
        selectedNetworkIndex--;
        if (selectedNetworkIndex < displayOffset) {
            displayOffset = selectedNetworkIndex;
        }
        displayNeedsUpdate = true;
        Serial.printf("Navigate up: %d\n", selectedNetworkIndex);
    }
}

void navigateDown() {
    if (selectedNetworkIndex < (int)wifiNetworks.size() - 1) {
        selectedNetworkIndex++;
        if (selectedNetworkIndex >= displayOffset + MAX_VISIBLE_NETWORKS) {
            displayOffset = selectedNetworkIndex - MAX_VISIBLE_NETWORKS + 1;
        }
        displayNeedsUpdate = true;
        Serial.printf("Navigate down: %d\n", selectedNetworkIndex);
    }
}

void handleTiltInput() {
    if (!imuAvailable) return;
    if (currentState != STATE_SHOW_WIFI_LIST) return;

    unsigned long now = millis();
    if (now - lastGestureTime < GESTURE_COOLDOWN) return;

    readAccelerometer();

    // Calculate jolt (sudden change in acceleration)
    float deltaX = smoothAccelX - prevAccelX;
    prevAccelX = smoothAccelX;

    // Debug: print jolt values every second
    static unsigned long lastDebug = 0;
    if (now - lastDebug > 1000) {
        Serial.printf("Accel X=%.2f delta=%.2f\n", smoothAccelX, deltaX);
        lastDebug = now;
    }

    // Jolt detection - quick flick in either direction
    // Positive delta = flicked toward you (screen tilting toward floor) = NEXT
    // Negative delta = flicked away from you (screen tilting toward ceiling) = PREV
    if (deltaX > JOLT_THRESHOLD) {
        navigateDown();
        lastGestureTime = now;
        Serial.printf("JOLT DOWN -> index %d\n", selectedNetworkIndex);
    } else if (deltaX < -JOLT_THRESHOLD) {
        navigateUp();
        lastGestureTime = now;
        Serial.printf("JOLT UP -> index %d\n", selectedNetworkIndex);
    }
}

void handleButtonInput() {
    bool buttonState = digitalRead(WS_BOOT_BUTTON) == LOW;

    if (buttonState && !buttonPressed) {
        // Button just pressed
        buttonPressed = true;
        buttonPressStart = millis();
    } else if (!buttonState && buttonPressed) {
        // Button just released
        buttonPressed = false;
        unsigned long pressDuration = millis() - buttonPressStart;

        if (pressDuration > BUTTON_DEBOUNCE) {
            if (currentState == STATE_SHOW_WIFI_LIST && wifiNetworks.size() > 0) {
                bool shouldSelect = imuAvailable || (pressDuration >= LONG_PRESS_TIME);

                if (shouldSelect) {
                    // Save selected credentials for retry logic
                    savedSsid = wifiNetworks[selectedNetworkIndex].ssid;
                    savedPassword = getPasswordForSSID(savedSsid);
                    wifiCredentialsSaved = true;
                    Serial.printf("Saved WiFi: %s\n", savedSsid.c_str());

                    currentState = STATE_SCANNING_BLE;
                    displayNeedsUpdate = true;
                } else {
                    // Without IMU: short press = NEXT
                    Serial.println("Short press - NEXT");
                    navigateDown();
                }
            } else if (currentState == STATE_RETRY_C6) {
                // In retry state, button press = retry immediately
                Serial.println("Button - Retry C6 now");
                currentState = STATE_SCANNING_BLE;
                displayNeedsUpdate = true;
            } else if (currentState == STATE_CONNECTED) {
                // In connected state, any press rescans WiFi
                currentState = STATE_SCANNING_WIFI;
                wifiCredentialsSaved = false;  // Clear saved credentials
                displayNeedsUpdate = true;
            }
        }
    }
}

// ============== Display Functions ==============
void displayStatus(const char* title, const char* message, uint16_t color = TFT_WHITE) {
    lcd.fillScreen(TFT_BLACK);

    // Header
    lcd.fillRect(0, 0, SCREEN_WIDTH, 28, 0x2104);
    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(TFT_CYAN, 0x2104);
    lcd.drawString(title, 10, 4);

    // Status message
    lcd.setFont(&fonts::DejaVu12);
    lcd.setTextColor(color, TFT_BLACK);
    lcd.drawString(message, 10, 80);

    // Battery info if available
    if (c6BatteryLevel >= 0) {
        lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        char battStr[20];
        sprintf(battStr, "C6 Bat: %d%%", c6BatteryLevel);
        lcd.drawString(battStr, SCREEN_WIDTH - 100, 4);
    }
}

void displayWifiList() {
    // Screen is 320x172 in landscape. Use full height.
    lcd.fillScreen(TFT_BLACK);

    // Layout constants
    const int headerHeight = 28;
    const int itemHeight = 20;
    const int headerY = 0;
    const int listStartY = headerHeight + 2;  // Below header
    const int footerY = SCREEN_HEIGHT - 18;   // Near bottom (172 - 18 = 154)
    const int maxVisible = (footerY - listStartY) / itemHeight;

    // Header bar
    lcd.fillRect(0, headerY, SCREEN_WIDTH, headerHeight, 0x2104);
    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(TFT_CYAN, 0x2104);
    lcd.drawString("Select WiFi", 10, headerY + 3);

    // Battery info in header (if available)
    if (c6BatteryLevel >= 0) {
        lcd.setTextColor(TFT_YELLOW, 0x2104);
        char battStr[20];
        sprintf(battStr, "C6:%d%%", c6BatteryLevel);
        lcd.drawString(battStr, SCREEN_WIDTH - 90, headerY + 3);
    }

    // Footer instructions
    lcd.setFont(&fonts::DejaVu12);
    lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    const char* footerText = imuAvailable ? "Tilt:Nav  BOOT:Select" : "Short:Next  Long:Select";
    lcd.drawString(footerText, 10, footerY);

    // Network list
    lcd.setFont(&fonts::DejaVu12);
    int y = listStartY;
    int visibleCount = min(maxVisible, (int)wifiNetworks.size() - displayOffset);

    for (int i = displayOffset; i < displayOffset + visibleCount; i++) {
        bool isSelected = (i == selectedNetworkIndex);

        // Background for selected item
        if (isSelected) {
            lcd.fillRect(0, y, SCREEN_WIDTH, itemHeight, TFT_NAVY);
        }

        // Selection indicator and SSID
        uint16_t bgColor = isSelected ? TFT_NAVY : TFT_BLACK;
        if (isSelected) {
            lcd.setTextColor(TFT_YELLOW, bgColor);
            lcd.drawString(">", 4, y + 3);
        }

        lcd.setTextColor(isSelected ? TFT_WHITE : TFT_LIGHTGRAY, bgColor);
        String displaySsid = wifiNetworks[i].ssid;
        if (displaySsid.length() > 16) {
            displaySsid = displaySsid.substring(0, 14) + "..";
        }
        lcd.drawString(displaySsid.c_str(), 20, y + 3);

        // RSSI indicator with color coding
        int rssi = wifiNetworks[i].rssi;
        uint16_t rssiColor = rssi > -50 ? TFT_GREEN : (rssi > -70 ? TFT_YELLOW : TFT_RED);
        lcd.setTextColor(rssiColor, bgColor);
        char rssiStr[8];
        sprintf(rssiStr, "%d", rssi);
        lcd.drawString(rssiStr, SCREEN_WIDTH - 50, y + 3);

        // Lock icon for encrypted networks
        if (wifiNetworks[i].encrypted) {
            lcd.setTextColor(TFT_ORANGE, bgColor);
            lcd.drawString("*", SCREEN_WIDTH - 60, y + 3);
        }

        y += itemHeight;
    }

    // Scroll indicators
    lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    if (displayOffset > 0) {
        lcd.drawString("^", SCREEN_WIDTH - 15, listStartY);
    }
    if (displayOffset + visibleCount < (int)wifiNetworks.size()) {
        lcd.drawString("v", SCREEN_WIDTH - 15, footerY - 14);
    }
}

void displayConnectedStatus() {
    // Screen is 320x172 in landscape. Use full height, no manual offset needed.
    lcd.fillScreen(TFT_BLACK);

    // Header at top
    lcd.fillRect(0, 0, SCREEN_WIDTH, 28, 0x0400);  // Dark green
    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(TFT_GREEN, 0x0400);
    lcd.drawString("Connected!", 10, 5);

    lcd.setFont(&fonts::DejaVu12);

    // Selected network
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString("WiFi:", 10, 38);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.drawString(wifiNetworks[selectedNetworkIndex].ssid.c_str(), 60, 38);

    // C6 Battery
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString("C6 Battery:", 10, 58);
    if (c6BatteryLevel >= 0) {
        uint16_t batColor = c6BatteryLevel > 50 ? TFT_GREEN : (c6BatteryLevel > 20 ? TFT_YELLOW : TFT_RED);
        lcd.setTextColor(batColor, TFT_BLACK);
        char batStr[10];
        sprintf(batStr, "%d%%", c6BatteryLevel);
        lcd.drawString(batStr, 110, 58);
    }

    // C6 WiFi Status
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString("C6 WiFi:", 10, 78);
    const char* wifiStatusStr;
    uint16_t statusColor;
    switch (c6WifiStatus) {
        case 0: wifiStatusStr = "Idle"; statusColor = TFT_DARKGRAY; break;
        case 1: wifiStatusStr = "Connecting..."; statusColor = TFT_YELLOW; break;
        case 2: wifiStatusStr = "Connected!"; statusColor = TFT_GREEN; break;
        case 3: wifiStatusStr = "Failed"; statusColor = TFT_RED; break;
        case 4: wifiStatusStr = "Disconnected"; statusColor = TFT_ORANGE; break;
        default: wifiStatusStr = "Unknown"; statusColor = TFT_DARKGRAY; break;
    }
    lcd.setTextColor(statusColor, TFT_BLACK);
    lcd.drawString(wifiStatusStr, 90, 78);

    // C6 Message
    if (c6Message.length() > 0) {
        lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        lcd.drawString("Msg:", 10, 100);
        lcd.setTextColor(TFT_MAGENTA, TFT_BLACK);
        lcd.drawString(c6Message.c_str(), 50, 100);
    }

    // Footer at bottom (172 height - ~15 for text = 157)
    lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    lcd.drawString("BOOT: Rescan WiFi", 10, 155);
}

void displayRetryScreen(int secondsRemaining) {
    lcd.fillScreen(TFT_BLACK);

    // Header - orange warning color
    lcd.fillRect(0, 0, SCREEN_WIDTH, 28, 0x4208);  // Dark orange
    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(TFT_ORANGE, 0x4208);
    lcd.drawString("C6 Not Found", 10, 5);

    lcd.setFont(&fonts::DejaVu12);

    // Saved WiFi info
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString("WiFi:", 10, 38);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.drawString(savedSsid.c_str(), 60, 38);

    // Status message
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.drawString("C6 may be sleeping or", 10, 65);
    lcd.drawString("out of range", 10, 82);

    // Countdown display
    int minutes = secondsRemaining / 60;
    int seconds = secondsRemaining % 60;
    char countdownStr[30];
    sprintf(countdownStr, "Auto-retry in: %d:%02d", minutes, seconds);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString(countdownStr, 10, 110);

    // Progress bar for countdown
    int barWidth = SCREEN_WIDTH - 20;
    int barProgress = barWidth - (barWidth * secondsRemaining / (RETRY_TIMEOUT_MS / 1000));
    lcd.drawRect(10, 130, barWidth, 10, TFT_DARKGRAY);
    if (barProgress > 0) {
        lcd.fillRect(11, 131, barProgress - 2, 8, TFT_ORANGE);
    }

    // Footer instructions
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.drawString("BOOT: Retry now", 10, 155);
}

// ============== WiFi Scanning ==============
void scanWifiNetworks() {
    displayStatus("WiFi Scanner", "Scanning networks...", TFT_YELLOW);
    Serial.println("Scanning WiFi networks...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    Serial.printf("Found %d networks\n", n);

    wifiNetworks.clear();
    for (int i = 0; i < n; i++) {
        WifiNetwork net;
        net.ssid = WiFi.SSID(i);
        net.rssi = WiFi.RSSI(i);
        net.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;

        Serial.printf("  [%d] SSID='%s' RSSI=%d\n", i, net.ssid.c_str(), net.rssi);

        // Skip empty SSIDs
        if (net.ssid.length() > 0) {
            wifiNetworks.push_back(net);
        }
    }
    Serial.printf("After filtering: %d networks\n", wifiNetworks.size());

    // Sort by signal strength
    std::sort(wifiNetworks.begin(), wifiNetworks.end(),
        [](const WifiNetwork& a, const WifiNetwork& b) {
            return a.rssi > b.rssi;
        });

    selectedNetworkIndex = 0;
    displayOffset = 0;

    WiFi.scanDelete();
}

// ============== BLE Functions ==============
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Match by name (in scan response) OR by service UUID (in advertisement)
        // This is more reliable because names may not be in every packet
        bool matchedByName = advertisedDevice.haveName() && advertisedDevice.getName() == TARGET_NAME;
        bool matchedByService = advertisedDevice.haveServiceUUID() &&
                                advertisedDevice.isAdvertisingService(BLEUUID(BATTERY_SERVICE_UUID));

        if (matchedByName || matchedByService) {
            const char* matchType = matchedByName ? "name" : "service UUID";
            Serial.printf("Found C6-IDF-Test (matched by %s)!\n", matchType);
            if (advertisedDevice.haveName()) {
                Serial.printf("  Device name: %s\n", advertisedDevice.getName().c_str());
            }
            if (advertisedDevice.haveServiceUUID()) {
                Serial.printf("  Has service UUID: %s\n", advertisedDevice.getServiceUUID().toString().c_str());
            }
            BLEDevice::getScan()->stop();
            targetDevice = new BLEAdvertisedDevice(advertisedDevice);
        }
    }
};

static void wifiStatusNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 0) {
        c6WifiStatus = pData[0];
        Serial.printf("C6 WiFi status notification: %d\n", c6WifiStatus);
    }
}

static void messageNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 0) {
        c6Message = String((char*)pData, length);
        Serial.printf("C6 Message notification: %s\n", c6Message.c_str());
    }
}

bool connectToC6() {
    if (targetDevice == nullptr) return false;

    displayStatus("BLE", "Connecting to C6...", TFT_YELLOW);
    Serial.println("Connecting to C6...");

    pClient = BLEDevice::createClient();

    if (!pClient->connect(targetDevice)) {
        Serial.println("Failed to connect");
        return false;
    }

    Serial.println("Connected to C6!");
    bleConnected = true;

    // Get Battery Service
    BLERemoteService* pBatteryService = pClient->getService(BLEUUID(BATTERY_SERVICE_UUID));
    if (pBatteryService) {
        pBatteryChar = pBatteryService->getCharacteristic(BLEUUID(BATTERY_LEVEL_CHAR_UUID));
        if (pBatteryChar && pBatteryChar->canRead()) {
            String value = pBatteryChar->readValue();
            if (value.length() > 0) {
                c6BatteryLevel = (int)(uint8_t)value[0];
                Serial.printf("C6 Battery: %d%%\n", c6BatteryLevel);
            }
        }
    }

    // Get WiFi Service
    BLERemoteService* pWifiService = pClient->getService(BLEUUID(WIFI_SERVICE_UUID));
    if (pWifiService == nullptr) {
        Serial.println("WiFi service not found!");
        return false;
    }

    pSsidChar = pWifiService->getCharacteristic(BLEUUID(WIFI_SSID_CHAR_UUID));
    pPassChar = pWifiService->getCharacteristic(BLEUUID(WIFI_PASS_CHAR_UUID));
    pStatusChar = pWifiService->getCharacteristic(BLEUUID(WIFI_STATUS_CHAR_UUID));
    pCommandChar = pWifiService->getCharacteristic(BLEUUID(WIFI_COMMAND_CHAR_UUID));
    pMessageChar = pWifiService->getCharacteristic(BLEUUID(WIFI_MESSAGE_CHAR_UUID));

    if (!pSsidChar || !pPassChar || !pStatusChar) {
        Serial.println("Required characteristics not found!");
        return false;
    }

    // Read initial message from C6
    if (pMessageChar && pMessageChar->canRead()) {
        c6Message = pMessageChar->readValue().c_str();
        Serial.printf("C6 Message: %s\n", c6Message.c_str());
    }

    // Subscribe to status notifications
    if (pStatusChar->canNotify()) {
        pStatusChar->registerForNotify(wifiStatusNotifyCallback);
    }

    // Subscribe to message notifications
    if (pMessageChar && pMessageChar->canNotify()) {
        pMessageChar->registerForNotify(messageNotifyCallback);
        Serial.println("Subscribed to message notifications");
    }

    return true;
}

bool sendWifiCredentials(const String& ssid, const String& password) {
    if (!pSsidChar || !pPassChar) return false;

    displayStatus("Sending", "Sending WiFi credentials...", TFT_YELLOW);
    Serial.printf("Sending SSID: %s\n", ssid.c_str());

    // Send SSID
    pSsidChar->writeValue(ssid.c_str(), ssid.length());
    delay(100);

    // Send password
    pPassChar->writeValue(password.c_str(), password.length());
    delay(100);

    Serial.println("Credentials sent!");
    return true;
}

void scanForC6() {
    displayStatus("BLE Scanner", "Looking for C6...", TFT_YELLOW);
    Serial.println("Scanning for C6-IDF-Test (by name or Battery Service UUID 0x180F)...");

    targetDevice = nullptr;
    BLEScan* pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pScan->setActiveScan(true);   // Request scan response (where name usually is)
    pScan->setInterval(100);      // Scan interval in 0.625ms units = 62.5ms
    pScan->setWindow(99);         // Scan window in 0.625ms units = 61.875ms
    pScan->start(8, true);        // 8 second BLOCKING scan (increased from 5)
    int devicesFound = pScan->getResults()->getCount();
    Serial.printf("Scan complete. Found %d BLE devices total.\n", devicesFound);
    pScan->clearResults();        // Free memory after scan
}

// ============== Setup ==============
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("  ESP32-S3 WiFi Provisioner");
    Serial.println("  Target: C6-IDF-Test");
    Serial.println("=================================\n");

    // Init button
    pinMode(WS_BOOT_BUTTON, INPUT_PULLUP);

    // Init LCD
    lcd.init();
    lcd.setRotation(1);
    lcd.setBrightness(200);

    // Turn off the NeoPixel LED on GPIO 38
    rgbLedWrite(WS_NEOPIXEL, 0, 0, 0);  // Turn off onboard NeoPixel

    displayStatus("Initializing", "Starting up...", TFT_WHITE);

    // Init IMU (optional)
    imuAvailable = initQMI8658();
    if (imuAvailable) {
        Serial.println("IMU ready - flick navigation enabled");
        Serial.println("  Flick toward you: Next");
        Serial.println("  Flick away: Previous");
        Serial.println("  BOOT button: Select");
        // Initialize smoothed and previous values
        readAccelerometer();
        smoothAccelX = accelX;
        prevAccelX = accelX;
    } else {
        Serial.println("IMU not found - using button navigation");
        Serial.println("  Short press: Next item");
        Serial.println("  Long press:  Select");
    }

    // Init BLE
    BLEDevice::init("S3-WiFi-Config");

    currentState = STATE_SCANNING_WIFI;
}

// ============== Main Loop ==============
void loop() {
    handleButtonInput();
    handleTiltInput();

    switch (currentState) {
        case STATE_SCANNING_WIFI:
            scanWifiNetworks();
            if (wifiNetworks.size() > 0) {
                currentState = STATE_SHOW_WIFI_LIST;
            } else {
                displayStatus("No Networks", "No WiFi networks found", TFT_RED);
                delay(2000);
            }
            break;

        case STATE_SHOW_WIFI_LIST:
            // Only redraw when something changed
            if (displayNeedsUpdate || lastDisplayedState != currentState) {
                Serial.printf("Drawing WiFi list, selected=%d\n", selectedNetworkIndex);
                displayWifiList();
                displayNeedsUpdate = false;
                lastDisplayedState = currentState;
            }
            delay(30);  // Small delay for gesture polling
            break;

        case STATE_SCANNING_BLE:
            scanForC6();
            if (targetDevice != nullptr) {
                currentState = STATE_CONNECTING_BLE;
            } else {
                // C6 not found - go to retry state with countdown
                Serial.println("C6 not found, entering retry state");
                retryStartTime = millis();
                lastDisplayedCountdown = -1;
                currentState = STATE_RETRY_C6;
                displayNeedsUpdate = true;
            }
            break;

        case STATE_CONNECTING_BLE:
            if (connectToC6()) {
                currentState = STATE_SENDING_CREDENTIALS;
            } else {
                // BLE connect failed - go to retry state
                Serial.println("BLE connect failed, entering retry state");
                retryStartTime = millis();
                lastDisplayedCountdown = -1;
                currentState = STATE_RETRY_C6;
                displayNeedsUpdate = true;
            }
            break;

        case STATE_SENDING_CREDENTIALS:
            // Use saved credentials (which were set when WiFi was selected)
            if (sendWifiCredentials(savedSsid, savedPassword)) {
                currentState = STATE_WAITING_WIFI_STATUS;
                displayStatus("Waiting", "C6 connecting to WiFi...", TFT_YELLOW);
            } else {
                // Send failed - go to retry state
                Serial.println("Send credentials failed, entering retry state");
                retryStartTime = millis();
                lastDisplayedCountdown = -1;
                currentState = STATE_RETRY_C6;
                displayNeedsUpdate = true;
            }
            break;

        case STATE_WAITING_WIFI_STATUS:
            // Check if BLE connection is still alive
            if (pClient && !pClient->isConnected()) {
                Serial.println("BLE connection lost while waiting!");
                bleConnected = false;
                c6BatteryLevel = -1;
                c6WifiStatus = -1;
                currentState = STATE_SCANNING_BLE;  // Rescan for C6, not WiFi
                displayNeedsUpdate = true;
                break;
            }

            // Wait for status notification
            delay(100);
            if (c6WifiStatus == 2) {  // Connected
                currentState = STATE_CONNECTED;
            } else if (c6WifiStatus == 3) {  // Failed
                displayStatus("WiFi Failed", "C6 could not connect", TFT_RED);
                delay(3000);
                currentState = STATE_SHOW_WIFI_LIST;
            }
            // Keep updating battery
            if (pBatteryChar && pBatteryChar->canRead()) {
                String value = pBatteryChar->readValue();
                if (value.length() > 0) {
                    c6BatteryLevel = (int)(uint8_t)value[0];
                }
            }
            break;

        case STATE_CONNECTED: {
            // Check if BLE connection is still alive
            if (pClient && !pClient->isConnected()) {
                Serial.println("BLE connection lost! Entering retry state...");
                bleConnected = false;
                c6BatteryLevel = -1;
                c6WifiStatus = -1;
                c6Message = "";
                pBatteryChar = nullptr;
                pSsidChar = nullptr;
                pPassChar = nullptr;
                pStatusChar = nullptr;
                pCommandChar = nullptr;
                pMessageChar = nullptr;
                if (targetDevice) {
                    delete targetDevice;
                    targetDevice = nullptr;
                }
                // Go to retry state with countdown
                retryStartTime = millis();
                lastDisplayedCountdown = -1;
                currentState = STATE_RETRY_C6;
                displayNeedsUpdate = true;
                break;
            }

            // Only redraw when state changes, battery updates, or message changes
            static bool needsRedraw = true;
            static int lastDisplayedBattery = -1;
            static int lastDisplayedWifiStatus = -1;
            static String lastDisplayedMessage = "";

            if (needsRedraw || c6BatteryLevel != lastDisplayedBattery ||
                c6WifiStatus != lastDisplayedWifiStatus || c6Message != lastDisplayedMessage) {
                displayConnectedStatus();
                lastDisplayedBattery = c6BatteryLevel;
                lastDisplayedWifiStatus = c6WifiStatus;
                lastDisplayedMessage = c6Message;
                needsRedraw = false;
            }

            // Periodically update battery and message
            static unsigned long lastBatteryRead = 0;
            if (millis() - lastBatteryRead > 5000) {
                if (pBatteryChar && pBatteryChar->canRead()) {
                    String value = pBatteryChar->readValue();
                    if (value.length() > 0) {
                        c6BatteryLevel = (int)(uint8_t)value[0];
                    }
                }
                if (pMessageChar && pMessageChar->canRead()) {
                    c6Message = pMessageChar->readValue().c_str();
                }
                lastBatteryRead = millis();
            }

            delay(50);
            break;
        }

        case STATE_RETRY_C6: {
            // Calculate remaining time
            unsigned long elapsed = millis() - retryStartTime;
            int secondsRemaining = (RETRY_TIMEOUT_MS - elapsed) / 1000;

            if (secondsRemaining <= 0) {
                // Timer expired - auto retry
                Serial.println("Retry timer expired, scanning for C6...");
                currentState = STATE_SCANNING_BLE;
                displayNeedsUpdate = true;
                break;
            }

            // Only update display when countdown changes (every second)
            if (secondsRemaining != lastDisplayedCountdown) {
                displayRetryScreen(secondsRemaining);
                lastDisplayedCountdown = secondsRemaining;
            }

            delay(100);  // Check button frequently
            break;
        }

        case STATE_ERROR:
            displayStatus("Error", statusMessage.c_str(), TFT_RED);
            delay(3000);
            currentState = STATE_SCANNING_WIFI;
            break;
    }
}
