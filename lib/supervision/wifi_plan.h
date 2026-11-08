// When the configuration access point is up (RG-configuration-wifi-sur-place).
// Pure logic: no hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::supervision {

class WifiPlan {
 public:
  // failureMs: how long the saved Wi-Fi may stay down before the portal opens
  // beside it. forcedMs: how long a portal opened on request stays up.
  constexpr WifiPlan(uint32_t failureMs, uint32_t forcedMs)
      : failureMs_(failureMs), forcedMs_(forcedMs) {}

  // Returns whether the access point should be up at `nowMs` (millis(),
  // wrap-safe). Call it regularly.
  bool update(uint32_t nowMs, bool hasSettings, bool connected);

  // WIFI CONFIGURER on the serial link: opens the portal even while connected.
  void requestPortal(uint32_t nowMs);

  // New settings were saved from the portal: the portal stays up until the new
  // Wi-Fi is joined, then closes, a requested one included.
  void settingsSaved(uint32_t nowMs);

 private:
  uint32_t failureMs_;
  uint32_t forcedMs_;
  bool started_ = false;
  uint32_t downSinceMs_ = 0;
  bool awaitingJoin_ = false;
  bool forced_ = false;
  uint32_t forcedSinceMs_ = 0;
};

}  // namespace terrarium::supervision
