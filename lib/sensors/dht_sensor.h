// Second air probe: a DHT11 (KY-015 module) on its own pin (RG-brochage,
// cadrage 2026-007). Its role, interior or exterior, is a setting
// (RG-sondes-d-air).
#pragma once

#include <DHT.h>

#include <cstdint>

#include "air_sensor.h"

namespace terrarium::sensors {

class DhtSensor {
 public:
  explicit DhtSensor(uint8_t pin) : dht_(pin, DHT11) {}

  void begin();
  // An absent probe, or a checksum error, reads as invalid: a silent probe.
  AirReading read();

 private:
  DHT dht_;
};

}  // namespace terrarium::sensors
