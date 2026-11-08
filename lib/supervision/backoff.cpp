#include "backoff.h"

namespace terrarium::supervision {

void Backoff::failed(uint32_t nowMs) {
  if (!waiting_) {
    waitMs_ = minMs_;
  } else {
    waitMs_ = waitMs_ > maxMs_ / 2 ? maxMs_ : waitMs_ * 2;
  }
  waiting_ = true;
  failedAtMs_ = nowMs;
}

}  // namespace terrarium::supervision
