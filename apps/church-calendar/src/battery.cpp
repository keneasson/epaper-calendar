#include "battery.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
// Native build: use simple mock implementation
static float mock_voltage = 3.8f;

void battery_init() {}

int battery_voltage_to_percent(float voltage) {
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;
    return static_cast<int>((voltage - 3.0f) / 1.2f * 100.0f);
}

BatteryStatus battery_read() {
    BatteryStatus status;
    status.voltage = mock_voltage;
    status.percentage = battery_voltage_to_percent(mock_voltage);
    status.isLow = mock_voltage < BATTERY_LOW_WARN;
    status.isCritical = mock_voltage < BATTERY_CRITICAL;
    return status;
}

#else
// Hardware build: use FireBeetleBattery library with DFRobot's calibration
#include <FireBeetleBattery.h>

static FireBeetleBattery battery(BATTERY_PIN);

void battery_init() {
    battery.begin();
    battery.setThresholds(BATTERY_LOW_WARN, BATTERY_CRITICAL);
}

int battery_voltage_to_percent(float voltage) {
    return FireBeetleBattery::voltageToPercentage(voltage);
}

BatteryStatus battery_read() {
    FireBeetleBatteryStatus fbStatus = battery.read();

    BatteryStatus status;
    status.voltage = fbStatus.voltage;
    status.percentage = fbStatus.percentage;
    status.isLow = fbStatus.isLow;
    status.isCritical = fbStatus.isCritical;

    return status;
}

#endif
