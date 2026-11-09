#include <unity.h>

#include <cmath>
#include <cstdint>

#include "sensor_watch.h"

using terrarium::control::AirRejection;
using terrarium::control::classifyAirReading;
using terrarium::control::isPlausibleAirReading;
using terrarium::control::rejectionName;
using terrarium::control::SensorHealth;
using terrarium::control::SensorWatch;

namespace {

// RG-capteur-en-defaut: silent for more than 2 minutes.
constexpr uint32_t kMaxSilentMs = 2UL * 60UL * 1000UL;

}  // namespace

void setUp() {}
void tearDown() {}

void test_valid_reading_is_ok() {
  SensorWatch watch(kMaxSilentMs);
  TEST_ASSERT_EQUAL(SensorHealth::kOk, watch.update(0, true));
}

void test_invalid_reading_holds_first() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, true);
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(30000, false));
}

void test_holds_up_to_the_limit_itself() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, false);
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(kMaxSilentMs, false));
}

void test_fault_once_silent_beyond_the_limit() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(1000, true);
  watch.update(2000, false);
  TEST_ASSERT_EQUAL(SensorHealth::kFault, watch.update(2000 + kMaxSilentMs + 1, false));
}

void test_never_valid_since_boot_ends_in_fault() {
  SensorWatch watch(kMaxSilentMs);
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(0, false));
  TEST_ASSERT_EQUAL(SensorHealth::kFault, watch.update(kMaxSilentMs + 1, false));
}

void test_recovers_on_first_valid_reading() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, false);
  watch.update(kMaxSilentMs + 1, false);
  TEST_ASSERT_EQUAL(SensorHealth::kOk, watch.update(kMaxSilentMs + 30000, true));
}

void test_a_valid_reading_restarts_the_silence_count() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, false);
  watch.update(kMaxSilentMs, true);
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(kMaxSilentMs + 30000, false));
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(2 * kMaxSilentMs + 30000, false));
}

// millis() wraps after about 49.7 days: elapsed time must survive it.
void test_survives_millis_wrap_around() {
  SensorWatch watch(kMaxSilentMs);
  const uint32_t beforeWrap = UINT32_MAX - 1000;
  watch.update(beforeWrap, false);
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(beforeWrap + 60000, false));
  TEST_ASSERT_EQUAL(SensorHealth::kFault, watch.update(beforeWrap + kMaxSilentMs + 1, false));
}

// RG-capteur-en-defaut: an aberrant reading counts as no reading.
void test_accepts_terrarium_conditions() {
  TEST_ASSERT_TRUE(isPlausibleAirReading(19.0f, 85.0f));
  TEST_ASSERT_TRUE(isPlausibleAirReading(4.0f, 100.0f));
  TEST_ASSERT_TRUE(isPlausibleAirReading(-20.0f, 0.0f));
  TEST_ASSERT_TRUE(isPlausibleAirReading(60.0f, 50.0f));
}

void test_rejects_aberrant_readings() {
  TEST_ASSERT_FALSE(isPlausibleAirReading(-20.1f, 80.0f));
  TEST_ASSERT_FALSE(isPlausibleAirReading(60.1f, 80.0f));
  TEST_ASSERT_FALSE(isPlausibleAirReading(19.0f, -0.1f));
  TEST_ASSERT_FALSE(isPlausibleAirReading(19.0f, 100.1f));
  TEST_ASSERT_FALSE(isPlausibleAirReading(NAN, 80.0f));
  TEST_ASSERT_FALSE(isPlausibleAirReading(19.0f, NAN));
}

// RG-capteur-en-defaut: the cause of a rejection, the first that applies.
void test_classifies_the_cause_of_a_rejection() {
  TEST_ASSERT_EQUAL(AirRejection::kNone, classifyAirReading(true, 19.0f, 85.0f));
  TEST_ASSERT_EQUAL(AirRejection::kSilent, classifyAirReading(false, 19.0f, 85.0f));
  TEST_ASSERT_EQUAL(AirRejection::kSilent, classifyAirReading(false, 99.0f, 150.0f));
  TEST_ASSERT_EQUAL(AirRejection::kTemperatureOutOfRange, classifyAirReading(true, 60.1f, 50.0f));
  TEST_ASSERT_EQUAL(AirRejection::kTemperatureOutOfRange, classifyAirReading(true, NAN, 102.0f));
  TEST_ASSERT_EQUAL(AirRejection::kHumidityOutOfRange, classifyAirReading(true, 15.2f, 102.0f));
  TEST_ASSERT_EQUAL(AirRejection::kHumidityOutOfRange, classifyAirReading(true, 15.2f, NAN));
}

void test_names_each_cause_as_published() {
  TEST_ASSERT_NULL(rejectionName(AirRejection::kNone));
  TEST_ASSERT_EQUAL_STRING("muet", rejectionName(AirRejection::kSilent));
  TEST_ASSERT_EQUAL_STRING("temperature-hors-plage",
                           rejectionName(AirRejection::kTemperatureOutOfRange));
  TEST_ASSERT_EQUAL_STRING("humidite-hors-plage", rejectionName(AirRejection::kHumidityOutOfRange));
}

void test_safety_enabled_ends_in_fault_whatever_the_cause() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, AirRejection::kHumidityOutOfRange, true);
  TEST_ASSERT_EQUAL(SensorHealth::kFault,
                    watch.update(kMaxSilentMs + 1, AirRejection::kHumidityOutOfRange, true));
}

// Safety disabled by the owner: regulate on the rejected value, never a fault.
void test_safety_disabled_regulates_on_an_out_of_range_reading() {
  SensorWatch watch(kMaxSilentMs);
  TEST_ASSERT_EQUAL(SensorHealth::kOk, watch.update(0, AirRejection::kHumidityOutOfRange, false));
  TEST_ASSERT_EQUAL(SensorHealth::kOk,
                    watch.update(10 * kMaxSilentMs, AirRejection::kTemperatureOutOfRange, false));
}

// Safety disabled and nothing to read: keep the last decision, without limit.
void test_safety_disabled_holds_a_silent_sensor_forever() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, AirRejection::kNone, false);
  TEST_ASSERT_EQUAL(SensorHealth::kHolding, watch.update(1000, AirRejection::kSilent, false));
  TEST_ASSERT_EQUAL(SensorHealth::kHolding,
                    watch.update(100 * kMaxSilentMs, AirRejection::kSilent, false));
}

void test_disabling_safety_lifts_a_fault() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, AirRejection::kHumidityOutOfRange, true);
  watch.update(kMaxSilentMs + 1, AirRejection::kHumidityOutOfRange, true);
  TEST_ASSERT_EQUAL(SensorHealth::kOk,
                    watch.update(kMaxSilentMs + 30000, AirRejection::kHumidityOutOfRange, false));
}

// Re-enabled after a long silence: the silence counts from its start.
void test_reenabling_safety_on_a_long_silence_is_a_fault() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, AirRejection::kSilent, false);
  TEST_ASSERT_EQUAL(SensorHealth::kFault,
                    watch.update(kMaxSilentMs + 1, AirRejection::kSilent, true));
}

void test_remembers_the_last_cause() {
  SensorWatch watch(kMaxSilentMs);
  watch.update(0, AirRejection::kHumidityOutOfRange, false);
  TEST_ASSERT_EQUAL(AirRejection::kHumidityOutOfRange, watch.rejection());
  watch.update(1000, AirRejection::kNone, false);
  TEST_ASSERT_EQUAL(AirRejection::kNone, watch.rejection());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_reading_is_ok);
  RUN_TEST(test_invalid_reading_holds_first);
  RUN_TEST(test_holds_up_to_the_limit_itself);
  RUN_TEST(test_fault_once_silent_beyond_the_limit);
  RUN_TEST(test_never_valid_since_boot_ends_in_fault);
  RUN_TEST(test_recovers_on_first_valid_reading);
  RUN_TEST(test_a_valid_reading_restarts_the_silence_count);
  RUN_TEST(test_survives_millis_wrap_around);
  RUN_TEST(test_accepts_terrarium_conditions);
  RUN_TEST(test_rejects_aberrant_readings);
  RUN_TEST(test_classifies_the_cause_of_a_rejection);
  RUN_TEST(test_names_each_cause_as_published);
  RUN_TEST(test_safety_enabled_ends_in_fault_whatever_the_cause);
  RUN_TEST(test_safety_disabled_regulates_on_an_out_of_range_reading);
  RUN_TEST(test_safety_disabled_holds_a_silent_sensor_forever);
  RUN_TEST(test_disabling_safety_lifts_a_fault);
  RUN_TEST(test_reenabling_safety_on_a_long_silence_is_a_fault);
  RUN_TEST(test_remembers_the_last_cause);
  return UNITY_END();
}
