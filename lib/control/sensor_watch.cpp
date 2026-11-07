#include "sensor_watch.h"

#include <cmath>

namespace terrarium::control {

namespace {

constexpr float kAirMinC = -20.0f;
constexpr float kAirMaxC = 60.0f;

}  // namespace

bool isPlausibleAirReading(float temperatureC, float humidityRhPercent) {
  // Written so that NaN, which fails every comparison, is rejected.
  return temperatureC >= kAirMinC && temperatureC <= kAirMaxC && humidityRhPercent >= 0.0f &&
         humidityRhPercent <= 100.0f;
}

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
