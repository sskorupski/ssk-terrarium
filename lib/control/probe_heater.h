// When the SHT45 heats itself out of condensation (RG-chauffe-de-la-sht45).
// Pure logic: no hardware, no allocation. The pulse itself, and discarding the
// reading it returns, belong to the sensor driver.
#pragma once

#include <cstdint>

#include "sensor_watch.h"

namespace terrarium::control {

// Why the SHT45 heats, as published on MQTT.
enum class HeatReason : uint8_t {
  kNone,                // No heating.
  kHighHumidity,        // Accepted reading at or above the threshold.
  kHumidityOutOfRange,  // Reading rejected for its humidity (RG-capteur-en-defaut).
};

// The reason as published on MQTT, or nullptr for kNone.
const char* heatReasonName(HeatReason reason);

class ProbeHeater {
 public:
  constexpr ProbeHeater(float minRhPercent, uint32_t minIntervalMs)
      : minRhPercent_(minRhPercent), minIntervalMs_(minIntervalMs) {}

  // Fed with the SHT45 reading just taken at `nowMs` (millis(), wrap-safe):
  // whether to heat right now, and why. A due heating counts as started at
  // `nowMs`, even if the sensor then refuses it, so a failing pulse is not
  // retried at every reading. A silent sensor, or one rejected for its
  // temperature, never heats.
  HeatReason due(uint32_t nowMs, AirRejection rejection, float humidityRhPercent);

 private:
  float minRhPercent_;
  uint32_t minIntervalMs_;
  bool heated_ = false;
  uint32_t lastStartMs_ = 0;
};

}  // namespace terrarium::control
