// The four power outputs (relay channels). Every write goes through here so that the safety
// layer has a single place to enforce limits.
#pragma once

#include <cstdint>

namespace terrarium::outputs {

struct OutputPins {
  uint8_t heater;
  uint8_t light;
  uint8_t mistPump;
  uint8_t fan;
  uint8_t offLevel;  // Level that releases a relay, per the module trigger level.
};

struct OutputState {
  bool heater;
  bool light;
  bool mistPump;
  bool fan;
};

class Outputs {
 public:
  explicit constexpr Outputs(OutputPins pins) : pins_(pins) {}

  // Configures the pins and drives every output off.
  void begin();
  void apply(const OutputState& state);
  void allOff();
  const OutputState& state() const { return state_; }

 private:
  void write(uint8_t pin, bool on) const;

  OutputPins pins_;
  OutputState state_{false, false, false, false};
};

}  // namespace terrarium::outputs
