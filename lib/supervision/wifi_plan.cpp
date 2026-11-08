#include "wifi_plan.h"

namespace terrarium::supervision {

bool WifiPlan::update(uint32_t nowMs, bool hasSettings, bool connected) {
  if (!started_) {
    started_ = true;
    downSinceMs_ = nowMs;
  }
  if (forced_ && nowMs - forcedSinceMs_ >= forcedMs_) forced_ = false;
  if (connected) {
    downSinceMs_ = nowMs;
    awaitingJoin_ = false;
  }
  if (!hasSettings || forced_ || awaitingJoin_) return true;
  return !connected && nowMs - downSinceMs_ >= failureMs_;
}

void WifiPlan::requestPortal(uint32_t nowMs) {
  forced_ = true;
  forcedSinceMs_ = nowMs;
}

void WifiPlan::settingsSaved(uint32_t nowMs) {
  forced_ = false;
  awaitingJoin_ = true;
  started_ = true;
  downSinceMs_ = nowMs;
}

}  // namespace terrarium::supervision
