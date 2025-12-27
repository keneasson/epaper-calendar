/*
 * Time API Simulation for Native Builds
 * Uses system time instead of NTP.
 */

#ifndef TIME_SIM_H
#define TIME_SIM_H

#include <ctime>
#include <cstdio>

// Simulated timezone configuration
inline void configTime(long gmtOffset_sec, int daylightOffset_sec, const char* server) {
    printf("[SIM] Time config: GMT offset=%ld, DST=%d, server=%s\n",
           gmtOffset_sec, daylightOffset_sec, server);
    printf("[SIM] Using system time instead of NTP\n");
}

// Get local time (uses system time)
inline bool getLocalTime(struct tm* info) {
    time_t now = time(nullptr);
    struct tm* local = localtime(&now);
    if (local) {
        *info = *local;
        return true;
    }
    return false;
}

#endif // TIME_SIM_H
