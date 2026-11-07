#include "heat_guard.h"

namespace terrarium::control {

bool HeatGuard::filter(uint32_t nowMs, bool requested) {
  if (paused_) {
    if (nowMs - pausedSinceMs_ < pauseMs_) return false;
    paused_ = false;
  }
  if (!requested) {
    on_ = false;
    return false;
  }
  if (!on_) {
    on_ = true;
    onSinceMs_ = nowMs;
  } else if (nowMs - onSinceMs_ > maxOnMs_) {
    on_ = false;
    paused_ = true;
    pausedSinceMs_ = nowMs;
    return false;
  }
  return true;
}

}  // namespace terrarium::control
