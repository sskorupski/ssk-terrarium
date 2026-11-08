// Wait between two reconnection attempts, doubling after each failure. Pure
// logic: no hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::supervision {

class Backoff {
 public:
  constexpr Backoff(uint32_t minMs, uint32_t maxMs) : minMs_(minMs), maxMs_(maxMs) {}

  // Whether an attempt may start at `nowMs` (millis(), wrap-safe).
  bool ready(uint32_t nowMs) const { return !waiting_ || nowMs - failedAtMs_ >= waitMs_; }

  void failed(uint32_t nowMs);
  void succeeded() { waiting_ = false; }

 private:
  uint32_t minMs_;
  uint32_t maxMs_;
  bool waiting_ = false;
  uint32_t waitMs_ = 0;
  uint32_t failedAtMs_ = 0;
};

}  // namespace terrarium::supervision
