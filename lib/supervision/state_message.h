// The JSON published on ssk-terrarium/<id>/state (RG-topics-mqtt). Pure logic:
// no hardware, no allocation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "heat_settings.h"
#include "mode_machine.h"
#include "posix_tz.h"
#include "time_windows.h"

namespace terrarium::supervision {

// Largest state JSON: four windows, the longest time zone and the bounds fit
// (618 bytes with a 30-character version, test_longest_state_fits).
constexpr size_t kStateSize = 640;

// What the regulation loop hands to the supervision task, every cycle.
struct StateSnapshot {
  bool airValid;
  float airTemperatureC;
  float airHumidityRhPercent;
  bool substrateValid;
  float substrateTemperatureC;
  bool heater;
  bool light;
  bool mistPump;
  bool fan;
  control::Mode mode;
  float setpointC;
  float hysteresisC;
  bool sensorFault;
  bool heatPaused;
  bool substrateCut;  // RG-coupure-par-le-substrat.
  uint32_t uptimeS;
  const char* firmwareVersion;            // Static string; replaced if it would break the JSON.
  control::TimeWindows windows;           // PLAGES windows.
  char timezone[control::kTimezoneSize];  // POSIX TZ; replaced by "" if invalid.
  bool timeKnown;                         // NTP answered since boot.
  bool inWindow;                          // Meaningful only when timeKnown.
  control::HeatSettingsBounds bounds;     // Compile-time bounds (RG-reglages-bornes).
};

// Writes the JSON into `out`. Returns its length, or 0 if it does not fit.
size_t formatState(const StateSnapshot& state, char* out, size_t size);

}  // namespace terrarium::supervision
