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
  // Heats the chip 1 s at its highest power, 200 mW, to dry it out of
  // condensation (RG-chauffe-de-la-sht45). Blocks for about 1.1 s. The reading
  // taken at the end of the pulse is skewed by the heat: it is thrown away.
  // Returns false on any bus or sensor error.
  bool heat();

 private:
  SensirionI2cSht4x sht_;
};

}  // namespace terrarium::sensors
