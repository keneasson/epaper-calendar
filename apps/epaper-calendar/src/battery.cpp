#include "battery.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#else
#include <Arduino.h>
#endif

// Number of ADC samples to average for noise reduction
static constexpr int ADC_SAMPLES = 16;

// ADC characteristics
static constexpr float ADC_MAX = 4095.0f;
static constexpr float ADC_REF_VOLTAGE = 3.3f;

void battery_init() {
    pinMode(BATTERY_PIN, INPUT);
    // Allow ADC to stabilize
    analogRead(BATTERY_PIN);
}

static float read_voltage_averaged() {
    uint32_t sum = 0;

    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(BATTERY_PIN);
        delayMicroseconds(100);  // Brief delay between samples
    }

    float average = static_cast<float>(sum) / ADC_SAMPLES;
    float voltage = (average / ADC_MAX) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER;

    return voltage;
}

int battery_voltage_to_percent(float voltage) {
    // LiPo voltage curve approximation:
    // 4.2V = 100% (fully charged)
    // 3.7V = ~50% (nominal)
    // 3.0V = 0% (empty, cutoff)

    constexpr float V_MAX = 4.2f;
    constexpr float V_MIN = 3.0f;

    if (voltage >= V_MAX) return 100;
    if (voltage <= V_MIN) return 0;

    return static_cast<int>((voltage - V_MIN) / (V_MAX - V_MIN) * 100.0f);
}

BatteryStatus battery_read() {
    BatteryStatus status;

    status.voltage = read_voltage_averaged();
    status.percentage = battery_voltage_to_percent(status.voltage);
    status.isLow = status.voltage < BATTERY_LOW_WARN;
    status.isCritical = status.voltage < BATTERY_CRITICAL;

    return status;
}
