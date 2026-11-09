// User settings persisted in NVS (cadrage NF4). Safety limits are not settings:
// they live in config.h and cannot be changed at runtime.
#pragma once

#include <cstdint>

#include "heat_settings.h"
#include "identity.h"
#include "network_settings.h"
#include "posix_tz.h"
#include "time_windows.h"

namespace terrarium::storage {

struct Settings {
  float setpointC;
  float heatHysteresisC;
  float mistThresholdRhPercent;
  uint32_t mistPulseMs;
  uint32_t mistMinIntervalMs;
  uint8_t lightOnHour;
  uint8_t lightOffHour;
  uint8_t mode;  // control::Mode, stored as its underlying value.
};

class SettingsStore {
 public:
  // Fills `out` with the stored settings. Returns false, leaving `out`
  // untouched, when nothing valid is stored: callers keep the defaults.
  bool load(Settings& out);
  bool save(const Settings& settings);

  // Operating mode alone, saved on every change (RG-mode-au-demarrage).
  // loadMode returns false, leaving `out` untouched, when nothing is stored.
  bool loadMode(uint8_t& out);
  bool saveMode(uint8_t mode);

  // Whether the owner keeps the air sensor's safety on (RG-capteur-en-defaut).
  // loadAirSafety returns false, leaving `out` untouched, when nothing is
  // stored; only a stored 0 reads as off, anything else as on.
  bool loadAirSafety(bool& out);
  bool saveAirSafety(bool enabled);

  // Setpoint and hysteresis of ACTIF heating (RG-mode-au-demarrage). Returns
  // false, leaving `out` untouched, when none is stored; the caller checks the
  // bounds itself (RG-reglages-bornes).
  bool loadHeatSettings(control::HeatSettings& out);
  bool saveHeatSettings(const control::HeatSettings& settings);

  // Windows and time zone of PLAGES (RG-mode-au-demarrage). Return false,
  // leaving `out` untouched, when none is stored; the caller validates.
  bool loadTimeWindows(control::TimeWindows& out);
  bool saveTimeWindows(const control::TimeWindows& windows);
  bool loadTimezone(char (&out)[control::kTimezoneSize]);
  bool saveTimezone(const char* timezone);

  // Wi-Fi and broker settings (RG-configuration-wifi-sur-place). loadNetwork
  // returns false, leaving `out` untouched, when nothing valid is stored.
  bool loadNetwork(supervision::NetworkSettings& out);
  bool saveNetwork(const supervision::NetworkSettings& settings);

  // Password of the configuration access point: drawn at the first boot,
  // then kept. Returns false only if NVS cannot be written.
  bool loadOrCreateApPassword(char (&out)[supervision::kApPasswordLength + 1]);
};

}  // namespace terrarium::storage
