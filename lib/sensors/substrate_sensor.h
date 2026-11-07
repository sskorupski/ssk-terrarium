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

  bool begin();
  SubstrateReading read();

 private:
  OneWire oneWire_;
  DallasTemperature probes_;
};

}  // namespace terrarium::sensors
