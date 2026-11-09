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

// Why an air reading is rejected (RG-capteur-en-defaut), the first that
// applies: no answer, then temperature, then humidity.
enum class AirRejection : uint8_t {
  kNone,                   // Accepted.
  kSilent,                 // The sensor did not answer.
  kTemperatureOutOfRange,  // Outside -20..60 °C, or NaN.
  kHumidityOutOfRange,     // Outside 0..100 %RH, or NaN.
};

AirRejection classifyAirReading(bool answered, float temperatureC, float humidityRhPercent);

// The cause as published on MQTT, or nullptr for an accepted reading.
const char* rejectionName(AirRejection rejection);

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

  // Same, knowing why a reading is rejected and whether the owner keeps the
  // sensor's safety on (RG-capteur-en-defaut). With safety off, an out-of-range
  // reading is regulated on, and a silent sensor holds the outputs without
  // limit: never a fault.
  SensorHealth update(uint32_t nowMs, AirRejection rejection, bool safetyEnabled);

  // The cause of the last rejection, kNone once a reading is accepted.
  AirRejection rejection() const { return rejection_; }

  SensorHealth health() const { return health_; }

 private:
  uint32_t maxSilentMs_;
  uint32_t invalidSinceMs_ = 0;
  SensorHealth health_ = SensorHealth::kOk;
  AirRejection rejection_ = AirRejection::kNone;
};

}  // namespace terrarium::control
