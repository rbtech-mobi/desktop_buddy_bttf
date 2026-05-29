#ifndef SECRETS_H
#define SECRETS_H

// ============================================================================
// WiFi Configuration: Dual Network with Automatic Failover
// ============================================================================
// The device will try PRIMARY network first. If unavailable, it automatically
// switches to SECONDARY. If primary comes back online, device switches back.
// Example use cases:
//   - Primary: Main network (2.4 GHz), Secondary: Guest/Backup (2.4 GHz)
//   - Primary: Office WiFi, Secondary: Mobile hotspot fallback

// Primary WiFi Network (preferred)
const char* WIFI_SSID_PRIMARY = "your-primary-ssid";
const char* WIFI_PASS_PRIMARY = "your-primary-password";

// Fallback WiFi Network (secondary)
const char* WIFI_SSID_SECONDARY = "your-secondary-ssid";
const char* WIFI_PASS_SECONDARY = "your-secondary-password";

// WiFi Failover Timings
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;  // Timeout per attempt (ms)
const unsigned long WIFI_PRIMARY_RETRY_MS = 30000;    // Check if primary is back (ms)

// ============================================================================
// MQTT Configuration (TLS/Secure)
// ============================================================================
const char* MQTT_HOST = "your-broker-host";
const int   MQTT_PORT = 8883;                         // TLS port (secure)
const char* MQTT_USER = "your-mqtt-username";
const char* MQTT_PASS = "your-mqtt-password";

// CA Root Certificate for TLS broker validation (PEM format)
// Obtain from your MQTT broker. Must include \n for line breaks.
const char* MQTT_ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"MIIBkjCB+wIJAKHHCgVFH5NCMA0GCSqGSIb3DQEBBQUAMBMxETAPBgNVBAMMCCpz\n"
"...(base64 certificate content goes here)...\n"
"-----END CERTIFICATE-----\n";

#endif  // SECRETS_H
