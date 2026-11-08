// Commands received over MQTT and the events that answer them (RG-topics-mqtt).
// Parsing only: whether settings are within bounds is decided by the
// regulation loop (RG-reglages-bornes). No hardware.
#pragma once

#include <cstddef>
#include <cstdint>

#include "heat_settings.h"
#include "mode_machine.h"
#include "posix_tz.h"
#include "time_windows.h"

namespace terrarium::supervision {

constexpr size_t kEventSize = 192;

// cmd/mode: the bare wire name, exactly. Returns false and leaves `out`
// untouched on anything else.
bool parseModePayload(const uint8_t* payload, size_t length, control::Mode& out);

enum class ConfigError : uint8_t {
  kNone,
  kMalformed,     // Not a JSON object.
  kUnknownField,  // A field other than those below.
  kNotANumber,    // "consigne" or "hysteresis" is not a number.
  kEmpty,         // No field at all.
  kBadWindows,    // "plages" is not a list of 0 to 4 valid windows.
  kBadTimezone,   // "fuseau" is not a valid POSIX TZ string.
};

// What a cmd/config carries. Each part is applied only if present.
struct ConfigCommand {
  bool hasHeat;
  control::HeatSettings heat;  // Merged with the current values.
  bool hasWindows;
  control::TimeWindows windows;  // Replaces the current list.
  bool hasTimezone;
  char timezone[control::kTimezoneSize];
};

// cmd/config: a JSON object with any of "consigne" and "hysteresis" in C,
// "plages" (a list of "HH:MM-HH:MM") and "fuseau" (a POSIX TZ string).
// Missing heat fields keep their `current` value. On any error, `out` is left
// untouched: the command is refused whole (RG-reglages-bornes).
ConfigError parseConfigPayload(const uint8_t* payload, size_t length,
                               const control::HeatSettings& current, ConfigCommand& out);

const char* describe(ConfigError error);

// Refuses commands for a while after each subscription: the broker delivers
// retained messages right then, and a retained command is never applied.
class CommandGate {
 public:
  explicit constexpr CommandGate(uint32_t windowMs) : windowMs_(windowMs) {}

  void subscribed(uint32_t nowMs) {
    open_ = true;
    subscribedAtMs_ = nowMs;
  }
  void disconnected() { open_ = false; }
  bool accepts(uint32_t nowMs) const { return open_ && nowMs - subscribedAtMs_ >= windowMs_; }

 private:
  uint32_t windowMs_;
  bool open_ = false;
  uint32_t subscribedAtMs_ = 0;
};

enum class ModeOutcome : uint8_t {
  kApplied,
  kAlreadyActive,
  kNotSaved,  // Applied, but lost at the next power cut.
};

// Events published on ssk-terrarium/<id>/event. Return false if truncated.
bool formatModeEvent(control::Mode mode, ModeOutcome outcome, char (&out)[kEventSize]);
bool formatSettingsEvent(const control::HeatSettings& settings, bool applied,
                         char (&out)[kEventSize]);
bool formatWindowsEvent(const control::TimeWindows& windows, char (&out)[kEventSize]);
// `timezone` must be a valid POSIX TZ string: it is quoted verbatim.
bool formatTimezoneEvent(const char* timezone, char (&out)[kEventSize]);
// `subtopic` and `reason` are fixed strings of the firmware, quoted verbatim.
bool formatRejectedCommandEvent(const char* subtopic, const char* reason, char (&out)[kEventSize]);

}  // namespace terrarium::supervision
