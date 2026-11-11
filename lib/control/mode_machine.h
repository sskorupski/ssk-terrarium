// Operating modes and the outputs each one allows (cadrage, modes table).
// Pure logic: no hardware, no allocation.
#pragma once

#include <cstdint>

#include "ventilation.h"

namespace terrarium::control {

enum class Mode : uint8_t {
  kActive,       // ACTIF: setpoint heating, misting, light schedule.
  kHibernation,  // HIBERNATION: frost protection only.
  kMaintenance,  // MAINTENANCE: everything off, measurements only.
  kSchedule,     // PLAGES: setpoint heating in time windows, floor outside.
};

enum class HeatingPolicy : uint8_t {
  kOff,
  kSetpoint,         // Regulate around the configured setpoint.
  kFrostProtection,  // Regulate in the frost band only.
  kSchedule,         // Setpoint in a time window, floor band outside (RG-chauffage-par-plages).
};

struct ModePolicy {
  HeatingPolicy heating;
  bool mistAllowed;
  bool lightAllowed;
  FanPolicy fan;  // RG-ventilation-periodique.
};

// Policy of a mode. Constant per mode, independent of the network.
ModePolicy policyFor(Mode mode);

// Wire names used by MQTT and the web page: "ACTIF", "HIBERNATION", "MAINTENANCE",
// "PLAGES".
const char* toString(Mode mode);

// Parses a wire name (exact, case-sensitive). Returns false and leaves `out`
// untouched on anything else, including nullptr.
bool parseMode(const char* text, Mode& out);

// Byte stored in NVS for a mode (RG-mode-au-demarrage).
uint8_t modeToStored(Mode mode);

// Reads a stored byte back. Returns false and leaves `out` untouched on any
// value that is not a known mode: the caller then starts in MAINTENANCE.
bool modeFromStored(uint8_t stored, Mode& out);

// Current mode. Changes only on an explicit request (cadrage: switching is
// manual, to avoid an untimely wake-up).
class ModeMachine {
 public:
  explicit constexpr ModeMachine(Mode initial) : mode_(initial) {}

  Mode mode() const { return mode_; }
  ModePolicy policy() const { return policyFor(mode_); }

  // Switches to `target`. Returns true if the mode changed.
  bool request(Mode target);

 private:
  Mode mode_;
};

}  // namespace terrarium::control
