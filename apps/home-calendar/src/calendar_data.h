#ifndef CALENDAR_DATA_H
#define CALENDAR_DATA_H

#include <Arduino.h>

// Maximum limits for memory management
constexpr size_t MAX_TITLE_LENGTH = 64;
constexpr size_t MAX_APPOINTMENTS = 20;
constexpr size_t MAX_MONTH_EVENTS = 31;
constexpr size_t MAX_ETAG_LENGTH = 64;

// Single appointment/event
struct Appointment {
    char id[32];                    // Event ID from Google Calendar
    char title[MAX_TITLE_LENGTH];   // Event title (truncated)
    uint32_t startTime;             // Unix timestamp
    uint32_t endTime;               // Unix timestamp
    bool allDay;                    // All-day event flag

    void clear() {
        id[0] = '\0';
        title[0] = '\0';
        startTime = 0;
        endTime = 0;
        allDay = false;
    }
};

// Abbreviated event for month grid (just day + short title)
struct MonthEvent {
    uint8_t day;                    // Day of month (1-31)
    char title[16];                 // Short title for grid display

    void clear() {
        day = 0;
        title[0] = '\0';
    }
};

// Month metadata
struct MonthInfo {
    uint16_t year;                  // e.g., 2026
    uint8_t month;                  // 1-12
    uint8_t firstDayOfWeek;         // 0=Sun, 1=Mon, ..., 6=Sat (what day the 1st falls on)
    uint8_t daysInMonth;            // 28-31
    uint8_t daysInPrevMonth;        // For showing trailing days from previous month

    void clear() {
        year = 0;
        month = 0;
        firstDayOfWeek = 0;
        daysInMonth = 0;
        daysInPrevMonth = 0;
    }
};

// API response metadata
struct ApiMeta {
    char etag[MAX_ETAG_LENGTH];     // For cache validation
    uint32_t generatedAt;           // Server timestamp

    void clear() {
        etag[0] = '\0';
        generatedAt = 0;
    }
};

// Complete calendar data from API
struct CalendarData {
    ApiMeta meta;
    Appointment appointments[MAX_APPOINTMENTS];
    uint8_t appointmentCount;
    MonthEvent monthEvents[MAX_MONTH_EVENTS];
    uint8_t monthEventCount;
    MonthInfo month;

    void clear() {
        meta.clear();
        appointmentCount = 0;
        for (int i = 0; i < MAX_APPOINTMENTS; i++) {
            appointments[i].clear();
        }
        monthEventCount = 0;
        for (int i = 0; i < MAX_MONTH_EVENTS; i++) {
            monthEvents[i].clear();
        }
        month.clear();
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
    NotModified,        // HTTP 304 - cache is valid
    CacheError,
    Unknown
};

// Battery status information
struct BatteryStatus {
    float voltage;      // Raw voltage reading
    int percentage;     // 0-100
    bool isLow;         // Below warning threshold
    bool isCritical;    // Below critical threshold
    bool isUsbPowered;  // USB power detected (battery reading may be affected)
};

#endif // CALENDAR_DATA_H
