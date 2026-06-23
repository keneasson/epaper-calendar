#include "wifi_manager.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#include "WiFi.h"
#else
#include <WiFi.h>
#endif

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

// Timeout per connection attempt
static constexpr unsigned long ATTEMPT_TIMEOUT_MS = WIFI_TIMEOUT_MS / WIFI_MAX_RETRIES;

static bool attempt_connection() {
    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime >= ATTEMPT_TIMEOUT_MS) {
            return false;
        }
        delay(100);
    }

    return true;
}

ResultCode wifi_connect() {
    LOG("WiFi: Starting connection");
    LOGF("WiFi: SSID: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle retries ourselves

    for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; attempt++) {
        LOGF("WiFi: Attempt %d/%d\n", attempt, WIFI_MAX_RETRIES);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        if (attempt_connection()) {
            LOG("WiFi: Connected!");
            LOGF("WiFi: IP: %s\n", WiFi.localIP().toString().c_str());
            LOGF("WiFi: RSSI: %d dBm\n", WiFi.RSSI());
            return ResultCode::Success;
        }

        LOG("WiFi: Attempt timed out");
        WiFi.disconnect(true);
        delay(500);  // Brief pause before retry
    }

    LOG("WiFi: All attempts failed");
    return ResultCode::WifiConnectionFailed;
}

void wifi_disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    LOG("WiFi: Disconnected and radio off");
}

int wifi_get_rssi() {
    return WiFi.RSSI();
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
