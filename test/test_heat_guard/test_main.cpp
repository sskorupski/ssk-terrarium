#include <unity.h>

#include <cstdint>

#include "heat_guard.h"

using terrarium::control::HeatGuard;

namespace {

// RG-chauffe-continue-bornee: 2 h on at most, then 15 min off.
constexpr uint32_t kMaxOnMs = 2UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kPauseMs = 15UL * 60UL * 1000UL;

HeatGuard makeGuard() { return HeatGuard(kMaxOnMs, kPauseMs); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_passes_a_request_through() {
  HeatGuard guard = makeGuard();
  TEST_ASSERT_TRUE(guard.filter(0, true));
  TEST_ASSERT_FALSE(guard.filter(1000, false));
  TEST_ASSERT_FALSE(guard.paused());
}

void test_allows_exactly_the_limit() {
  HeatGuard guard = makeGuard();
  guard.filter(0, true);
  TEST_ASSERT_TRUE(guard.filter(kMaxOnMs, true));
}

void test_cuts_beyond_the_limit_and_pauses() {
  HeatGuard guard = makeGuard();
  guard.filter(0, true);
  TEST_ASSERT_FALSE(guard.filter(kMaxOnMs + 1, true));
  TEST_ASSERT_TRUE(guard.paused());
}

void test_stays_off_during_the_pause() {
  HeatGuard guard = makeGuard();
  guard.filter(0, true);
  guard.filter(kMaxOnMs + 1, true);
  TEST_ASSERT_FALSE(guard.filter(kMaxOnMs + kPauseMs, true));
  TEST_ASSERT_TRUE(guard.paused());
}

void test_resumes_after_the_pause() {
  HeatGuard guard = makeGuard();
  guard.filter(0, true);
  const uint32_t cutAt = kMaxOnMs + 1;
  guard.filter(cutAt, true);
  TEST_ASSERT_TRUE(guard.filter(cutAt + kPauseMs, true));
  TEST_ASSERT_FALSE(guard.paused());
}

void test_a_new_heating_period_gets_the_full_limit_after_the_pause() {
  HeatGuard guard = makeGuard();
  guard.filter(0, true);
  const uint32_t resumeAt = kMaxOnMs + 1 + kPauseMs;
  guard.filter(kMaxOnMs + 1, true);
  guard.filter(resumeAt, true);
  TEST_ASSERT_TRUE(guard.filter(resumeAt + kMaxOnMs, true));
  TEST_ASSERT_FALSE(guard.filter(resumeAt + kMaxOnMs + 1, true));
}

void test_switching_off_restarts_the_count() {
  HeatGuard guard = makeGuard();
  guard.filter(0, true);
  guard.filter(kMaxOnMs - 1000, false);
  TEST_ASSERT_TRUE(guard.filter(kMaxOnMs, true));
  TEST_ASSERT_TRUE(guard.filter(2 * kMaxOnMs, true));
}

void test_survives_millis_wrap_around() {
  HeatGuard guard = makeGuard();
  const uint32_t start = UINT32_MAX - 1000;
  guard.filter(start, true);
  TEST_ASSERT_TRUE(guard.filter(start + kMaxOnMs, true));
  TEST_ASSERT_FALSE(guard.filter(start + kMaxOnMs + 1, true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_passes_a_request_through);
  RUN_TEST(test_allows_exactly_the_limit);
  RUN_TEST(test_cuts_beyond_the_limit_and_pauses);
  RUN_TEST(test_stays_off_during_the_pause);
  RUN_TEST(test_resumes_after_the_pause);
  RUN_TEST(test_a_new_heating_period_gets_the_full_limit_after_the_pause);
  RUN_TEST(test_switching_off_restarts_the_count);
  RUN_TEST(test_survives_millis_wrap_around);
  return UNITY_END();
}
