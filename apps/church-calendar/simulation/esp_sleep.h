/*
 * ESP32 Sleep API Simulation for Native Builds
 */

#ifndef ESP_SLEEP_SIM_H
#define ESP_SLEEP_SIM_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

inline void esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
    printf("[SIM] Sleep timer set for %llu microseconds (%.1f hours)\n",
           (unsigned long long)time_in_us,
           time_in_us / 1000000.0 / 3600.0);
}

inline void esp_deep_sleep_start() {
    printf("[SIM] Entering deep sleep (simulation exit)\n");
    printf("\n=== SIMULATION COMPLETE ===\n");
    exit(0);
}

#endif // ESP_SLEEP_SIM_H
