#include <unity.h>

#include "wifi_plan.h"

using terrarium::supervision::WifiPlan;

namespace {

constexpr uint32_t kFailureMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kForcedMs = 15UL * 60UL * 1000UL;

}  // namespace

void setUp() {}
void tearDown() {}

// RG-configuration-wifi-sur-place: no Wi-Fi saved, the access point is up.
void test_portal_open_without_credentials() {
  WifiPlan plan(kFailureMs, kForcedMs);
  TEST_ASSERT_TRUE(plan.update(0, false, false));
  TEST_ASSERT_TRUE(plan.update(kFailureMs * 10, false, false));
}

void test_portal_closed_while_connected() {
  WifiPlan plan(kFailureMs, kForcedMs);
  TEST_ASSERT_FALSE(plan.update(0, true, true));
  TEST_ASSERT_FALSE(plan.update(kFailureMs * 10, true, true));
}

// The saved Wi-Fi gets 5 minutes before the portal opens alongside it.
void test_portal_opens_after_five_minutes_without_connection() {
  WifiPlan plan(kFailureMs, kForcedMs);
  TEST_ASSERT_FALSE(plan.update(1000, true, false));
  TEST_ASSERT_FALSE(plan.update(1000 + kFailureMs - 1, true, false));
  TEST_ASSERT_TRUE(plan.update(1000 + kFailureMs, true, false));
}

void test_portal_closes_once_connected() {
  WifiPlan plan(kFailureMs, kForcedMs);
  plan.update(0, true, false);
  TEST_ASSERT_TRUE(plan.update(kFailureMs, true, false));
  TEST_ASSERT_FALSE(plan.update(kFailureMs + 1, true, true));
}

// The 5 minutes count from the last time the Wi-Fi was up, not from boot.
void test_failure_delay_restarts_after_each_connection() {
  WifiPlan plan(kFailureMs, kForcedMs);
  plan.update(0, true, true);
  plan.update(kFailureMs * 3, true, true);
  TEST_ASSERT_FALSE(plan.update(kFailureMs * 3 + 1, true, false));
  TEST_ASSERT_FALSE(plan.update(kFailureMs * 4 - 1, true, false));
  TEST_ASSERT_TRUE(plan.update(kFailureMs * 4, true, false));
}

// WIFI CONFIGURER opens the portal even while connected.
void test_requested_portal_stays_open_while_connected() {
  WifiPlan plan(kFailureMs, kForcedMs);
  plan.update(0, true, true);
  plan.requestPortal(100);
  TEST_ASSERT_TRUE(plan.update(200, true, true));
  TEST_ASSERT_TRUE(plan.update(100 + kForcedMs - 1, true, true));
}

void test_requested_portal_closes_after_its_delay() {
  WifiPlan plan(kFailureMs, kForcedMs);
  plan.requestPortal(100);
  TEST_ASSERT_FALSE(plan.update(100 + kForcedMs, true, true));
}

// After saving, the portal stays up until the new Wi-Fi is joined, so that a
// wrong password can be corrected at once.
void test_portal_stays_open_after_saving_until_connected() {
  WifiPlan plan(kFailureMs, kForcedMs);
  TEST_ASSERT_TRUE(plan.update(0, false, false));
  plan.settingsSaved(100);
  TEST_ASSERT_TRUE(plan.update(200, true, false));
  TEST_ASSERT_TRUE(plan.update(200 + kFailureMs * 3, true, false));
  TEST_ASSERT_FALSE(plan.update(300 + kFailureMs * 3, true, true));
}

void test_requested_portal_closes_once_settings_are_saved_and_joined() {
  WifiPlan plan(kFailureMs, kForcedMs);
  plan.update(0, true, true);
  plan.requestPortal(100);
  plan.settingsSaved(200);
  TEST_ASSERT_TRUE(plan.update(300, true, false));
  TEST_ASSERT_FALSE(plan.update(400, true, true));
}

// Once joined, the new Wi-Fi gets the usual 5 minutes after a later loss.
void test_after_joining_the_failure_delay_applies_again() {
  WifiPlan plan(kFailureMs, kForcedMs);
  plan.settingsSaved(0);
  plan.update(100, true, true);
  TEST_ASSERT_FALSE(plan.update(200, true, false));
  TEST_ASSERT_TRUE(plan.update(100 + kFailureMs, true, false));
}

// millis() wraps after 49.7 days.
void test_survives_millis_wraparound() {
  WifiPlan plan(kFailureMs, kForcedMs);
  const uint32_t start = UINT32_MAX - 1000;
  plan.update(start, true, true);
  TEST_ASSERT_FALSE(plan.update(start + 2000, true, false));
  TEST_ASSERT_TRUE(plan.update(start + 2000 + kFailureMs, true, false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_portal_open_without_credentials);
  RUN_TEST(test_portal_closed_while_connected);
  RUN_TEST(test_portal_opens_after_five_minutes_without_connection);
  RUN_TEST(test_portal_closes_once_connected);
  RUN_TEST(test_failure_delay_restarts_after_each_connection);
  RUN_TEST(test_requested_portal_stays_open_while_connected);
  RUN_TEST(test_requested_portal_closes_after_its_delay);
  RUN_TEST(test_portal_stays_open_after_saving_until_connected);
  RUN_TEST(test_requested_portal_closes_once_settings_are_saved_and_joined);
  RUN_TEST(test_after_joining_the_failure_delay_applies_again);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}
