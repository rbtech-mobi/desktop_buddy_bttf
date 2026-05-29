#ifndef SECRETS_H
#define SECRETS_H

// Wi-Fi
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

// MQTT
const char* MQTT_HOST = "your-broker-host";
const int   MQTT_PORT = 8883;
const char* MQTT_USER = "your-user";
const char* MQTT_PASS = "your-password";

// CA root certificate for TLS broker validation.
const char* MQTT_ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"...\n"
"-----END CERTIFICATE-----\n";

#endif  // SECRETS_H
