#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>

// Maximum lengths for stored values
#define STORAGE_MAX_SSID_LEN     32
#define STORAGE_MAX_PASSWORD_LEN 64
#define STORAGE_MAX_ENDPOINT_LEN 128
#define STORAGE_MAX_NAME_LEN     32

// Initialize NVS storage
// Must be called before any other storage functions
void config_storage_init();

// WiFi Credentials
bool config_has_wifi_credentials();
bool config_save_wifi_credentials(const char* ssid, const char* password);
bool config_load_wifi_ssid(char* buffer, size_t bufferSize);
bool config_load_wifi_password(char* buffer, size_t bufferSize);
void config_clear_wifi_credentials();

// API Endpoint (optional override of compiled default)
bool config_has_api_endpoint();
bool config_save_api_endpoint(const char* endpoint);
bool config_load_api_endpoint(char* buffer, size_t bufferSize);
void config_clear_api_endpoint();

// Device Name (for BLE advertising)
bool config_has_device_name();
bool config_save_device_name(const char* name);
bool config_load_device_name(char* buffer, size_t bufferSize);
void config_clear_device_name();

// Last update timestamp (persists across deep sleep)
void config_save_last_update(uint32_t timestamp);
uint32_t config_load_last_update();

// Last error code (persists across deep sleep)
void config_save_last_error(uint8_t errorCode);
uint8_t config_load_last_error();

// Factory reset - clear all stored config
void config_factory_reset();

#endif // CONFIG_STORAGE_H
