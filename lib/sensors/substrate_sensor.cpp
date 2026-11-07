#include "substrate_sensor.h"

namespace terrarium::sensors {

// TODO(phase 1): probes_.begin(), non-blocking conversions (setWaitForConversion(false)).
bool SubstrateSensor::begin() { return false; }

// TODO(phase 1): getTempCByIndex(0); DEVICE_DISCONNECTED_C means valid = false.
SubstrateReading SubstrateSensor::read() { return {0.0f, false}; }

}  // namespace terrarium::sensors
