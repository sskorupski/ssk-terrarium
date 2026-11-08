#include "heat_settings.h"

namespace terrarium::control {

bool isAcceptedHeatSettings(const HeatSettings& settings, const HeatSettingsBounds& bounds) {
  // Written so that NaN fails every comparison and is refused.
  const bool setpointOk =
      settings.setpointC >= bounds.setpointMinC && settings.setpointC <= bounds.setpointMaxC;
  const bool hysteresisOk = settings.hysteresisC >= bounds.hysteresisMinC &&
                            settings.hysteresisC <= bounds.hysteresisMaxC;
  return setpointOk && hysteresisOk &&
         settings.setpointC + settings.hysteresisC < bounds.stopBelowC;
}

}  // namespace terrarium::control
