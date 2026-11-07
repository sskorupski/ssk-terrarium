// Air temperature and humidity: Sensirion SHT45 on the shared I2C bus.
#pragma once

#include <SensirionI2cSht4x.h>

namespace terrarium::sensors {

struct AirReading {
  float temperatureC;
  float humidityRhPercent;
  bool valid;  // False on any bus or sensor error: consumers must cut outputs.
};

class AirSensor {
 public:
  // Expects Wire to be started on the configured pins.
  bool begin();
  AirReading read();

 private:
  SensirionI2cSht4x sht_;
};

}  // namespace terrarium::sensors
