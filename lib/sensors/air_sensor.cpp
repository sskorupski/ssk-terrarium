#include "air_sensor.h"

#include <Wire.h>

namespace terrarium::sensors {

namespace {

constexpr int16_t kNoError = 0;

}  // namespace

bool AirSensor::begin() {
  sht_.begin(Wire, SHT45_I2C_ADDR_44);
  if (sht_.softReset() != kNoError) return false;
  delay(2);  // Soft reset takes at most 1 ms.
  uint32_t serialNumber = 0;
  return sht_.serialNumber(serialNumber) == kNoError;
}

// High precision: about 8 ms on the bus, once every 30 s. Any bus or CRC
// error makes the reading invalid; the regulator decides what follows.
AirReading AirSensor::read() {
  float temperatureC = 0.0f;
  float humidityRhPercent = 0.0f;
  if (sht_.measureHighPrecision(temperatureC, humidityRhPercent) != kNoError) {
    return {0.0f, 0.0f, false};
  }
  return {temperatureC, humidityRhPercent, true};
}

bool AirSensor::heat() {
  float discardedTemperatureC = 0.0f;
  float discardedHumidityRhPercent = 0.0f;
  return sht_.activateHighestHeaterPowerLong(discardedTemperatureC, discardedHumidityRhPercent) ==
         kNoError;
}

}  // namespace terrarium::sensors
