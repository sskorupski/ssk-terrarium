#include "outputs.h"

#include <Arduino.h>

namespace terrarium::outputs {

void Outputs::begin() {
  pinMode(pins_.heater, OUTPUT);
  pinMode(pins_.light, OUTPUT);
  pinMode(pins_.mistPump, OUTPUT);
  pinMode(pins_.fan, OUTPUT);
  allOff();
}

void Outputs::apply(const OutputState& state) {
  // TODO(phase 4): enforce kMistPulseMaxMs here, when misting is wired.
  write(pins_.heater, state.heater);
  write(pins_.light, state.light);
  write(pins_.mistPump, state.mistPump);
  write(pins_.fan, state.fan);
  state_ = state;
}

void Outputs::allOff() { apply({false, false, false, false}); }

void Outputs::write(uint8_t pin, bool on) const {
  digitalWrite(pin, on ? !pins_.offLevel : pins_.offLevel);
}

}  // namespace terrarium::outputs
