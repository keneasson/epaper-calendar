#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "calendar_data.h"

// Forward declaration to avoid circular includes
enum class ResultCode;

// Cache status for display indicator
enum class CacheStatus {
    None,       // No cache used (fresh data)
    Fresh,      // Using cache, age < 24h
    Stale       // Using cache, age > 24h
};

// Initialize SPIFFS filesystem
// Returns Success or CacheError
ResultCode cache_init();

// Save raw JSON response to SPIFFS
// json: the raw JSON string from API response
// len: length of JSON string
ResultCode cache_save(const char* json, size_t len);

// Load and parse cached JSON into CalendarData
// data: output CalendarData structure
// Returns Success, CacheError, or JsonParseError
ResultCode cache_load(CalendarData& data);

// Check if cache file exists
bool cache_exists();

// Get cache age in seconds (based on RTC timestamp)
// Returns 0 if no valid timestamp
uint32_t cache_get_age();

// Update cache timestamp (call after successful save)
void cache_update_timestamp();

// Get cache status based on age
CacheStatus cache_get_status();

// Clear cache file
void cache_clear();

#endif // CACHE_MANAGER_H
