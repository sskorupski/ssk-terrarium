// Setpoint and hysteresis of ACTIF heating, settable from the network within
// fixed bounds (RG-reglages-bornes, INV-5). Pure logic: no hardware, no
// allocation.
#pragma once

namespace terrarium::control {

struct HeatSettings {
  float setpointC;
  float hysteresisC;
};

// Compile-time bounds, from config.h.
struct HeatSettingsBounds {
  float setpointMinC;
  float setpointMaxC;
  float hysteresisMinC;
  float hysteresisMaxC;
  float stopBelowC;  // Heating must stop strictly below it: the high alert.
};

// Whether `settings` lies within `bounds`. NaN and infinities are refused.
bool isAcceptedHeatSettings(const HeatSettings& settings, const HeatSettingsBounds& bounds);

}  // namespace terrarium::control
