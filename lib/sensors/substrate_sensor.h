// Substrate temperature on the warm side: optional DS18B20 on 1-Wire (cadrage F2).
#pragma once

#include <DallasTemperature.h>
#include <OneWire.h>

#include <cstdint>

namespace terrarium::sensors {

struct SubstrateReading {
  float temperatureC;
  bool valid;  // False when absent or disconnected. The probe is informative only.
};

class SubstrateSensor {
 public:
  explicit SubstrateSensor(uint8_t pin) : oneWire_(pin), probes_(&oneWire_) {}

  // Returns whether a probe answered. The probe is optional: false is not an error.
  bool begin();
  // Latest completed conversion; invalid when absent or disconnected.
  SubstrateReading read();

 private:
  OneWire oneWire_;
  DallasTemperature probes_;
  SubstrateReading last_{0.0f, false};
};

}  // namespace terrarium::sensors
