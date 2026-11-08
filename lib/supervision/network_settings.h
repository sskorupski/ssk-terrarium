// Wi-Fi and broker settings entered on the configuration portal
// (RG-configuration-wifi-sur-place). Pure logic: no hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::supervision {

struct NetworkSettings {
  char wifiSsid[33];      // 1 to 32 bytes.
  char wifiPassword[64];  // Empty (open network) or 8 to 63 characters.
  char brokerHost[64];    // Host name only: letters, digits, '.' and '-'.
  uint16_t brokerPort;    // 1 to 65535.
  char mqttUser[33];      // 1 to 32 printable characters.
  char mqttPassword[65];  // 1 to 64 printable characters.
};

enum class SettingsError : uint8_t {
  kNone,
  kWifiSsid,
  kWifiPassword,
  kBrokerHost,
  kBrokerPort,
  kMqttUser,
  kMqttPassword,
};

// Checks the posted form fields and copies them into `out`. On any error,
// returns the first faulty field and leaves `out` untouched. A missing field
// (nullptr) is an error.
SettingsError fillFromForm(const char* wifiSsid, const char* wifiPassword, const char* brokerHost,
                           const char* brokerPort, const char* mqttUser, const char* mqttPassword,
                           NetworkSettings& out);

// Checks settings read back from NVS.
SettingsError validate(const NetworkSettings& settings);

// What the portal shows for an error, in French without accents.
const char* describe(SettingsError error);

}  // namespace terrarium::supervision
