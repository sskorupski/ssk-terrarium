// Cuts heating on a hot substrate (RG-coupure-par-le-substrat). Pure logic:
// no hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::control {

// What one reading changed, for the serial announcements.
enum class SubstrateCapChange : uint8_t {
  kNone,
  kCut,            // The substrate reached the cut threshold.
  kResumedCold,    // The substrate fell below the resume threshold.
  kResumedSilent,  // The probe stayed silent too long during a cut.
};

// Whether a substrate reading can be true: -20..60 °C. NaN is rejected.
bool isPlausibleSubstrateReading(float temperatureC);

class SubstrateCap {
 public:
  constexpr SubstrateCap(float cutAtC, float resumeBelowC, uint32_t maxSilentMs)
      : cutAtC_(cutAtC), resumeBelowC_(resumeBelowC), maxSilentMs_(maxSilentMs) {}

  // Feeds the reading taken at `nowMs` (millis(), wrap-safe). A cut starts on
  // a valid reading at or above cutAtC and ends on a valid reading strictly
  // below resumeBelowC, or once readings have been invalid for more than
  // maxSilentMs. Without a probe, it never cuts.
  SubstrateCapChange update(uint32_t nowMs, bool valid, float temperatureC);

  bool cut() const { return cut_; }

 private:
  float cutAtC_;
  float resumeBelowC_;
  uint32_t maxSilentMs_;
  bool cut_ = false;
  bool silent_ = false;
  uint32_t silentSinceMs_ = 0;
};

}  // namespace terrarium::control
