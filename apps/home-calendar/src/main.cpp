/*
 * Home Calendar - Google Calendar E-Paper Display
 *
 * Displays upcoming appointments and monthly calendar on
 * a 7.5" 3-color e-paper display in portrait mode (480x800).
 *
 * Hardware: ESP32-C6 + GooDisplay GDEY075Z08
 */

#include <Arduino.h>
#include "config.h"
#include "calendar_data.h"
#include "battery.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "display.h"
#include "power_manager.h"
#include "cache_manager.h"

#if DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// RTC memory persists through deep sleep
RTC_DATA_ATTR char cachedEtag[MAX_ETAG_LENGTH] = "";
RTC_DATA_ATTR bool firstBoot = true;

// Generate hardcoded test data for display testing
static void generate_test_data(CalendarData& data) {
    data.clear();

    // Mock metadata
    strncpy(data.meta.etag, "test-etag-001", MAX_ETAG_LENGTH);
    data.meta.generatedAt = 1767571200;  // Jan 5, 2026 00:00 UTC

    // Mock appointments for January 2026
    // Timestamps are UTC - display converts to EST (UTC-5)
    data.appointmentCount = 0;

    // Jan 6, 2026 - Tuesday
    Appointment& apt1 = data.appointments[data.appointmentCount++];
    strncpy(apt1.id, "evt1", sizeof(apt1.id));
    strncpy(apt1.title, "Orthodontics", MAX_TITLE_LENGTH);
    apt1.startTime = 1767709200;  // Jan 6, 2026 9:20 AM EST = 14:20 UTC
    apt1.endTime = 1767712800;
    apt1.allDay = false;

    Appointment& apt2 = data.appointments[data.appointmentCount++];
    strncpy(apt2.id, "evt2", sizeof(apt2.id));
    strncpy(apt2.title, "Dentist Appointment", MAX_TITLE_LENGTH);
    apt2.startTime = 1767726000;  // Jan 6, 2026 2:00 PM EST = 19:00 UTC
    apt2.endTime = 1767729600;
    apt2.allDay = false;

    // Jan 7, 2026 - Wednesday
    Appointment& apt3 = data.appointments[data.appointmentCount++];
    strncpy(apt3.id, "evt3", sizeof(apt3.id));
    strncpy(apt3.title, "Team Meeting", MAX_TITLE_LENGTH);
    apt3.startTime = 1767798000;  // Jan 7, 2026 10:00 AM EST = 15:00 UTC
    apt3.endTime = 1767801600;
    apt3.allDay = false;

    Appointment& apt4 = data.appointments[data.appointmentCount++];
    strncpy(apt4.id, "evt4", sizeof(apt4.id));
    strncpy(apt4.title, "Lunch with Sarah", MAX_TITLE_LENGTH);
    apt4.startTime = 1767805200;  // Jan 7, 2026 12:00 PM EST = 17:00 UTC
    apt4.endTime = 1767808800;
    apt4.allDay = false;

    // Jan 8, 2026 - Thursday
    Appointment& apt5 = data.appointments[data.appointmentCount++];
    strncpy(apt5.id, "evt5", sizeof(apt5.id));
    strncpy(apt5.title, "Doctor - Immunization Shots", MAX_TITLE_LENGTH);
    apt5.startTime = 1767880800;  // Jan 8, 2026 9:00 AM EST = 14:00 UTC
    apt5.endTime = 1767884400;
    apt5.allDay = false;

    // Jan 10, 2026 - Saturday
    Appointment& apt6 = data.appointments[data.appointmentCount++];
    strncpy(apt6.id, "evt6", sizeof(apt6.id));
    strncpy(apt6.title, "Piano Recital", MAX_TITLE_LENGTH);
    apt6.startTime = 1768075200;  // Jan 10, 2026 3:00 PM EST = 20:00 UTC
    apt6.endTime = 1768082400;
    apt6.allDay = false;

    // Jan 12, 2026 - Monday (all day event)
    Appointment& apt7 = data.appointments[data.appointmentCount++];
    strncpy(apt7.id, "evt7", sizeof(apt7.id));
    strncpy(apt7.title, "Family Dinner", MAX_TITLE_LENGTH);
    apt7.startTime = 1768176000;  // Jan 12, 2026 00:00 UTC
    apt7.endTime = 1768262400;
    apt7.allDay = true;

    // Month events (abbreviated for grid) - can have multiple per day
    data.monthEventCount = 0;

    // Jan 6 - two events
    MonthEvent& mevt1 = data.monthEvents[data.monthEventCount++];
    mevt1.day = 6;
    strncpy(mevt1.title, "Ortho", sizeof(mevt1.title));

    MonthEvent& mevt2 = data.monthEvents[data.monthEventCount++];
    mevt2.day = 6;
    strncpy(mevt2.title, "Dentist", sizeof(mevt2.title));

    // Jan 7 - two events
    MonthEvent& mevt3 = data.monthEvents[data.monthEventCount++];
    mevt3.day = 7;
    strncpy(mevt3.title, "Meeting", sizeof(mevt3.title));

    MonthEvent& mevt4 = data.monthEvents[data.monthEventCount++];
    mevt4.day = 7;
    strncpy(mevt4.title, "Lunch", sizeof(mevt4.title));

    // Jan 8
    MonthEvent& mevt5 = data.monthEvents[data.monthEventCount++];
    mevt5.day = 8;
    strncpy(mevt5.title, "Shots", sizeof(mevt5.title));

    // Jan 10
    MonthEvent& mevt6 = data.monthEvents[data.monthEventCount++];
    mevt6.day = 10;
    strncpy(mevt6.title, "Piano", sizeof(mevt6.title));

    // Jan 12
    MonthEvent& mevt7 = data.monthEvents[data.monthEventCount++];
    mevt7.day = 12;
    strncpy(mevt7.title, "Dinner", sizeof(mevt7.title));

    // Month info - January 2026
    // January 1, 2026 is a Thursday (day 4)
    data.month.year = 2026;
    data.month.month = 1;
    data.month.firstDayOfWeek = 4;  // Thursday = 4 (0=Sun, 4=Thu)
    data.month.daysInMonth = 31;
    data.month.daysInPrevMonth = 31;  // December has 31 days
}

// Map a failure to a short headline + a self-service hint for the full-screen
// error (shown when there is no cached calendar to fall back on). WiFi
// failures get cause-specific copy from the connection ladder's reason-code
// classification (see wifi_manager.h).
static void handle_error(ResultCode result) {
    const char* message;
    const char* hint = nullptr;
    switch (result) {
        case ResultCode::WifiConnectionFailed:
        case ResultCode::WifiTimeout:
            switch (wifi_get_fail_cause()) {
                case WifiFailCause::RouterRejecting:
                    message = "Router refused connection";
                    hint = "Reboot router, or re-add device in router app";
                    break;
                case WifiFailCause::WrongPassword:
                    message = "WiFi password incorrect";
                    hint = "Check the WiFi password and try again";
                    break;
                case WifiFailCause::NetworkNotFound:
                    message = "WiFi network not found";
                    hint = "This display needs a 2.4GHz network";
                    break;
                case WifiFailCause::SecurityUnsupported:
                    message = "WiFi security unsupported";
                    hint = "Use a WPA2 or WPA3 personal network";
                    break;
                default:
                    message = "WiFi Failed";
                    hint = "Move the display closer to your router";
                    break;
            }
            break;
        case ResultCode::ApiRequestFailed:
        case ResultCode::ApiTimeout:
            message = "Calendar service unreachable";
            hint = "WiFi is OK - check your internet connection";
            break;
        case ResultCode::JsonParseError:
        case ResultCode::InvalidResponse:
            message = "Calendar data error";
            hint = "Will retry automatically";
            break;
        case ResultCode::BatteryCritical:
            message = "Low Battery";
            hint = "Recharge the display soon";
            break;
        case ResultCode::CacheError:
            message = "Stored data unreadable";
            break;
        default:
            message = "Unknown Error";
            break;
    }
    display_error(message, hint);
}

// One-line, footer-sized version of the WiFi failure cause, shown in red under
// the calendar when we fall back to cached data (the calendar still renders;
// this tells the customer why it stopped updating and what to do).
static const char* wifi_footer_note(WifiFailCause cause) {
    switch (cause) {
        case WifiFailCause::RouterRejecting:     return "Your router is rejecting your connection";
        case WifiFailCause::WrongPassword:       return "WiFi password looks incorrect";
        case WifiFailCause::NetworkNotFound:     return "WiFi not found - needs 2.4GHz network";
        case WifiFailCause::SecurityUnsupported: return "WiFi security mode not supported";
        default:                                 return "Unable to connect to WiFi";
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);

    LOG("\n========================================");
    LOG("Home Calendar - Starting");
    LOG("========================================\n");

    // Initialize systems
    battery_init();
    power_init_button();
    display_init();

    // Apply the local timezone immediately, on every boot and BEFORE any
    // rendering. Deep sleep clears the TZ env var, and the cache-fallback path
    // never calls power_sync_time(); without this, those renders would use raw
    // UTC and show every appointment off by the GMT offset (~5 hours).
    power_init_timezone();

#ifdef WIFI_DIAG
    // Bring-up/debug aid: log all visible networks with RSSI + security before
    // attempting to connect. Enabled only in the firebeetle_c6_diag build.
    wifi_scan_diagnostic();
#endif

    // Initialize SPIFFS cache (non-fatal if fails)
    if (cache_init() != ResultCode::Success) {
        LOG("Warning: Cache init failed, continuing without cache");
    }

    // Check battery first
    BatteryStatus battery = battery_read();
    LOGF("Battery: %.2fV (%d%%)\n", battery.voltage, battery.percentage);

    if (battery.isCritical) {
        LOG("Battery critical - hibernating");
        display_error("Low Battery");
        display_sleep();
        power_enter_hibernate();
        return;  // Never reached
    }

    // Check if woken by button (force refresh)
    bool forceRefresh = power_was_button_wake();
    if (forceRefresh) {
        LOG("Button wake - forcing refresh");
    }

    // For Phase 1: Use hardcoded test data instead of API
    // This tests the display layout without needing the backend
    CalendarData data;

#if TEST_MODE
    // In test mode, use hardcoded data directly
    LOG("TEST MODE: Using hardcoded test data");
    generate_test_data(data);
    CacheStatus cacheStatus = CacheStatus::None;
    const char* statusNote = nullptr;
#else
    // Production mode: Connect to WiFi and fetch from API
    CacheStatus cacheStatus = CacheStatus::None;
    const char* statusNote = nullptr;  // cause-specific footer text when on cache
    bool useCache = false;
    ResultCode failureCode = ResultCode::WifiConnectionFailed;  // for the no-cache error screen

    // Silent connection - no display update during hourly checks
    ResultCode wifiResult = wifi_connect();
    if (wifiResult != ResultCode::Success) {
        LOG("WiFi failed - trying cache fallback");
        useCache = true;
        failureCode = wifiResult;
        statusNote = wifi_footer_note(wifi_get_fail_cause());
    }

    if (!useCache) {
        // WiFi connected - sync time and fetch from API
        ResultCode timeResult = power_sync_time();
        if (timeResult != ResultCode::Success) {
            LOG("Warning: NTP sync failed, continuing anyway");
        }

        // Fetch calendar data (with raw JSON for caching)
        const char* etag = (forceRefresh || firstBoot) ? nullptr : cachedEtag;
        String rawJson;
        ResultCode apiResult = api_fetch_calendar(data, etag, &rawJson);

        wifi_disconnect();

        if (apiResult == ResultCode::Success) {
            // New data received - save to cache and render
            LOG("New data received - saving to cache");
            if (cache_save(rawJson.c_str(), rawJson.length()) == ResultCode::Success) {
                cache_update_timestamp();
            }
            strncpy(cachedEtag, data.meta.etag, MAX_ETAG_LENGTH);
            firstBoot = false;
        } else if (apiResult == ResultCode::NotModified) {
            // Data unchanged - check if we need to refresh display anyway
            LOG("Data unchanged (304)");

            // At 1am wake, refresh display for date change even if data unchanged
            if (power_is_date_change_wake()) {
                LOG("Date change wake - refreshing display");
                if (cache_load(data) != ResultCode::Success) {
                    LOG("Cache load failed, sleeping");
                    display_sleep();
                    power_enter_sleep(SLEEP_ON_ERROR);
                    return;
                }
                // Continue to render section below
            } else {
                // Hourly check with no change - sleep silently without display update
                LOG("Silent check - no changes, going back to sleep");
                display_sleep();
                long sleepSeconds = power_calculate_sleep_time();
                LOGF("Sleeping for %ld seconds\n", sleepSeconds);
                power_enter_sleep(sleepSeconds);
                return;
            }
        } else {
            // API failed - try cache fallback. WiFi itself is fine, so tell
            // the customer the problem is past the router (internet/service).
            LOG("API failed - trying cache fallback");
            useCache = true;
            failureCode = apiResult;
            statusNote = "WiFi OK - calendar service unreachable";
        }
    } else {
        wifi_disconnect();
    }

    // Cache fallback logic
    if (useCache) {
        if (!cache_exists()) {
            LOG("No cache available - showing error");
            handle_error(failureCode);
            display_sleep();
            power_enter_sleep(SLEEP_ON_ERROR);
            return;
        }

        cacheStatus = cache_get_status();
        LOGF("Using cached data (status: %s)\n",
             cacheStatus == CacheStatus::Fresh ? "fresh" : "stale");

        if (cache_load(data) != ResultCode::Success) {
            LOG("Cache load failed - showing error");
            handle_error(ResultCode::CacheError);
            display_sleep();
            power_enter_sleep(SLEEP_ON_ERROR);
            return;
        }
    }
#endif

    // Render the calendar
    LOG("Rendering calendar...");
    battery = battery_read();  // Re-read for accurate display
    display_calendar(data, battery, cacheStatus, statusNote);

    // Calculate sleep time and enter deep sleep
    display_sleep();

#if TEST_MODE
    LOG("TEST MODE: Not entering deep sleep");
    LOG("Display rendered successfully!");
#else
    long sleepSeconds = power_calculate_sleep_time();
    LOGF("Sleeping for %ld seconds\n", sleepSeconds);
    power_enter_sleep(sleepSeconds);
#endif
}

void loop() {
    // Never reached in normal operation (deep sleep resets)
#if TEST_MODE
    delay(10000);
    LOG("Test mode: still running...");
#endif
}
