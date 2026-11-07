// Tells a silent sensor from a faulty one (RG-capteur-en-defaut). Pure logic:
// no hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::control {

enum class SensorHealth : uint8_t {
  kOk,       // Last reading valid: regulate on it.
  kHolding,  // Invalid for at most the limit: keep the outputs as they are.
  kFault,    // Invalid beyond the limit: heating and misting off.
};

// Whether an air reading can be true in the garage terrarium. Anything outside
// -20..60 °C or 0..100 %RH, or NaN, comes from a faulty sensor or bus, and
// counts as no reading (RG-capteur-en-defaut).
bool isPlausibleAirReading(float temperatureC, float humidityRhPercent);

class SensorWatch {
 public:
  explicit constexpr SensorWatch(uint32_t maxSilentMs) : maxSilentMs_(maxSilentMs) {}

  // Feeds the validity of the reading taken at `nowMs` (millis(), wrap-safe).
  // Silence counts from the first invalid reading, including right after boot.
  SensorHealth update(uint32_t nowMs, bool valid);

  SensorHealth health() const { return health_; }

 private:
  uint32_t maxSilentMs_;
  uint32_t invalidSinceMs_ = 0;
  SensorHealth health_ = SensorHealth::kOk;
};

}  // namespace terrarium::control
