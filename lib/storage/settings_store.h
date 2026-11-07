// User settings persisted in NVS (cadrage NF4). Safety limits are not settings:
// they live in config.h and cannot be changed at runtime.
#pragma once

#include <cstdint>

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
};

}  // namespace terrarium::storage
