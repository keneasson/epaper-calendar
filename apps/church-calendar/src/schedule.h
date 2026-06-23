#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Arduino.h>

// Maximum limits for memory management
constexpr size_t MAX_NAME_LENGTH = 48;
constexpr size_t MAX_DATE_LENGTH = 64;
constexpr size_t MAX_COLLECTION_LENGTH = 64;

// Represents the service schedule fetched from API
struct Schedule {
    char serviceDate[MAX_DATE_LENGTH];      // e.g., "Sunday, November 30, 2025"
    char presiding[MAX_NAME_LENGTH];        // Preside
    char exhorting[MAX_NAME_LENGTH];        // Exhort
    char keyboard[MAX_NAME_LENGTH];         // Organist
    char steward[MAX_NAME_LENGTH];          // Steward
    char doorkeeper[MAX_NAME_LENGTH];       // Doorkeeper
    char collection[MAX_COLLECTION_LENGTH]; // 2nd Collection (optional)
    bool hasLunch;                          // Show lunch message

    bool hasCollection() const { return collection[0] != '\0'; }

    void clear() {
        serviceDate[0] = '\0';
        presiding[0] = '\0';
        exhorting[0] = '\0';
        keyboard[0] = '\0';
        steward[0] = '\0';
        doorkeeper[0] = '\0';
        collection[0] = '\0';
        hasLunch = false;
    }
};

// Result type for operations that can fail
enum class ResultCode {
    Success,
    WifiConnectionFailed,
    WifiTimeout,
    ApiRequestFailed,
    ApiTimeout,
    JsonParseError,
    InvalidResponse,
    BatteryCritical,
    NtpSyncFailed,
    Unknown
};

// Battery status information
struct BatteryStatus {
    float voltage;      // Raw voltage reading
    int percentage;     // 0-100
    bool isLow;         // Below warning threshold
    bool isCritical;    // Below critical threshold
};

// Time information for wake scheduling
struct WakeSchedule {
    int hour;           // 0-23
    int minute;         // 0-59
    bool updateDays[7]; // Sunday=0, Monday=1, ..., Saturday=6
};

#endif // SCHEDULE_H
