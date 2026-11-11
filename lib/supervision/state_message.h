// The JSON published on ssk-terrarium/<id>/state (RG-topics-mqtt). Pure logic:
// no hardware, no allocation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "air_probes.h"
#include "heat_settings.h"
#include "mode_machine.h"
#include "posix_tz.h"
#include "sensor_watch.h"
#include "time_windows.h"
#include "ventilation.h"

namespace terrarium::supervision {

// Largest state JSON: four windows, the longest time zone, the longest cause
// of rejection, both probes, the ventilation and the bounds fit
// (test_longest_state_fits).
constexpr size_t kStateSize = 1280;

// One air probe as published in "sondes" (RG-topics-mqtt, cadrage 2026-007).
struct ProbeState {
  control::ProbeRole role;
  control::AirRejection rejection;  // kNone: the reading is accepted and published.
  float temperatureC;
  float humidityRhPercent;
  bool fault;  // Rejected for more than 2 minutes (RG-capteur-en-defaut).
};

// What the regulation loop hands to the supervision task, every cycle.
struct StateSnapshot {
  bool airValid;  // An interior probe gives the air measure (RG-sondes-d-air).
  float airTemperatureC;
  float airHumidityRhPercent;
  control::AirSource airSource;        // "sonde_retenue".
  control::AirRejection airRejection;  // "cause_mesure_air": the priority interior probe's.
  bool airSafety;                      // The owner keeps the interior air's safety on.
  control::ProbeCombination combination;
  ProbeState probes[control::kProbeCount];  // Indexed by control::Probe.
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
  control::FanSettings fanSettings;       // "ventilation" (RG-ventilation-periodique).
  control::FanSettingsBounds fanBounds;   // Compile-time bounds (RG-reglages-bornes).
};

// Writes the JSON into `out`. Returns its length, or 0 if it does not fit.
size_t formatState(const StateSnapshot& state, char* out, size_t size);

}  // namespace terrarium::supervision
