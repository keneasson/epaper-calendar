#include "api_client.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#include "HTTPClient.h"
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

static ResultCode parse_json(const char* json, Schedule& schedule) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);

    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        LOGF("API: JSON parse error: %s\n", error.c_str());
        return ResultCode::JsonParseError;
    }

    schedule.clear();

    // API returns an array - get the first (upcoming) service
    if (!doc.is<JsonArray>() || doc.size() == 0) {
        LOG("API: Expected non-empty array");
        return ResultCode::InvalidResponse;
    }

    JsonObject service = doc[0];

    // Extract service fields
    safe_copy(schedule.serviceDate, service["Date"] | "", MAX_DATE_LENGTH);
    safe_copy(schedule.presiding, service["Preside"] | "", MAX_NAME_LENGTH);
    safe_copy(schedule.exhorting, service["Exhort"] | "", MAX_NAME_LENGTH);
    safe_copy(schedule.keyboard, service["Organist"] | "", MAX_NAME_LENGTH);
    safe_copy(schedule.steward, service["Steward"] | "", MAX_NAME_LENGTH);
    safe_copy(schedule.doorkeeper, service["Doorkeeper"] | "", MAX_NAME_LENGTH);
    safe_copy(schedule.collection, service["Collection"] | "", MAX_COLLECTION_LENGTH);

    // Lunch is planned if the "Lunch" field is present
    schedule.hasLunch = service.containsKey("Lunch");

    LOGF("API: Parsed service for %s\n", schedule.serviceDate);
    return ResultCode::Success;
}

#if defined(NATIVE_BUILD)
// Native build simulation
static ResultCode fetch_once(Schedule& schedule) {
    HTTPClient http;
    LOGF("API: Fetching from %s\n", API_ENDPOINT);
    http.begin(API_ENDPOINT);
    http.setTimeout(API_TIMEOUT_MS);
    if (strlen(API_KEY) > 0) {
        http.addHeader("Authorization", String("Bearer ") + API_KEY);
    }
    int httpCode = http.GET();
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
    return parse_json(response.c_str(), schedule);
}

#else
// ESP32 - use Arduino HTTPClient with NetworkClientSecure for HTTPS
static ResultCode fetch_once(Schedule& schedule) {
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
        http.addHeader("Authorization", String("Bearer ") + API_KEY);
    }

    LOG("API: Sending GET request...");
    int httpCode = http.GET();
    LOGF("API: Response code: %d\n", httpCode);

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

    return parse_json(response.c_str(), schedule);
}
#endif

ResultCode api_fetch_schedule(Schedule& schedule) {
    LOG("API: Starting fetch");

    for (int attempt = 1; attempt <= API_MAX_RETRIES; attempt++) {
        LOGF("API: Attempt %d/%d\n", attempt, API_MAX_RETRIES);

        ResultCode result = fetch_once(schedule);

        if (result == ResultCode::Success) {
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
}
