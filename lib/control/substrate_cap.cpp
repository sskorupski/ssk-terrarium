#include "substrate_cap.h"

namespace terrarium::control {

namespace {

constexpr float kSubstrateMinC = -20.0f;
constexpr float kSubstrateMaxC = 60.0f;

}  // namespace

bool isPlausibleSubstrateReading(float temperatureC) {
  // Written so that NaN, which fails every comparison, is rejected.
  return temperatureC >= kSubstrateMinC && temperatureC <= kSubstrateMaxC;
}

SubstrateCapChange SubstrateCap::update(uint32_t nowMs, bool valid, float temperatureC) {
  if (!valid || !isPlausibleSubstrateReading(temperatureC)) {
    if (!cut_) return SubstrateCapChange::kNone;
    if (!silent_) {
      silent_ = true;
      silentSinceMs_ = nowMs;
    } else if (nowMs - silentSinceMs_ > maxSilentMs_) {
      cut_ = false;
      silent_ = false;
      return SubstrateCapChange::kResumedSilent;
    }
    return SubstrateCapChange::kNone;
  }

  silent_ = false;
  if (!cut_ && temperatureC >= cutAtC_) {
    cut_ = true;
    return SubstrateCapChange::kCut;
  }
  if (cut_ && temperatureC < resumeBelowC_) {
    cut_ = false;
    return SubstrateCapChange::kResumedCold;
  }
  return SubstrateCapChange::kNone;
}

}  // namespace terrarium::control
