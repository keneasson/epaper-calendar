/*
 * WiFi API Simulation for Native Builds
 */

#ifndef WIFI_SIM_H
#define WIFI_SIM_H

#include "Arduino.h"

// WiFi status codes
typedef enum {
    WL_NO_SHIELD = 255,
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
} wl_status_t;

// WiFi modes
typedef enum {
    WIFI_OFF = 0,
    WIFI_STA = 1,
    WIFI_AP = 2,
    WIFI_AP_STA = 3
} wifi_mode_t;

class WiFiClass {
public:
    WiFiClass() : status_(WL_DISCONNECTED), mode_(WIFI_OFF) {}

    void mode(wifi_mode_t m) {
        mode_ = m;
        printf("[SIM] WiFi mode set to %d\n", m);
    }

    void begin(const char* ssid, const char* password) {
        printf("[SIM] WiFi connecting to '%s'...\n", ssid);
        // Simulate connection delay
        delay(500);
        status_ = WL_CONNECTED;
        printf("[SIM] WiFi connected!\n");
    }

    void disconnect(bool wifiOff = false) {
        status_ = WL_DISCONNECTED;
        if (wifiOff) {
            mode_ = WIFI_OFF;
        }
        printf("[SIM] WiFi disconnected\n");
    }

    wl_status_t status() const { return status_; }

    void setAutoReconnect(bool autoReconnect) {
        (void)autoReconnect;
    }

    IPAddress localIP() const {
        return IPAddress(192, 168, 1, 100);
    }

    int RSSI() const {
        return -55;  // Good signal strength
    }

private:
    wl_status_t status_;
    wifi_mode_t mode_;
};

extern WiFiClass WiFi;

#endif // WIFI_SIM_H
