#include "dht_sensor.h"

#include <cmath>

namespace terrarium::sensors {

void DhtSensor::begin() { dht_.begin(); }

// About 20 ms with interrupts off, once every 30 s: far from the watchdog.
// The library returns NaN on a missing answer or a checksum error.
AirReading DhtSensor::read() {
  // force = true: the library would otherwise return a reading up to 2 s old.
  const float temperatureC = dht_.readTemperature(/*S=*/false, /*force=*/true);
  const float humidityRhPercent = dht_.readHumidity(/*force=*/false);
  if (std::isnan(temperatureC) || std::isnan(humidityRhPercent)) return {0.0f, 0.0f, false};
  return {temperatureC, humidityRhPercent, true};
}

}  // namespace terrarium::sensors
