#include "wifi_manager.h"
#include "config.h"

#ifdef NATIVE_BUILD
#include "Arduino.h"
#include "WiFi.h"
#else
#include <WiFi.h>
#include <esp_wifi.h>  // esp_wifi_set_config(), protocol/bandwidth/MAC control
#include <esp_mac.h>   // esp_efuse_mac_get_default()
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

#ifndef NATIVE_BUILD
// RTC memory persists through deep sleep - store WiFi connection info to speed up reconnection
// By storing BSSID and channel, we can skip the scanning phase
RTC_DATA_ATTR uint8_t savedBSSID[6] = {0};
RTC_DATA_ATTR int32_t savedChannel = 0;
RTC_DATA_ATTR bool wifiInfoValid = false;
RTC_DATA_ATTR int32_t savedProfile = 0;  // ladder rung that last connected (see PROFILES)
#endif

#ifndef NATIVE_BUILD
// ============================================================================
// Disconnect-reason diagnostics
// ============================================================================
// The driver reports WHY an attempt failed via the STA_DISCONNECTED event.
// This reason code is the single most important field diagnostic:
//     2 AUTH_EXPIRE             = AP never answered our auth frame
//                                 (RF decode failure, 11ax interop, steering)
//    15 4WAY_HANDSHAKE_TIMEOUT  = wrong password, or WPA3/PMF negotiation died
//   201 NO_AP_FOUND             = SSID not visible on 2.4GHz
//   211 (authmode threshold)    = AP security below our configured minimum
static volatile uint8_t lastDisconnectReason = 0;

static const char* disconnect_reason_name(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED:              return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE:              return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE:               return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE:             return "ASSOC_EXPIRE";
        case WIFI_REASON_NOT_AUTHED:               return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED:              return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE:              return "ASSOC_LEAVE";
        case WIFI_REASON_MIC_FAILURE:              return "MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:   return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_BEACON_TIMEOUT:           return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND:              return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL:                return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL:               return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:        return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL:          return "CONNECTION_FAIL";
        case 36:                                   return "STA_LEAVING";
        case 210:                                  return "NO_AP_FOUND_IN_RSSI_THRESHOLD";
        case 211:                                  return "NO_AP_FOUND_IN_AUTHMODE_THRESHOLD";
        default:                                   return "OTHER";
    }
}

static void on_sta_disconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
    LOGF("WiFi: disconnect event, reason %u (%s)\n",
         lastDisconnectReason, disconnect_reason_name(lastDisconnectReason));
}

// ============================================================================
// Failure classification (drives the customer-facing error message)
// ============================================================================
// Tally each attempt's terminal reason, then classify the overall failure into
// one actionable cause. Precedence: a completed-but-failed key handshake is
// the strongest signal (the AP is responsive -> credentials), then an explicit
// security-threshold verdict, then the silent-auth-drop signature (router
// refusing this device - the confirmed XB8 wedged-entry failure, issue #2),
// then never-found.
static uint8_t cntAuthExpire = 0;   // 2/4: AP found, auth never answered
static uint8_t cntHandshake = 0;    // 15/204/202/14: AP answered, keys failed
static uint8_t cntNotFound = 0;     // 201: SSID not seen
static uint8_t cntSecurity = 0;     // 210/211: authmode/RSSI threshold verdict

static void tally_failure_reason(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_ASSOC_EXPIRE:             cntAuthExpire++; break;
        case WIFI_REASON_MIC_FAILURE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_AUTH_FAIL:                cntHandshake++;  break;
        case WIFI_REASON_NO_AP_FOUND:              cntNotFound++;   break;
        case 210:
        case 211:                                  cntSecurity++;   break;
        default: break;  // timeouts with no verdict tell us nothing
    }
}

static WifiFailCause classify_failure() {
    if (cntHandshake > 0)  return WifiFailCause::WrongPassword;
    if (cntSecurity > 0)   return WifiFailCause::SecurityUnsupported;
    if (cntAuthExpire > 0 && cntAuthExpire >= cntNotFound) {
        return WifiFailCause::RouterRejecting;
    }
    if (cntNotFound > 0)   return WifiFailCause::NetworkNotFound;
    return WifiFailCause::Unknown;
}

static const char* fail_cause_name(WifiFailCause cause) {
    switch (cause) {
        case WifiFailCause::None:                return "none";
        case WifiFailCause::NetworkNotFound:     return "network-not-found";
        case WifiFailCause::RouterRejecting:     return "router-rejecting";
        case WifiFailCause::WrongPassword:       return "wrong-password";
        case WifiFailCause::SecurityUnsupported: return "security-unsupported";
        default:                                 return "unknown";
    }
}

// ============================================================================
// Connection compatibility ladder
// ============================================================================
// Modern gateways (WiFi 6/7, WPA3-transition, band steering - e.g. the Rogers
// XB8) interoperate badly with some of the C6's "modern" defaults. Each rung
// drops one more capability:
//   0 standard  - 802.11ax announced, max TX power (best throughput/range)
//   1 no-11ax   - b/g/n only; escapes HE (WiFi 6) auth/assoc interop bugs that
//                 11n-only boards (original ESP32/ESP32-E) never trigger
//   2 low-power - b/g/n + 8.5dBm TX; at max TX power an overdriven/mismatched
//                 PA distorts the auth frame on some C6/C3 units so the AP
//                 cannot decode it and never replies -> AUTH_EXPIRE
//                 (arduino-esp32 #6767)
// The rung that connects is cached in RTC memory and used first on later wakes,
// so the ladder only walks on first boot or after the gateway changes behavior.
struct ConnectProfile {
    bool disable11ax;      // strip HE capabilities, present as plain 11n client
    bool wpa2Only;         // declare PMF-incapable so SAE (WPA3) is off the table
    bool altMac;           // use a locally-administered STA MAC (diagnostic)
    wifi_power_t txPower;  // TX power during auth/association
    const char* name;
};

static const ConnectProfile PROFILES[] = {
    { false, false, false, WIFI_POWER_19_5dBm, "standard (11ax, 19.5dBm)" },
    { true,  false, false, WIFI_POWER_19_5dBm, "compat (b/g/n, 19.5dBm)" },
    { true,  false, false, WIFI_POWER_8_5dBm,  "compat low-power (b/g/n, 8.5dBm)" },
    // On a WPA2/WPA3-transition AP the supplicant prefers SAE whenever we are
    // PMF-capable - so every rung above authenticates via WPA3-SAE. Gateways
    // with broken/selective SAE (silently ignoring our SAE commit) produce
    // AUTH_EXPIRE on all of them, while legacy WPA2-only clients join fine.
    // SAE requires PMF: declaring ourselves PMF-incapable forces the plain
    // WPA2-PSK open-auth path those legacy clients use.
    { true,  true,  false, WIFI_POWER_19_5dBm, "wpa2-only (b/g/n, PMF off, 19.5dBm)" },
    // Diagnostic discriminator: identical to the rung above except the STA
    // presents a locally-administered MAC (stable derivation from the factory
    // MAC, not random - reboots always start from the factory MAC again).
    // If ONLY this rung connects, the gateway is filtering the factory MAC
    // (device blocked/paused in the ISP app, or auto-quarantined by an
    // "advanced security" feature) and the user must unblock it there.
    // If this rung ALSO fails, suspect the unit's RF TX path instead.
    { true,  true,  true,  WIFI_POWER_19_5dBm, "alt-mac (wpa2-only, local-admin MAC)" },
};
static constexpr int PROFILE_COUNT = sizeof(PROFILES) / sizeof(PROFILES[0]);
static constexpr int ATTEMPTS_PER_PROFILE = 2;

// Switch the STA between the factory MAC and a stable locally-administered
// variant (set the local-admin bit, twiddle the last byte). esp_wifi_set_mac
// is not persistent, so every boot starts from the factory MAC.
static void apply_sta_mac(bool useAltMac) {
    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        LOGF("WiFi: get_mac failed: 0x%x (%s)\n", err, esp_err_to_name(err));
        return;
    }

    // NOTE: esp_read_mac(ESP_MAC_WIFI_STA), NOT esp_efuse_mac_get_default().
    // On 802.15.4-capable chips (C6/H2) the efuse default is the EUI-64 base
    // whose first six bytes contain the FF:FE infix - not the STA MAC.
    uint8_t factory[6];
    esp_read_mac(factory, ESP_MAC_WIFI_STA);
    uint8_t target[6];
    memcpy(target, factory, 6);
    if (useAltMac) {
        target[0] |= 0x02;   // locally administered
        target[5] ^= 0xA5;   // distinct per-unit suffix
    }

    if (memcmp(mac, target, 6) == 0) {
        return;  // already presenting the right identity
    }

    err = esp_wifi_set_mac(WIFI_IF_STA, target);
    LOGF("WiFi: STA MAC -> %02X:%02X:%02X:%02X:%02X:%02X (%s)%s\n",
         target[0], target[1], target[2], target[3], target[4], target[5],
         useAltMac ? "locally-administered" : "factory",
         err == ESP_OK ? "" : " FAILED");
    if (err != ESP_OK) {
        LOGF("WiFi: set_mac error: 0x%x (%s)\n", err, esp_err_to_name(err));
    }
}

static void apply_profile(int index) {
    const ConnectProfile& p = PROFILES[index];
    LOGF("WiFi: Applying profile %d (%s)\n", index, p.name);

    apply_sta_mac(p.altMac);

    uint8_t protocol = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
#ifdef WIFI_PROTOCOL_11AX
    if (!p.disable11ax) {
        protocol |= WIFI_PROTOCOL_11AX;
    }
#endif
    esp_err_t err = esp_wifi_set_protocol(WIFI_IF_STA, protocol);
    if (err != ESP_OK) {
        LOGF("WiFi: set_protocol failed: 0x%x (%s)\n", err, esp_err_to_name(err));
    }

    // 20MHz only: the C6's 802.11ax mode is 20MHz-only anyway, and HT40 on a
    // crowded 2.4GHz band just adds decode/interop risk during auth.
    err = esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    if (err != ESP_OK) {
        LOGF("WiFi: set_bandwidth failed: 0x%x (%s)\n", err, esp_err_to_name(err));
    }

    WiFi.setTxPower(p.txPower);
}

// Start a STA association configured for WPA3-Personal-Transition gateways.
//
// WiFi.begin() zero-inits the driver config (sae_pwe_h2e = UNSPECIFIED), which
// can mismatch the gateway's SAE password-element method and fail auth with
// AUTH_EXPIRE even at full signal (observed against the Rogers/Comcast XB8).
// We set the SSID/password AND sae_pwe_h2e = BOTH (accept hunt-and-peck and
// hash-to-element) plus PMF-capable in one config, then connect directly.
//
// The config also requests a full-channel scan sorted by signal strength, so
// on mesh/band-steering setups we authenticate against the strongest BSSID
// broadcasting our SSID instead of whichever answered a probe first. Passing a
// BSSID pins the exact AP (fast reconnect after deep sleep, skips the scan).
static void wifi_begin_sta(const char* ssid, const char* password,
                           const uint8_t* bssid, int32_t channel, bool wpa2Only) {
    wifi_config_t conf;
    memset(&conf, 0, sizeof(conf));
    strncpy(reinterpret_cast<char*>(conf.sta.ssid), ssid, sizeof(conf.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(conf.sta.password), password, sizeof(conf.sta.password) - 1);
    if (wpa2Only) {
        // No PMF capability -> SAE is impossible -> supplicant must use the
        // legacy WPA2-PSK open-system auth path (see PROFILES comment).
        conf.sta.pmf_cfg.capable = false;
        conf.sta.pmf_cfg.required = false;
    } else {
        conf.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        conf.sta.pmf_cfg.capable = true;    // WPA3-transition requires PMF-capable
        conf.sta.pmf_cfg.required = false;  // ...but not PMF-required (keeps WPA2 path open)
    }
    conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;  // floor: never join an open/WEP twin
    conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;      // see every BSSID of the SSID...
    conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;  // ...and auth to the strongest
    // NOTE: deliberately no failure_retry_cnt - in-driver retries delay the
    // driver's verdict (the reason code) past our attempt window. We retry
    // ourselves and want the real reason as fast as possible.

    if (bssid != nullptr) {
        memcpy(conf.sta.bssid, bssid, 6);
        conf.sta.bssid_set = 1;
        conf.sta.channel = static_cast<uint8_t>(channel);
    }

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &conf);
    if (err != ESP_OK) {
        // Likely "sta is connecting": the Arduino core has a hardcoded
        // retry-once-on-first-failure (STA.cpp first_connect) that IGNORES
        // setAutoReconnect(false) and can grab the radio between our attempts.
        // Force the STA idle and try once more.
        LOGF("WiFi: set_config failed: 0x%x (%s) - forcing idle and retrying\n",
             err, esp_err_to_name(err));
        esp_wifi_disconnect();
        delay(150);
        err = esp_wifi_set_config(WIFI_IF_STA, &conf);
        if (err != ESP_OK) {
            LOGF("WiFi: set_config retry failed: 0x%x (%s)\n", err, esp_err_to_name(err));
        }
    }

    lastDisconnectReason = 0;
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        LOGF("WiFi: connect call failed: 0x%x (%s)\n", err, esp_err_to_name(err));
    }
}
#endif

static WifiFailCause lastFailCause = WifiFailCause::None;

WifiFailCause wifi_get_fail_cause() {
    return lastFailCause;
}

static bool attempt_connection() {
    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime >= ATTEMPT_TIMEOUT_MS) {
            return false;
        }
#ifndef NATIVE_BUILD
        // The driver delivered a terminal verdict (e.g. AUTH_EXPIRE) and
        // auto-reconnect is off, so nothing more will happen this attempt.
        // Bail out now instead of burning the rest of the timeout, and let
        // the ladder retry with fresh settings.
        if (lastDisconnectReason != 0) {
            return false;
        }
#endif
        delay(100);
    }

    return true;
}

ResultCode wifi_connect() {
    LOG("WiFi: Starting connection");
    LOGF("WiFi: SSID: %s\n", WIFI_SSID);

    lastFailCause = WifiFailCause::None;
#ifndef NATIVE_BUILD
    cntAuthExpire = cntHandshake = cntNotFound = cntSecurity = 0;
#endif

    // Check if already connected (shouldn't happen after deep sleep, but check anyway)
    if (WiFi.status() == WL_CONNECTED) {
        LOG("WiFi: Already connected!");
        LOGF("WiFi: IP: %s\n", WiFi.localIP().toString().c_str());
        return ResultCode::Success;
    }

#ifndef NATIVE_BUILD
    // Log every disconnect's 802.11 reason code - the key field diagnostic.
    static bool eventRegistered = false;
    if (!eventRegistered) {
        WiFi.onEvent(on_sta_disconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        eventRegistered = true;
    }
#endif

    // ESP32-C6: Force clean state after deep sleep wake
    // This prevents the radio from being in an undefined state
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(50);  // Reduced delay - just enough for radio reset

    // Configure WiFi
    WiFi.persistent(false);  // Don't save credentials to flash (prevents corruption)
    WiFi.setAutoReconnect(false);  // We handle retries ourselves
    WiFi.mode(WIFI_STA);
    delay(50);  // Reduced delay

#ifndef NATIVE_BUILD
    // Keep the radio awake for the entire connect: modem power-save (the
    // Arduino default) can doze through the AP's auth reply, which then
    // times out on our side as AUTH_EXPIRE.
    WiFi.setSleep(false);

    LOGF("WiFi: STA MAC: %s\n", WiFi.macAddress().c_str());

    // Fast path: rejoin the exact AP (BSSID/channel) and ladder rung that
    // worked before deep sleep - skips scanning and the ladder walk.
    if (wifiInfoValid) {
        if (savedProfile < 0 || savedProfile >= PROFILE_COUNT) {
            savedProfile = 0;
        }
        LOGF("WiFi: Fast reconnect, cached channel %ld\n", savedChannel);
        apply_profile(savedProfile);
        wifi_begin_sta(WIFI_SSID, WIFI_PASSWORD, savedBSSID, savedChannel,
                       PROFILES[savedProfile].wpa2Only);

        if (attempt_connection()) {
            LOG("WiFi: Fast reconnect successful!");
            LOGF("WiFi: IP: %s\n", WiFi.localIP().toString().c_str());
            LOGF("WiFi: RSSI: %d dBm\n", WiFi.RSSI());
            return ResultCode::Success;
        }

        LOG("WiFi: Fast reconnect failed, falling back to full ladder");
        tally_failure_reason(lastDisconnectReason);
        wifiInfoValid = false;     // Cached info is stale
        WiFi.disconnect();         // NOTE: no wifioff - keep the driver running
        delay(100);
    }

    // Walk the compatibility ladder from most to least capable. Every failed
    // attempt logs the driver's reason code, so field logs show exactly what
    // the gateway did (AUTH_EXPIRE vs handshake timeout vs not found).
    bool ssidNotFound = false;  // set on NO_AP_FOUND to abort the ladder early
    for (int p = 0; p < PROFILE_COUNT; p++) {
        apply_profile(p);

        for (int attempt = 1; attempt <= ATTEMPTS_PER_PROFILE; attempt++) {
            LOGF("WiFi: Profile %d/%d attempt %d/%d (status: %d)\n",
                 p + 1, PROFILE_COUNT, attempt, ATTEMPTS_PER_PROFILE, WiFi.status());

            wifi_begin_sta(WIFI_SSID, WIFI_PASSWORD, nullptr, 0, PROFILES[p].wpa2Only);

            if (attempt_connection()) {
                LOG("WiFi: Connected!");
                LOGF("WiFi: IP: %s\n", WiFi.localIP().toString().c_str());
                LOGF("WiFi: RSSI: %d dBm\n", WiFi.RSSI());
                LOGF("WiFi: Profile: %s\n", PROFILES[p].name);

                // Save connection info for fast reconnect after next deep sleep
                memcpy(savedBSSID, WiFi.BSSID(), 6);
                savedChannel = WiFi.channel();
                savedProfile = p;
                wifiInfoValid = true;
                LOGF("WiFi: Saved BSSID, channel %ld and profile for fast reconnect\n",
                     savedChannel);
                return ResultCode::Success;
            }

            if (lastDisconnectReason == 0) {
                LOGF("WiFi: Attempt timed out with NO driver verdict (status: %d)\n",
                     WiFi.status());
            } else {
                LOGF("WiFi: Attempt failed (status: %d, reason: %u %s)\n",
                     WiFi.status(), lastDisconnectReason,
                     disconnect_reason_name(lastDisconnectReason));
            }
            tally_failure_reason(lastDisconnectReason);

            // NO_AP_FOUND means the SSID was not seen on ANY channel (every
            // attempt runs a full all-channel scan). The remaining ladder rungs
            // only vary auth/assoc behaviour (11ax, SAE, TX power) - none of
            // which can make an absent SSID appear. Walking all 12 attempts
            // here would burn ~12x the radio-on time every wake for as long as
            // the router/SSID is down, which is exactly what flattens the
            // battery during an outage. Abort now; the next scheduled wake (see
            // the disconnect backoff in power_manager) retries from the top.
            if (lastDisconnectReason == WIFI_REASON_NO_AP_FOUND) {
                LOG("WiFi: SSID not found on any channel - aborting ladder (no rung aids discovery)");
                WiFi.disconnect();
                ssidNotFound = true;
                break;
            }

            WiFi.disconnect();     // no wifioff - the next attempt needs the driver up
            delay(200);
        }
        if (ssidNotFound) break;
    }

    LOG("WiFi: All profiles failed");
    LOGF("WiFi: Final status: %d, last reason: %u (%s)\n", WiFi.status(),
         lastDisconnectReason, disconnect_reason_name(lastDisconnectReason));
    lastFailCause = classify_failure();
    LOGF("WiFi: Failure classified as '%s' (auth-expire:%u handshake:%u "
         "not-found:%u security:%u)\n", fail_cause_name(lastFailCause),
         cntAuthExpire, cntHandshake, cntNotFound, cntSecurity);
#else
    // Native simulation: plain retry loop against the mocked WiFi.
    for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; attempt++) {
        LOGF("WiFi: Attempt %d/%d (status: %d)\n", attempt, WIFI_MAX_RETRIES, WiFi.status());

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        if (attempt_connection()) {
            LOG("WiFi: Connected!");
            LOGF("WiFi: IP: %s\n", WiFi.localIP().toString().c_str());
            return ResultCode::Success;
        }

        LOGF("WiFi: Attempt %d timed out (status: %d)\n", attempt, WiFi.status());
        WiFi.disconnect(true);
        delay(200);
    }

    LOG("WiFi: All attempts failed");
    LOGF("WiFi: Final status: %d\n", WiFi.status());
    lastFailCause = WifiFailCause::Unknown;
#endif
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

#ifndef NATIVE_BUILD
// Human-readable name for an Arduino WiFi encryption/auth type.
static const char* auth_type_name(wifi_auth_mode_t type) {
    switch (type) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI-PSK";
        default:                        return "UNKNOWN";
    }
}

// Rough signal-quality label from RSSI (dBm).
static const char* rssi_quality(int rssi) {
    if (rssi >= -60) return "good";
    if (rssi >= -70) return "ok";
    if (rssi >= -80) return "weak";
    return "very weak";
}
#endif

void wifi_scan_diagnostic() {
#ifdef NATIVE_BUILD
    LOG("WiFi scan: not supported in native build");
#else
    LOG("==== WiFi scan diagnostic ====");
    LOGF("WiFi scan: looking for configured SSID '%s'\n", WIFI_SSID);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(100);

    int n = WiFi.scanNetworks();
    if (n <= 0) {
        LOGF("WiFi scan: no networks found (result %d)\n", n);
        LOG("==== end WiFi scan ====");
        WiFi.scanDelete();
        return;
    }

    LOGF("WiFi scan: %d network(s) found\n", n);
    bool configuredFound = false;
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        bool match = ssid == WIFI_SSID;
        if (match) configuredFound = true;
        LOGF("  [%2d] RSSI=%4d dBm (%-9s) ch=%2ld %-13s %s%s\n",
             i, rssi, rssi_quality(rssi), (long)WiFi.channel(i),
             auth_type_name(WiFi.encryptionType(i)),
             ssid.c_str(), match ? "  <-- CONFIGURED" : "");
    }

    if (configuredFound) {
        LOGF("WiFi scan: '%s' IS visible (see RSSI/security above)\n", WIFI_SSID);
    } else {
        LOGF("WiFi scan: '%s' NOT visible -- out of range or 5GHz-only?\n", WIFI_SSID);
    }
    LOG("==== end WiFi scan ====");

    WiFi.scanDelete();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(50);
#endif
}
