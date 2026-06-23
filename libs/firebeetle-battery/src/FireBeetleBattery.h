/**
 * FireBeetle Battery Library
 *
 * Accurate battery level reading for DFRobot FireBeetle 2 boards.
 * Uses DFRobot's specific ADC calibration formula.
 *
 * Supported boards:
 * - FireBeetle 2 ESP32-E (GPIO 36)
 * - FireBeetle 2 ESP32-C6 (GPIO 0 / A0)
 *
 * Reference: https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6
 */

#ifndef FIREBEETLE_BATTERY_H
#define FIREBEETLE_BATTERY_H

#include <Arduino.h>

// Battery voltage thresholds for LiPo
#define BATTERY_VOLTAGE_MAX     4.2f    // Fully charged
#define BATTERY_VOLTAGE_NOMINAL 3.7f    // ~50%
#define BATTERY_VOLTAGE_MIN     3.0f    // Empty (cutoff)
#define BATTERY_VOLTAGE_LOW     3.3f    // Low warning threshold
#define BATTERY_VOLTAGE_CRITICAL 3.0f   // Critical/hibernate threshold

struct FireBeetleBatteryStatus {
    float voltage;      // Battery voltage in volts
    uint8_t percentage; // 0-100%
    bool isLow;         // Below low warning threshold
    bool isCritical;    // Below critical threshold
    bool isUsbPowered;  // True if USB power detected (battery reading may be inaccurate)
};

class FireBeetleBattery {
public:
    /**
     * Constructor
     * @param pin ADC pin connected to battery (default: auto-detect based on board)
     * @param samples Number of ADC samples to average (default: 16)
     */
    FireBeetleBattery(int pin = -1, int samples = 16);

    /**
     * Initialize the battery monitor
     * Configures ADC pin and attenuation
     */
    void begin();

    /**
     * Read battery status
     * @return FireBeetleBatteryStatus with voltage, percentage, and status flags
     */
    FireBeetleBatteryStatus read();

    /**
     * Read raw battery voltage
     * @return Battery voltage in volts
     */
    float readVoltage();

    /**
     * Read battery percentage
     * @return Percentage 0-100
     */
    uint8_t readPercentage();

    /**
     * Convert voltage to percentage
     * @param voltage Battery voltage in volts
     * @return Percentage 0-100
     */
    static uint8_t voltageToPercentage(float voltage);

    /**
     * Set custom thresholds
     * @param lowThreshold Voltage for low warning
     * @param criticalThreshold Voltage for critical/hibernate
     */
    void setThresholds(float lowThreshold, float criticalThreshold);

private:
    int _pin;
    int _samples;
    float _lowThreshold;
    float _criticalThreshold;
    bool _initialized;

    /**
     * Read averaged ADC value and convert to battery voltage
     * Uses DFRobot's calibration formula
     */
    float readVoltageInternal();
};

#endif // FIREBEETLE_BATTERY_H
