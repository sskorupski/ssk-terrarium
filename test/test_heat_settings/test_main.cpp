#include <unity.h>

#include "config.h"
#include "heat_settings.h"

using terrarium::control::HeatSettings;

namespace cfg = terrarium::config;

namespace {

// The bounds the firmware uses, from config.h.
constexpr terrarium::control::HeatSettingsBounds kBounds{
    cfg::kSetpointMinC, cfg::kSetpointMaxC, cfg::kHeatHysteresisMinC, cfg::kHeatHysteresisMaxC,
    cfg::kAlertTempHighC};

bool isAcceptedHeatSettings(const HeatSettings& settings) {
  return terrarium::control::isAcceptedHeatSettings(settings, kBounds);
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-reglages-bornes: setpoint 15 to 22 C, hysteresis 0.2 to 2 C.
void test_accepts_settings_within_bounds() {
  TEST_ASSERT_TRUE(isAcceptedHeatSettings(HeatSettings{19.0f, 0.5f}));
  TEST_ASSERT_TRUE(isAcceptedHeatSettings(HeatSettings{15.0f, 0.2f}));
  TEST_ASSERT_TRUE(isAcceptedHeatSettings(HeatSettings{22.0f, 1.9f}));
}

void test_refuses_a_setpoint_out_of_bounds() {
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{14.9f, 0.5f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{22.1f, 0.5f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{35.0f, 0.5f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{-5.0f, 0.5f}));
}

void test_refuses_a_hysteresis_out_of_bounds() {
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{19.0f, 0.1f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{19.0f, 2.1f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{19.0f, 0.0f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{19.0f, -1.0f}));
}

// Heating stops at setpoint + hysteresis: that must stay under the 24 C alert.
void test_refuses_a_band_reaching_the_high_alert() {
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{22.0f, 2.0f}));
  TEST_ASSERT_TRUE(isAcceptedHeatSettings(HeatSettings{21.0f, 2.0f}));
}

void test_refuses_nan_and_infinity() {
  const float nan = __builtin_nanf("");
  const float inf = __builtin_inff();
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{nan, 0.5f}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{19.0f, nan}));
  TEST_ASSERT_FALSE(isAcceptedHeatSettings(HeatSettings{inf, 0.5f}));
}

// The firmware defaults must themselves be accepted.
void test_defaults_are_accepted() {
  TEST_ASSERT_TRUE(isAcceptedHeatSettings(
      HeatSettings{terrarium::config::kSetpointC, terrarium::config::kHeatHysteresisC}));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_settings_within_bounds);
  RUN_TEST(test_refuses_a_setpoint_out_of_bounds);
  RUN_TEST(test_refuses_a_hysteresis_out_of_bounds);
  RUN_TEST(test_refuses_a_band_reaching_the_high_alert);
  RUN_TEST(test_refuses_nan_and_infinity);
  RUN_TEST(test_defaults_are_accepted);
  return UNITY_END();
}
