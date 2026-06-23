#include "api_client.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#else
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <NetworkClientSecure.h>
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

static void safe_copy(char* dest, const char* src, size_t maxLen) {
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}

static ResultCode parse_json(const char* json, CalendarData& data) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);

    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        LOGF("API: JSON parse error: %s\n", error.c_str());
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

    LOGF("API: Parsed %d appointments, %d month events\n",
         data.appointmentCount, data.monthEventCount);

    return ResultCode::Success;
}

#ifndef NATIVE_BUILD
static ResultCode fetch_once(CalendarData& data, const char* currentEtag, String* rawJsonOut) {
    HTTPClient http;
    LOGF("API: Fetching from %s\n", API_ENDPOINT);

    NetworkClientSecure client;
    client.setInsecure();  // Skip certificate validation for now

    LOG("API: Connecting...");
    if (!http.begin(client, API_ENDPOINT)) {
        LOG("API: Failed to begin HTTP connection");
        return ResultCode::ApiRequestFailed;
    }

    http.setTimeout(API_TIMEOUT_MS);
    if (strlen(API_KEY) > 0) {
        http.addHeader("X-Api-Key", API_KEY);
    }

    // Add If-None-Match header if we have an etag
    if (currentEtag && strlen(currentEtag) > 0) {
        http.addHeader("If-None-Match", currentEtag);
    }

    LOG("API: Sending GET request...");
    int httpCode = http.GET();
    LOGF("API: Response code: %d\n", httpCode);

    if (httpCode == 304) {
        LOG("API: Not modified (304)");
        http.end();
        return ResultCode::NotModified;
    }

    if (httpCode <= 0) {
        LOGF("API: Connection failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return ResultCode::ApiRequestFailed;
    }

    if (httpCode != HTTP_CODE_OK) {
        LOGF("API: HTTP error %d\n", httpCode);
        http.end();
        return ResultCode::ApiRequestFailed;
    }

    String response = http.getString();
    http.end();
    LOGF("API: Response size: %d bytes\n", response.length());

    if (response.length() == 0) {
        LOG("API: Empty response");
        return ResultCode::InvalidResponse;
    }

    // Return raw JSON for caching if requested
    if (rawJsonOut != nullptr) {
        *rawJsonOut = response;
    }

    return parse_json(response.c_str(), data);
}
#endif

ResultCode api_fetch_calendar(CalendarData& data, const char* currentEtag, String* rawJsonOut) {
    LOGF("API: Starting fetch (etag: %s)\n", currentEtag ? currentEtag : "none");

#ifdef NATIVE_BUILD
    LOG("API: Native build - returning mock data");
    return ResultCode::ApiRequestFailed;
#else
    for (int attempt = 1; attempt <= API_MAX_RETRIES; attempt++) {
        LOGF("API: Attempt %d/%d\n", attempt, API_MAX_RETRIES);

        ResultCode result = fetch_once(data, currentEtag, rawJsonOut);

        if (result == ResultCode::Success || result == ResultCode::NotModified) {
            LOG("API: Fetch successful");
            return result;
        }

        if (attempt < API_MAX_RETRIES) {
            LOGF("API: Retrying in %d ms\n", API_RETRY_DELAY_MS);
            delay(API_RETRY_DELAY_MS);
        }
    }

    LOG("API: All attempts failed");
    return ResultCode::ApiRequestFailed;
#endif
}
