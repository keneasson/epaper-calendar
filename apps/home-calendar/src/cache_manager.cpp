#include "cache_manager.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#else
#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#endif

#include <ArduinoJson.h>

#if DEBUG_MODE
#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// RTC memory for cache timestamp (survives deep sleep)
RTC_DATA_ATTR uint32_t cacheTimestamp = 0;

// Flag to track if SPIFFS is initialized
static bool spiffsInitialized = false;

// Helper to safely copy strings
static void safe_copy(char* dest, const char* src, size_t maxLen) {
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}

ResultCode cache_init() {
#ifdef NATIVE_BUILD
    LOG("Cache: Native build - SPIFFS disabled");
    return ResultCode::Success;
#else
    if (spiffsInitialized) {
        return ResultCode::Success;
    }

    LOG("Cache: Initializing SPIFFS...");

    // true = format on failure
    if (!SPIFFS.begin(true)) {
        LOG("Cache: SPIFFS mount failed!");
        return ResultCode::CacheError;
    }

    spiffsInitialized = true;

    // Log filesystem info
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    LOGF("Cache: SPIFFS mounted - Total: %u, Used: %u, Free: %u\n",
         total, used, total - used);

    return ResultCode::Success;
#endif
}

ResultCode cache_save(const char* json, size_t len) {
#ifdef NATIVE_BUILD
    LOG("Cache: Native build - save skipped");
    return ResultCode::Success;
#else
    if (!spiffsInitialized) {
        LOG("Cache: SPIFFS not initialized");
        return ResultCode::CacheError;
    }

    LOGF("Cache: Saving %u bytes to %s\n", len, CACHE_FILE_PATH);

    File file = SPIFFS.open(CACHE_FILE_PATH, FILE_WRITE);
    if (!file) {
        LOG("Cache: Failed to open file for writing");
        return ResultCode::CacheError;
    }

    size_t written = file.print(json);
    file.close();

    if (written != len) {
        LOGF("Cache: Write incomplete - expected %u, wrote %u\n", len, written);
        return ResultCode::CacheError;
    }

    LOG("Cache: Save successful");
    return ResultCode::Success;
#endif
}

// Parse JSON into CalendarData (same logic as api_client.cpp)
static ResultCode parse_cached_json(const char* json, CalendarData& data) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);

    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        LOGF("Cache: JSON parse error: %s\n", error.c_str());
        return ResultCode::JsonParseError;
    }

    data.clear();

    // Parse metadata
    JsonObject meta = doc["meta"];
    if (meta) {
        safe_copy(data.meta.etag, meta["etag"] | "", MAX_ETAG_LENGTH);
        data.meta.generatedAt = meta["generatedAt"] | 0;
    }

    // Parse appointments array
    JsonArray appointments = doc["appointments"];
    data.appointmentCount = 0;
    for (JsonObject apt : appointments) {
        if (data.appointmentCount >= MAX_APPOINTMENTS) break;

        Appointment& a = data.appointments[data.appointmentCount];
        safe_copy(a.id, apt["id"] | "", sizeof(a.id));
        safe_copy(a.title, apt["title"] | "", MAX_TITLE_LENGTH);
        a.startTime = apt["start"] | 0;
        a.endTime = apt["end"] | 0;
        a.allDay = apt["allDay"] | false;
        data.appointmentCount++;
    }

    // Parse month events array
    JsonArray monthEvents = doc["monthEvents"];
    data.monthEventCount = 0;
    for (JsonObject evt : monthEvents) {
        if (data.monthEventCount >= MAX_MONTH_EVENTS) break;

        MonthEvent& e = data.monthEvents[data.monthEventCount];
        e.day = evt["day"] | 0;
        safe_copy(e.title, evt["title"] | "", sizeof(e.title));
        data.monthEventCount++;
    }

    // Parse month info
    JsonObject month = doc["month"];
    if (month) {
        data.month.year = month["year"] | 2026;
        data.month.month = month["month"] | 1;
        data.month.firstDayOfWeek = month["firstDayOfWeek"] | 0;
        data.month.daysInMonth = month["daysInMonth"] | 31;
        data.month.daysInPrevMonth = month["daysInPrevMonth"] | 31;
    }

    LOGF("Cache: Parsed %d appointments, %d month events\n",
         data.appointmentCount, data.monthEventCount);

    return ResultCode::Success;
}

ResultCode cache_load(CalendarData& data) {
#ifdef NATIVE_BUILD
    LOG("Cache: Native build - load skipped");
    return ResultCode::CacheError;
#else
    if (!spiffsInitialized) {
        LOG("Cache: SPIFFS not initialized");
        return ResultCode::CacheError;
    }

    if (!SPIFFS.exists(CACHE_FILE_PATH)) {
        LOG("Cache: No cache file found");
        return ResultCode::CacheError;
    }

    File file = SPIFFS.open(CACHE_FILE_PATH, FILE_READ);
    if (!file) {
        LOG("Cache: Failed to open cache file");
        return ResultCode::CacheError;
    }

    size_t fileSize = file.size();
    LOGF("Cache: Loading %u bytes from %s\n", fileSize, CACHE_FILE_PATH);

    if (fileSize > JSON_BUFFER_SIZE) {
        LOG("Cache: File too large");
        file.close();
        return ResultCode::CacheError;
    }

    // Read entire file into buffer
    String json = file.readString();
    file.close();

    if (json.length() == 0) {
        LOG("Cache: Empty cache file");
        return ResultCode::CacheError;
    }

    return parse_cached_json(json.c_str(), data);
#endif
}

bool cache_exists() {
#ifdef NATIVE_BUILD
    return false;
#else
    if (!spiffsInitialized) {
        return false;
    }
    return SPIFFS.exists(CACHE_FILE_PATH);
#endif
}

uint32_t cache_get_age() {
    if (cacheTimestamp == 0) {
        return UINT32_MAX;  // No valid timestamp
    }

    time_t now = time(nullptr);
    if (now < 1700000000) {
        // Time not synced (year < 2023)
        return 0;  // Assume fresh if we can't determine age
    }

    return (uint32_t)(now - cacheTimestamp);
}

void cache_update_timestamp() {
    time_t now = time(nullptr);
    if (now > 1700000000) {
        cacheTimestamp = (uint32_t)now;
        LOGF("Cache: Updated timestamp to %u\n", cacheTimestamp);
    } else {
        LOG("Cache: Time not synced, timestamp not updated");
    }
}

CacheStatus cache_get_status() {
    uint32_t age = cache_get_age();

    if (age == UINT32_MAX || !cache_exists()) {
        return CacheStatus::None;
    }

    if (age < CACHE_MAX_AGE_SECONDS) {
        return CacheStatus::Fresh;
    }

    return CacheStatus::Stale;
}

void cache_clear() {
#ifdef NATIVE_BUILD
    LOG("Cache: Native build - clear skipped");
#else
    if (!spiffsInitialized) {
        return;
    }

    if (SPIFFS.exists(CACHE_FILE_PATH)) {
        SPIFFS.remove(CACHE_FILE_PATH);
        LOG("Cache: Cleared");
    }

    cacheTimestamp = 0;
#endif
}
