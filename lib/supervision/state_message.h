// The JSON published on ssk-terrarium/<id>/state (RG-topics-mqtt). Pure logic:
// no hardware, no allocation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "heat_settings.h"
#include "mode_machine.h"
#include "posix_tz.h"
#include "sensor_watch.h"
#include "time_windows.h"

namespace terrarium::supervision {

// Largest state JSON: four windows, the longest time zone, the longest cause
// of rejection and the bounds fit (test_longest_state_fits).
constexpr size_t kStateSize = 704;

// What the regulation loop hands to the supervision task, every cycle.
struct StateSnapshot {
  bool airValid;
  float airTemperatureC;
  float airHumidityRhPercent;
  control::AirRejection airRejection;  // Why the last air reading was rejected, if it was.
  bool airSafety;                      // The owner keeps the air sensor's safety on.
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
