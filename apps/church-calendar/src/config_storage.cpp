#include "config_storage.h"

#ifndef NATIVE_BUILD
#include <Preferences.h>

// NVS namespace
static const char* NVS_NAMESPACE = "church_disp";

// NVS keys
static const char* KEY_WIFI_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASS = "wifi_pass";
static const char* KEY_API_URL = "api_url";
static const char* KEY_DEV_NAME = "dev_name";
static const char* KEY_LAST_UPDATE = "last_upd";
static const char* KEY_LAST_ERROR = "last_err";

static Preferences prefs;

void config_storage_init() {
    // Preferences handles initialization internally
}

// WiFi Credentials
bool config_has_wifi_credentials() {
    prefs.begin(NVS_NAMESPACE, true);  // read-only
    bool hasSSID = prefs.isKey(KEY_WIFI_SSID);
    bool hasPass = prefs.isKey(KEY_WIFI_PASS);
    prefs.end();
    return hasSSID && hasPass;
}

bool config_save_wifi_credentials(const char* ssid, const char* password) {
    if (!ssid || !password) return false;
    if (strlen(ssid) == 0) return false;

    prefs.begin(NVS_NAMESPACE, false);  // read-write
    size_t written1 = prefs.putString(KEY_WIFI_SSID, ssid);
    size_t written2 = prefs.putString(KEY_WIFI_PASS, password);
    prefs.end();

    return (written1 > 0 && written2 > 0);
}

bool config_load_wifi_ssid(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;

    prefs.begin(NVS_NAMESPACE, true);
    String ssid = prefs.getString(KEY_WIFI_SSID, "");
    prefs.end();

    if (ssid.length() == 0) return false;
    if (ssid.length() >= bufferSize) return false;

    strncpy(buffer, ssid.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}

bool config_load_wifi_password(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;

    prefs.begin(NVS_NAMESPACE, true);
    String pass = prefs.getString(KEY_WIFI_PASS, "");
    prefs.end();

    if (pass.length() >= bufferSize) return false;

    strncpy(buffer, pass.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}

void config_clear_wifi_credentials() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(KEY_WIFI_SSID);
    prefs.remove(KEY_WIFI_PASS);
    prefs.end();
}

// API Endpoint
bool config_has_api_endpoint() {
    prefs.begin(NVS_NAMESPACE, true);
    bool has = prefs.isKey(KEY_API_URL);
    prefs.end();
    return has;
}

bool config_save_api_endpoint(const char* endpoint) {
    if (!endpoint || strlen(endpoint) == 0) return false;

    prefs.begin(NVS_NAMESPACE, false);
    size_t written = prefs.putString(KEY_API_URL, endpoint);
    prefs.end();

    return written > 0;
}

bool config_load_api_endpoint(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;

    prefs.begin(NVS_NAMESPACE, true);
    String url = prefs.getString(KEY_API_URL, "");
    prefs.end();

    if (url.length() == 0) return false;
    if (url.length() >= bufferSize) return false;

    strncpy(buffer, url.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}

void config_clear_api_endpoint() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(KEY_API_URL);
    prefs.end();
}

// Device Name
bool config_has_device_name() {
    prefs.begin(NVS_NAMESPACE, true);
    bool has = prefs.isKey(KEY_DEV_NAME);
    prefs.end();
    return has;
}

bool config_save_device_name(const char* name) {
    if (!name || strlen(name) == 0) return false;

    prefs.begin(NVS_NAMESPACE, false);
    size_t written = prefs.putString(KEY_DEV_NAME, name);
    prefs.end();

    return written > 0;
}

bool config_load_device_name(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;

    prefs.begin(NVS_NAMESPACE, true);
    String name = prefs.getString(KEY_DEV_NAME, "");
    prefs.end();

    if (name.length() == 0) return false;
    if (name.length() >= bufferSize) return false;

    strncpy(buffer, name.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}

void config_clear_device_name() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(KEY_DEV_NAME);
    prefs.end();
}

// Last update timestamp
void config_save_last_update(uint32_t timestamp) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putULong(KEY_LAST_UPDATE, timestamp);
    prefs.end();
}

uint32_t config_load_last_update() {
    prefs.begin(NVS_NAMESPACE, true);
    uint32_t ts = prefs.getULong(KEY_LAST_UPDATE, 0);
    prefs.end();
    return ts;
}

// Last error code
void config_save_last_error(uint8_t errorCode) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(KEY_LAST_ERROR, errorCode);
    prefs.end();
}

uint8_t config_load_last_error() {
    prefs.begin(NVS_NAMESPACE, true);
    uint8_t err = prefs.getUChar(KEY_LAST_ERROR, 0);
    prefs.end();
    return err;
}

// Factory reset
void config_factory_reset() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
}

#else
// Native build stubs

static char _ssid[STORAGE_MAX_SSID_LEN] = "";
static char _pass[STORAGE_MAX_PASSWORD_LEN] = "";
static char _endpoint[STORAGE_MAX_ENDPOINT_LEN] = "";
static char _name[STORAGE_MAX_NAME_LEN] = "";
static uint32_t _lastUpdate = 0;
static uint8_t _lastError = 0;

void config_storage_init() {}

bool config_has_wifi_credentials() { return strlen(_ssid) > 0; }
bool config_save_wifi_credentials(const char* ssid, const char* password) {
    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    strncpy(_pass, password, sizeof(_pass) - 1);
    return true;
}
bool config_load_wifi_ssid(char* buffer, size_t bufferSize) {
    strncpy(buffer, _ssid, bufferSize - 1);
    return strlen(_ssid) > 0;
}
bool config_load_wifi_password(char* buffer, size_t bufferSize) {
    strncpy(buffer, _pass, bufferSize - 1);
    return true;
}
void config_clear_wifi_credentials() { _ssid[0] = '\0'; _pass[0] = '\0'; }

bool config_has_api_endpoint() { return strlen(_endpoint) > 0; }
bool config_save_api_endpoint(const char* endpoint) {
    strncpy(_endpoint, endpoint, sizeof(_endpoint) - 1);
    return true;
}
bool config_load_api_endpoint(char* buffer, size_t bufferSize) {
    strncpy(buffer, _endpoint, bufferSize - 1);
    return strlen(_endpoint) > 0;
}
void config_clear_api_endpoint() { _endpoint[0] = '\0'; }

bool config_has_device_name() { return strlen(_name) > 0; }
bool config_save_device_name(const char* name) {
    strncpy(_name, name, sizeof(_name) - 1);
    return true;
}
bool config_load_device_name(char* buffer, size_t bufferSize) {
    strncpy(buffer, _name, bufferSize - 1);
    return strlen(_name) > 0;
}
void config_clear_device_name() { _name[0] = '\0'; }

void config_save_last_update(uint32_t timestamp) { _lastUpdate = timestamp; }
uint32_t config_load_last_update() { return _lastUpdate; }

void config_save_last_error(uint8_t errorCode) { _lastError = errorCode; }
uint8_t config_load_last_error() { return _lastError; }

void config_factory_reset() {
    _ssid[0] = '\0';
    _pass[0] = '\0';
    _endpoint[0] = '\0';
    _name[0] = '\0';
    _lastUpdate = 0;
    _lastError = 0;
}

#endif // NATIVE_BUILD
