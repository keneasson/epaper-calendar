#ifndef BATTERY_H
#define BATTERY_H

#include "schedule.h"

// Initialize battery monitoring (configure ADC pin)
void battery_init();

// Read current battery status with averaged samples
BatteryStatus battery_read();

// Convert raw voltage to percentage (0-100)
int battery_voltage_to_percent(float voltage);

#endif // BATTERY_H
