#include "sensor_watch.h"

#include <cmath>

namespace terrarium::control {

namespace {

constexpr float kAirMinC = -20.0f;
constexpr float kAirMaxC = 60.0f;

}  // namespace

// Written so that NaN, which fails every comparison, is rejected.
AirRejection classifyAirReading(bool answered, float temperatureC, float humidityRhPercent) {
  if (!answered) return AirRejection::kSilent;
  if (!(temperatureC >= kAirMinC && temperatureC <= kAirMaxC)) {
    return AirRejection::kTemperatureOutOfRange;
  }
  if (!(humidityRhPercent >= 0.0f && humidityRhPercent <= 100.0f)) {
    return AirRejection::kHumidityOutOfRange;
  }
  return AirRejection::kNone;
}

const char* rejectionName(AirRejection rejection) {
  switch (rejection) {
    case AirRejection::kNone:
      return nullptr;
    case AirRejection::kSilent:
      return "muet";
    case AirRejection::kTemperatureOutOfRange:
      return "temperature-hors-plage";
    case AirRejection::kHumidityOutOfRange:
      return "humidite-hors-plage";
  }
  return nullptr;
}

bool isPlausibleAirReading(float temperatureC, float humidityRhPercent) {
  return classifyAirReading(true, temperatureC, humidityRhPercent) == AirRejection::kNone;
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

SensorHealth SensorWatch::update(uint32_t nowMs, AirRejection rejection, bool safetyEnabled) {
  rejection_ = rejection;
  if (rejection == AirRejection::kNone || safetyEnabled) {
    return update(nowMs, rejection == AirRejection::kNone);
  }
  if (rejection != AirRejection::kSilent) {
    health_ = SensorHealth::kOk;
    return health_;
  }
  // Silent, safety off: the silence still counts from its start, so that
  // re-enabling the safety on a long silence declares the fault at once.
  if (health_ == SensorHealth::kOk) invalidSinceMs_ = nowMs;
  health_ = SensorHealth::kHolding;
  return health_;
}

}  // namespace terrarium::control
