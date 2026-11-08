#include "mode_machine.h"

#include <cstring>

namespace terrarium::control {

ModePolicy policyFor(Mode mode) {
  switch (mode) {
    case Mode::kActive:
      return {HeatingPolicy::kSetpoint, true, true};
    case Mode::kHibernation:
      return {HeatingPolicy::kFrostProtection, false, false};
    case Mode::kSchedule:
      // Light and fan follow ACTIF (RG-sorties-par-mode).
      return {HeatingPolicy::kSchedule, true, true};
    case Mode::kMaintenance:
      break;
  }
  // MAINTENANCE, and any out-of-range value: everything off.
  return {HeatingPolicy::kOff, false, false};
}

const char* toString(Mode mode) {
  switch (mode) {
    case Mode::kActive:
      return "ACTIF";
    case Mode::kHibernation:
      return "HIBERNATION";
    case Mode::kMaintenance:
      return "MAINTENANCE";
    case Mode::kSchedule:
      return "PLAGES";
  }
  return "MAINTENANCE";
}

bool parseMode(const char* text, Mode& out) {
  if (text == nullptr) return false;
  const Mode modes[] = {Mode::kActive, Mode::kHibernation, Mode::kMaintenance, Mode::kSchedule};
  for (Mode mode : modes) {
    if (std::strcmp(text, toString(mode)) == 0) {
      out = mode;
      return true;
    }
  }
  return false;
}

uint8_t modeToStored(Mode mode) { return static_cast<uint8_t>(mode); }

bool modeFromStored(uint8_t stored, Mode& out) {
  const Mode modes[] = {Mode::kActive, Mode::kHibernation, Mode::kMaintenance, Mode::kSchedule};
  for (Mode mode : modes) {
    if (stored == modeToStored(mode)) {
      out = mode;
      return true;
    }
  }
  return false;
}

bool ModeMachine::request(Mode target) {
  if (target == mode_) return false;
  mode_ = target;
  return true;
}

}  // namespace terrarium::control
