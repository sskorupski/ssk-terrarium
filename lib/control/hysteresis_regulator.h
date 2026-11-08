// On/off regulator with a dead band. Pure logic: no hardware, no allocation.
#pragma once

namespace terrarium::control {

class HysteresisRegulator {
 public:
  // Output turns on below `onBelowC` and off at or above `offAtC`; in between
  // it keeps its previous state. Starts off.
  constexpr HysteresisRegulator(float onBelowC, float offAtC)
      : onBelowC_(onBelowC), offAtC_(offAtC) {}

  // Band centred on `setpointC`, `hysteresisC` wide on each side.
  static constexpr HysteresisRegulator aroundSetpoint(float setpointC, float hysteresisC) {
    return HysteresisRegulator(setpointC - hysteresisC, setpointC + hysteresisC);
  }

  // Feeds one measurement and returns the new output. An invalid measurement
  // (flagged invalid, or NaN) forces the output off: a regulator that cannot
  // see must not heat.
  bool update(float measuredC, bool measurementValid);

  // Moves the band to `setpointC` +/- `hysteresisC`, keeping the current output
  // until the next measurement decides (RG-reglages-bornes).
  void retune(float setpointC, float hysteresisC) {
    onBelowC_ = setpointC - hysteresisC;
    offAtC_ = setpointC + hysteresisC;
  }

  // Forces the output off, e.g. when the mode no longer allows it.
  void reset() { on_ = false; }

  bool isOn() const { return on_; }

 private:
  float onBelowC_;
  float offAtC_;
  bool on_ = false;
};

}  // namespace terrarium::control
