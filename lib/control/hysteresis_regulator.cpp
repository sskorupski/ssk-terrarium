#include "hysteresis_regulator.h"

#include <cmath>

namespace terrarium::control {

bool HysteresisRegulator::update(float measuredC, bool measurementValid) {
  if (!measurementValid || std::isnan(measuredC)) {
    on_ = false;
  } else if (measuredC < onBelowC_) {
    on_ = true;
  } else if (measuredC >= offAtC_) {
    on_ = false;
  }
  return on_;
}

}  // namespace terrarium::control
