#include "substrate_sensor.h"

namespace terrarium::sensors {

namespace {

// Value a DS18B20 holds after power-up, before its first conversion.
constexpr float kPowerOnResetC = 85.0f;

}  // namespace

// Conversions run in the background: read() collects the previous one and
// starts the next, so the loop never waits the 750 ms of a 12-bit conversion.
bool SubstrateSensor::begin() {
  probes_.begin();
  probes_.setWaitForConversion(false);
  const bool found = probes_.getDeviceCount() > 0;
  probes_.requestTemperatures();
  return found;
}

SubstrateReading SubstrateSensor::read() {
  if (!probes_.isConversionComplete()) return last_;
  const float temperatureC = probes_.getTempCByIndex(0);
  const bool valid = temperatureC != DEVICE_DISCONNECTED_C && temperatureC != kPowerOnResetC;
  last_ = {temperatureC, valid};
  probes_.requestTemperatures();
  return last_;
}

}  // namespace terrarium::sensors
