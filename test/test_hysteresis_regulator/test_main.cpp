#include <unity.h>

#include <cmath>

#include "hysteresis_regulator.h"

using terrarium::control::HysteresisRegulator;

namespace {

// ACTIF defaults: 19 °C +/- 0.5 °C, i.e. on below 18.5, off at 19.5.
HysteresisRegulator makeActive() { return HysteresisRegulator::aroundSetpoint(19.0f, 0.5f); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_starts_off() { TEST_ASSERT_FALSE(makeActive().isOn()); }

void test_turns_on_below_band_when_cooling() {
  HysteresisRegulator regulator = makeActive();
  TEST_ASSERT_FALSE(regulator.update(19.0f, true));
  TEST_ASSERT_FALSE(regulator.update(18.5f, true));  // Lower edge itself: not below.
  TEST_ASSERT_TRUE(regulator.update(18.4f, true));
}

void test_turns_off_at_upper_edge_when_warming() {
  HysteresisRegulator regulator = makeActive();
  regulator.update(18.0f, true);
  TEST_ASSERT_TRUE(regulator.update(19.0f, true));
  TEST_ASSERT_TRUE(regulator.update(19.4f, true));
  TEST_ASSERT_FALSE(regulator.update(19.5f, true));
}

void test_dead_band_keeps_previous_state() {
  HysteresisRegulator regulator = makeActive();
  TEST_ASSERT_FALSE(regulator.update(19.0f, true));  // Off stays off inside the band.
  regulator.update(18.0f, true);
  TEST_ASSERT_TRUE(regulator.update(19.0f, true));  // On stays on inside the band.
  regulator.update(20.0f, true);
  TEST_ASSERT_FALSE(regulator.update(19.0f, true));  // Off again after crossing the top.
}

void test_invalid_flag_forces_off() {
  HysteresisRegulator regulator = makeActive();
  regulator.update(15.0f, true);
  TEST_ASSERT_FALSE(regulator.update(15.0f, false));
  TEST_ASSERT_FALSE(regulator.isOn());
}

void test_nan_forces_off_even_if_flagged_valid() {
  HysteresisRegulator regulator = makeActive();
  regulator.update(15.0f, true);
  TEST_ASSERT_FALSE(regulator.update(NAN, true));
}

void test_recovers_after_invalid_input() {
  HysteresisRegulator regulator = makeActive();
  regulator.update(15.0f, true);
  regulator.update(NAN, false);
  TEST_ASSERT_FALSE(regulator.update(19.0f, true));  // Inside the band: stays off.
  TEST_ASSERT_TRUE(regulator.update(15.0f, true));
}

void test_reset_forces_off() {
  HysteresisRegulator regulator = makeActive();
  regulator.update(15.0f, true);
  regulator.reset();
  TEST_ASSERT_FALSE(regulator.isOn());
}

void test_frost_band() {
  HysteresisRegulator regulator(4.0f, 6.0f);
  TEST_ASSERT_FALSE(regulator.update(5.0f, true));
  TEST_ASSERT_TRUE(regulator.update(3.9f, true));
  TEST_ASSERT_TRUE(regulator.update(5.9f, true));
  TEST_ASSERT_FALSE(regulator.update(6.0f, true));
}

// RG-reglages-bornes: a new setpoint takes effect at the next measurement,
// without dropping the heater mid-cycle.
void test_retune_keeps_the_state_and_moves_the_band() {
  HysteresisRegulator regulator = HysteresisRegulator::aroundSetpoint(19.0f, 0.5f);
  TEST_ASSERT_TRUE(regulator.update(18.0f, true));
  regulator.retune(20.0f, 0.5f);
  TEST_ASSERT_TRUE(regulator.isOn());
  TEST_ASSERT_TRUE(regulator.update(20.0f, true));
  TEST_ASSERT_FALSE(regulator.update(20.5f, true));
}

void test_retune_to_a_lower_band_stops_at_the_next_measurement() {
  HysteresisRegulator regulator = HysteresisRegulator::aroundSetpoint(21.0f, 0.5f);
  TEST_ASSERT_TRUE(regulator.update(20.0f, true));
  regulator.retune(16.0f, 0.5f);
  TEST_ASSERT_FALSE(regulator.update(20.0f, true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_starts_off);
  RUN_TEST(test_turns_on_below_band_when_cooling);
  RUN_TEST(test_turns_off_at_upper_edge_when_warming);
  RUN_TEST(test_dead_band_keeps_previous_state);
  RUN_TEST(test_invalid_flag_forces_off);
  RUN_TEST(test_nan_forces_off_even_if_flagged_valid);
  RUN_TEST(test_recovers_after_invalid_input);
  RUN_TEST(test_reset_forces_off);
  RUN_TEST(test_frost_band);
  RUN_TEST(test_retune_keeps_the_state_and_moves_the_band);
  RUN_TEST(test_retune_to_a_lower_band_stops_at_the_next_measurement);
  return UNITY_END();
}
