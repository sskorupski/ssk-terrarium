// Caps continuous heating (RG-chauffe-continue-bornee). Pure logic: no
// hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::control {

class HeatGuard {
 public:
  constexpr HeatGuard(uint32_t maxOnMs, uint32_t pauseMs) : maxOnMs_(maxOnMs), pauseMs_(pauseMs) {}

  // Filters the regulator's request at `nowMs` (millis(), wrap-safe) and
  // returns whether the heater may be on. Heating on for more than maxOnMs
  // is cut for pauseMs, then follows the request again.
  bool filter(uint32_t nowMs, bool requested);

  bool paused() const { return paused_; }

 private:
  uint32_t maxOnMs_;
  uint32_t pauseMs_;
  bool on_ = false;
  bool paused_ = false;
  uint32_t onSinceMs_ = 0;
  uint32_t pausedSinceMs_ = 0;
};

}  // namespace terrarium::control
