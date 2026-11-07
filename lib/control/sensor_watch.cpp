#include "sensor_watch.h"

namespace terrarium::control {

SensorHealth SensorWatch::update(uint32_t nowMs, bool valid) {
  if (valid) {
    health_ = SensorHealth::kOk;
  } else if (health_ == SensorHealth::kOk) {
    invalidSinceMs_ = nowMs;
    health_ = SensorHealth::kHolding;
  } else if (nowMs - invalidSinceMs_ > maxSilentMs_) {
    health_ = SensorHealth::kFault;
  }
  return health_;
}

}  // namespace terrarium::control
