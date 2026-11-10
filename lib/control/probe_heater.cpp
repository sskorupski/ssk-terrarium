#include "probe_heater.h"

namespace terrarium::control {

const char* heatReasonName(HeatReason reason) {
  switch (reason) {
    case HeatReason::kHighHumidity:
      return "humidite-haute";
    case HeatReason::kHumidityOutOfRange:
      return "humidite-hors-plage";
    case HeatReason::kNone:
      break;
  }
  return nullptr;
}

HeatReason ProbeHeater::due(uint32_t nowMs, AirRejection rejection, float humidityRhPercent) {
  HeatReason reason = HeatReason::kNone;
  if (rejection == AirRejection::kHumidityOutOfRange) {
    reason = HeatReason::kHumidityOutOfRange;
  } else if (rejection == AirRejection::kNone && humidityRhPercent >= minRhPercent_) {
    reason = HeatReason::kHighHumidity;
  }
  if (reason == HeatReason::kNone) return reason;
  if (heated_ && nowMs - lastStartMs_ < minIntervalMs_) return HeatReason::kNone;
  heated_ = true;
  lastStartMs_ = nowMs;
  return reason;
}

}  // namespace terrarium::control
