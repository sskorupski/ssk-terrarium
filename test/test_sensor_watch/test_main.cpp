#include <unity.h>

#include <cstdint>

#include "sensor_watch.h"

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
  return UNITY_END();
}
