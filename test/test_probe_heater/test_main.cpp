#include <unity.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "probe_heater.h"
#include "sensor_watch.h"

using terrarium::control::AirRejection;
using terrarium::control::HeatReason;
using terrarium::control::heatReasonName;
using terrarium::control::ProbeHeater;

namespace {

// RG-chauffe-de-la-sht45: from 95 %RH, at most once every 5 minutes.
constexpr float kMinRhPercent = 95.0f;
constexpr uint32_t kMinIntervalMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kPeriodMs = 30UL * 1000UL;

ProbeHeater heater() { return ProbeHeater(kMinRhPercent, kMinIntervalMs); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_accepted_reading_below_threshold_does_not_heat() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kNone, h.due(0, AirRejection::kNone, 94.9f));
}

void test_accepted_reading_at_threshold_heats() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kHighHumidity, h.due(0, AirRejection::kNone, 95.0f));
}

void test_accepted_reading_above_threshold_heats() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kHighHumidity, h.due(0, AirRejection::kNone, 99.8f));
}

void test_humidity_out_of_range_heats() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kHumidityOutOfRange,
                    h.due(0, AirRejection::kHumidityOutOfRange, 102.3f));
}

void test_humidity_out_of_range_heats_whatever_the_value() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kHumidityOutOfRange,
                    h.due(0, AirRejection::kHumidityOutOfRange, std::nanf("")));
}

void test_silent_probe_does_not_heat() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kNone, h.due(0, AirRejection::kSilent, 0.0f));
}

void test_temperature_out_of_range_does_not_heat() {
  ProbeHeater h = heater();
  TEST_ASSERT_EQUAL(HeatReason::kNone, h.due(0, AirRejection::kTemperatureOutOfRange, 102.0f));
}

void test_no_second_heating_within_the_interval() {
  ProbeHeater h = heater();
  h.due(0, AirRejection::kNone, 96.0f);
  for (uint32_t now = kPeriodMs; now < kMinIntervalMs; now += kPeriodMs) {
    TEST_ASSERT_EQUAL(HeatReason::kNone, h.due(now, AirRejection::kHumidityOutOfRange, 102.0f));
  }
}

void test_heats_again_once_the_interval_has_elapsed() {
  ProbeHeater h = heater();
  h.due(0, AirRejection::kNone, 96.0f);
  TEST_ASSERT_EQUAL(HeatReason::kHighHumidity, h.due(kMinIntervalMs, AirRejection::kNone, 96.0f));
}

void test_a_dry_reading_does_not_start_the_interval() {
  ProbeHeater h = heater();
  h.due(0, AirRejection::kNone, 80.0f);
  TEST_ASSERT_EQUAL(HeatReason::kHighHumidity, h.due(kPeriodMs, AirRejection::kNone, 96.0f));
}

void test_interval_counts_from_the_last_heating() {
  ProbeHeater h = heater();
  h.due(0, AirRejection::kNone, 96.0f);
  h.due(kMinIntervalMs, AirRejection::kNone, 96.0f);
  TEST_ASSERT_EQUAL(HeatReason::kNone,
                    h.due(kMinIntervalMs + kPeriodMs, AirRejection::kNone, 96.0f));
}

void test_interval_survives_millis_wrap() {
  ProbeHeater h = heater();
  const uint32_t start = UINT32_MAX - kPeriodMs;
  h.due(start, AirRejection::kNone, 96.0f);
  TEST_ASSERT_EQUAL(HeatReason::kNone, h.due(start + 2 * kPeriodMs, AirRejection::kNone, 96.0f));
  TEST_ASSERT_EQUAL(HeatReason::kHighHumidity,
                    h.due(start + kMinIntervalMs, AirRejection::kNone, 96.0f));
}

void test_reason_names() {
  TEST_ASSERT_NULL(heatReasonName(HeatReason::kNone));
  TEST_ASSERT_EQUAL_STRING("humidite-haute", heatReasonName(HeatReason::kHighHumidity));
  TEST_ASSERT_EQUAL_STRING("humidite-hors-plage", heatReasonName(HeatReason::kHumidityOutOfRange));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepted_reading_below_threshold_does_not_heat);
  RUN_TEST(test_accepted_reading_at_threshold_heats);
  RUN_TEST(test_accepted_reading_above_threshold_heats);
  RUN_TEST(test_humidity_out_of_range_heats);
  RUN_TEST(test_humidity_out_of_range_heats_whatever_the_value);
  RUN_TEST(test_silent_probe_does_not_heat);
  RUN_TEST(test_temperature_out_of_range_does_not_heat);
  RUN_TEST(test_no_second_heating_within_the_interval);
  RUN_TEST(test_heats_again_once_the_interval_has_elapsed);
  RUN_TEST(test_a_dry_reading_does_not_start_the_interval);
  RUN_TEST(test_interval_counts_from_the_last_heating);
  RUN_TEST(test_interval_survives_millis_wrap);
  RUN_TEST(test_reason_names);
  return UNITY_END();
}
