#include "air_sensor.h"

namespace terrarium::sensors {

// TODO(phase 1): sht_.begin(Wire, SHT45_I2C_ADDR_44), soft reset, serial number check.
bool AirSensor::begin() { return false; }

// TODO(phase 1): measureHighPrecision(); map any error to valid = false.
AirReading AirSensor::read() { return {0.0f, 0.0f, false}; }

}  // namespace terrarium::sensors
