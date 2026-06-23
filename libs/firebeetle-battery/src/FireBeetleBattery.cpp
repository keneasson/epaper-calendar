/**
 * FireBeetle Battery Library
 *
 * Uses DFRobot's specific ADC calibration formula for accurate readings.
 * Formula: batteryMillivolts = (adcMillivolts * 2.1218) + 1000
 *
 * This accounts for the internal voltage divider on FireBeetle boards.
 */

#include "FireBeetleBattery.h"

// Voltage divider calibration constants
// The FireBeetle 2 ESP32-C6 uses a 1:1 voltage divider (two equal resistors)
// So battery voltage = ADC voltage × 2
// Reference: https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6
//
// Note: When USB is connected, the battery reading may be affected by the
// charging circuit. The device shows "USB" instead of battery percentage
// when USB power is detected.
static constexpr float DFROBOT_SCALE = 2.0f;
static constexpr float DFROBOT_OFFSET = 0.0f;

// ADC characteristics (12-bit, 3.3V reference with 11dB attenuation)
static constexpr float ADC_MAX = 4095.0f;
static constexpr float ADC_REF_MV = 3300.0f;

// Default pins for each FireBeetle variant
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32_C6_BUILD)
    static constexpr int DEFAULT_BATTERY_PIN = 0;   // GPIO0 (A0) on C6
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    static constexpr int DEFAULT_BATTERY_PIN = 1;   // Check S3 variant
#else
    static constexpr int DEFAULT_BATTERY_PIN = 36;  // GPIO36 on ESP32-E
#endif


FireBeetleBattery::FireBeetleBattery(int pin, int samples)
    : _pin(pin)
    , _samples(samples)
    , _lowThreshold(BATTERY_VOLTAGE_LOW)
    , _criticalThreshold(BATTERY_VOLTAGE_CRITICAL)
    , _initialized(false)
{
    // Auto-detect pin if not specified
    if (_pin < 0) {
        _pin = DEFAULT_BATTERY_PIN;
    }
}

void FireBeetleBattery::begin() {
    pinMode(_pin, INPUT);

    // Configure ADC for full 0-3.3V range (11dB attenuation)
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32_C6_BUILD)
    analogSetPinAttenuation(_pin, ADC_11db);
#elif defined(ESP32)
    // ESP32-E uses analogSetAttenuation for global setting
    analogSetAttenuation(ADC_11db);
#endif

    // Perform a dummy read to stabilize ADC
    analogRead(_pin);
    delay(10);

    _initialized = true;
}

float FireBeetleBattery::readVoltageInternal() {
    if (!_initialized) {
        begin();
    }

    // Average multiple samples for noise reduction
    uint32_t sum = 0;
    for (int i = 0; i < _samples; i++) {
        sum += analogRead(_pin);
        delayMicroseconds(100);
    }
    uint32_t avgReading = sum / _samples;

    // Convert ADC reading to millivolts
    float adcMillivolts = (avgReading / ADC_MAX) * ADC_REF_MV;

    // Apply DFRobot calibration formula
    // This formula accounts for the internal voltage divider
    float batteryMillivolts = (adcMillivolts * DFROBOT_SCALE) + DFROBOT_OFFSET;

    // Convert to volts
    return batteryMillivolts / 1000.0f;
}

float FireBeetleBattery::readVoltage() {
    return readVoltageInternal();
}

uint8_t FireBeetleBattery::readPercentage() {
    return voltageToPercentage(readVoltageInternal());
}

uint8_t FireBeetleBattery::voltageToPercentage(float voltage) {
    // Linear mapping: 3.0V = 0%, 4.2V = 100%
    if (voltage >= BATTERY_VOLTAGE_MAX) return 100;
    if (voltage <= BATTERY_VOLTAGE_MIN) return 0;

    float percentage = (voltage - BATTERY_VOLTAGE_MIN) /
                       (BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN) * 100.0f;

    return static_cast<uint8_t>(percentage);
}

// Check if USB power is connected (affects battery readings)
static bool isUsbConnected() {
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ESP32_C6_BUILD)
    // On ESP32-C6 with USB-CDC, check if Serial is connected via USB
    // This is a heuristic - when USB is connected, battery ADC may be inaccurate
    return Serial && Serial.availableForWrite() > 0;
#else
    return false;
#endif
}

FireBeetleBatteryStatus FireBeetleBattery::read() {
    FireBeetleBatteryStatus status;

    status.voltage = readVoltageInternal();
    status.isUsbPowered = isUsbConnected();

    // When USB is connected and voltage reads low, the charging circuit
    // may be affecting the ADC. In this case, assume battery is OK.
    if (status.isUsbPowered && status.voltage < BATTERY_VOLTAGE_LOW) {
        // Battery reading is likely affected by USB/charging circuit
        // Report as charging/full instead of low
        status.voltage = BATTERY_VOLTAGE_MAX;  // Assume full when on USB
        status.percentage = 100;
        status.isLow = false;
        status.isCritical = false;
    } else {
        status.percentage = voltageToPercentage(status.voltage);
        status.isLow = status.voltage < _lowThreshold;
        status.isCritical = status.voltage < _criticalThreshold;
    }

    return status;
}

void FireBeetleBattery::setThresholds(float lowThreshold, float criticalThreshold) {
    _lowThreshold = lowThreshold;
    _criticalThreshold = criticalThreshold;
}
