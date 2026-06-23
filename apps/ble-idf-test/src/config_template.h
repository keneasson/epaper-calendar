#ifndef CONFIG_H
#define CONFIG_H

/*
 * ble-idf-test - known-network credentials
 *
 * Copy this file to config.h and fill in real values. config.h is gitignored
 * (the root .gitignore excludes every config.h), so credentials never reach
 * the repo.
 *
 * This S3 test client pushes these credentials to the C6 over BLE to exercise
 * the provisioning flow (see docs/ble-wifi-provisioning.md). It is a bench
 * tool, not shipped firmware - the product never hardcodes credentials.
 */

// Known networks: SSID -> password. Add as many pairs as you test against.
#define KNOWN_SSID_1      "YourPrimarySSID"
#define KNOWN_PASSWORD_1  "YourPrimaryPassword"
#define KNOWN_SSID_2      "YourSecondarySSID"
#define KNOWN_PASSWORD_2  "YourSecondaryPassword"

// Password used for an SSID not in the known list above.
#define DEFAULT_PASSWORD  KNOWN_PASSWORD_2

#endif // CONFIG_H
