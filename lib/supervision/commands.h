// Commands received over MQTT and the events that answer them (RG-topics-mqtt).
// Parsing only: whether settings are within bounds is decided by the
// regulation loop (RG-reglages-bornes). No hardware.
#pragma once

#include <cstddef>
#include <cstdint>

#include "heat_settings.h"
#include "mode_machine.h"

namespace terrarium::supervision {

constexpr size_t kEventSize = 192;

// cmd/mode: the bare wire name, exactly. Returns false and leaves `out`
// untouched on anything else.
bool parseModePayload(const uint8_t* payload, size_t length, control::Mode& out);

enum class ConfigError : uint8_t {
  kNone,
  kMalformed,     // Not a JSON object.
  kUnknownField,  // A field other than "consigne" and "hysteresis".
  kNotANumber,    // A known field whose value is not a number.
  kEmpty,         // No field at all.
};

// cmd/config: a JSON object with "consigne" and/or "hysteresis" in C. Missing
// fields keep their `current` value. On any error, `out` is left untouched.
ConfigError parseConfigPayload(const uint8_t* payload, size_t length,
                               const control::HeatSettings& current, control::HeatSettings& out);

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
// `subtopic` and `reason` are fixed strings of the firmware, quoted verbatim.
bool formatRejectedCommandEvent(const char* subtopic, const char* reason, char (&out)[kEventSize]);

}  // namespace terrarium::supervision
