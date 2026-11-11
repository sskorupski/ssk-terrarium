#include <unity.h>

#include <cstring>

#include "config.h"
#include "ventilation.h"

using terrarium::control::FanPolicy;
using terrarium::control::FanScheduler;
using terrarium::control::FanSettings;
using terrarium::control::FanSettingsPatch;

namespace cfg = terrarium::config;

namespace {

constexpr uint32_t kMinuteMs = 60UL * 1000UL;
constexpr uint32_t kHourMs = 60UL * kMinuteMs;

constexpr terrarium::control::FanSettingsBounds kBounds{
    cfg::kFanDurationMinMin, cfg::kFanDurationMaxMin, cfg::kFanPeriodMinMin, cfg::kFanPeriodMaxMin};

// F10: 5 minutes every 60, and the hibernation airing at 13:00.
constexpr FanSettings kDefaults{5, 60, 13 * 60};

bool accepted(const FanSettings& settings) {
  return terrarium::control::isAcceptedFanSettings(settings, kBounds);
}

FanScheduler scheduler() { return FanScheduler(cfg::kFanDailyMinGapMs, cfg::kFanDailyFallbackMs); }

// Periodic policy, time not used.
bool periodic(FanScheduler& fan, uint32_t nowMs, const FanSettings& settings = kDefaults) {
  return fan.update(nowMs, FanPolicy::kPeriodic, settings, false, 0);
}

bool dailyKnown(FanScheduler& fan, uint32_t nowMs, uint16_t minuteOfDay,
                const FanSettings& settings = kDefaults) {
  return fan.update(nowMs, FanPolicy::kDaily, settings, true, minuteOfDay);
}

bool dailyUnknown(FanScheduler& fan, uint32_t nowMs, const FanSettings& settings = kDefaults) {
  return fan.update(nowMs, FanPolicy::kDaily, settings, false, 0);
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-reglages-bornes: duration 1 to 30 min, period 15 to 240 min, duration
// strictly below the period, and a valid time of day.
void test_accepts_settings_within_bounds() {
  TEST_ASSERT_TRUE(accepted(kDefaults));
  TEST_ASSERT_TRUE(accepted(FanSettings{1, 15, 0}));
  TEST_ASSERT_TRUE(accepted(FanSettings{14, 15, 23 * 60 + 59}));
  TEST_ASSERT_TRUE(accepted(FanSettings{30, 240, 0}));
}

void test_refuses_settings_out_of_bounds() {
  TEST_ASSERT_FALSE(accepted(FanSettings{0, 60, 780}));
  TEST_ASSERT_FALSE(accepted(FanSettings{31, 60, 780}));
  TEST_ASSERT_FALSE(accepted(FanSettings{5, 14, 780}));
  TEST_ASSERT_FALSE(accepted(FanSettings{5, 241, 780}));
  TEST_ASSERT_FALSE(accepted(FanSettings{5, 60, 24 * 60}));
}

// The fan must stop within each period (RG-reglages-bornes).
void test_refuses_a_duration_not_below_the_period() {
  TEST_ASSERT_FALSE(accepted(FanSettings{15, 15, 780}));
  TEST_ASSERT_FALSE(accepted(FanSettings{20, 15, 780}));
}

void test_the_defaults_are_within_bounds() {
  TEST_ASSERT_TRUE(
      accepted(FanSettings{cfg::kFanDurationMin, cfg::kFanPeriodMin, cfg::kFanDailyMinute}));
}

// cmd/config "ventilation" is partial: missing fields keep their value.
void test_patch_keeps_missing_fields() {
  FanSettingsPatch patch{};
  patch.hasDuration = true;
  patch.durationMin = 10;
  const FanSettings next = terrarium::control::merged(kDefaults, patch);
  TEST_ASSERT_EQUAL_UINT16(10, next.durationMin);
  TEST_ASSERT_EQUAL_UINT16(60, next.periodMin);
  TEST_ASSERT_EQUAL_UINT16(780, next.dailyMinute);
}

void test_patch_replaces_every_field_given() {
  FanSettingsPatch patch{};
  patch.hasDuration = true;
  patch.durationMin = 3;
  patch.hasPeriod = true;
  patch.periodMin = 30;
  patch.hasDailyMinute = true;
  patch.dailyMinute = 14 * 60 + 30;
  const FanSettings next = terrarium::control::merged(kDefaults, patch);
  TEST_ASSERT_EQUAL_UINT16(3, next.durationMin);
  TEST_ASSERT_EQUAL_UINT16(30, next.periodMin);
  TEST_ASSERT_EQUAL_UINT16(870, next.dailyMinute);
}

// RG-ventilation-periodique, ACTIF and PLAGES: the fan runs the first
// `duree` minutes of each period, counted from the restart.
void test_periodic_runs_at_the_start_of_each_period() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, 0));
  TEST_ASSERT_TRUE(periodic(fan, 5 * kMinuteMs - 1));
  TEST_ASSERT_FALSE(periodic(fan, 5 * kMinuteMs));
  TEST_ASSERT_FALSE(periodic(fan, kHourMs - 1));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs));
  TEST_ASSERT_FALSE(periodic(fan, kHourMs + 5 * kMinuteMs));
  TEST_ASSERT_TRUE(periodic(fan, 10 * kHourMs + kMinuteMs));
}

// A mode change restarts the count: the first period begins with an airing.
void test_restart_begins_with_an_airing() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_FALSE(periodic(fan, 30 * kMinuteMs));
  fan.restart(30 * kMinuteMs);
  TEST_ASSERT_TRUE(periodic(fan, 30 * kMinuteMs));
  TEST_ASSERT_FALSE(periodic(fan, 35 * kMinuteMs));
}

// A setting accepted applies at once, without restarting the count.
void test_periodic_follows_a_new_duration_without_restarting() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_FALSE(periodic(fan, 6 * kMinuteMs));
  TEST_ASSERT_TRUE(periodic(fan, 6 * kMinuteMs, FanSettings{10, 60, 780}));
  TEST_ASSERT_FALSE(periodic(fan, 10 * kMinuteMs, FanSettings{10, 60, 780}));
}

// millis() wraps after 49.7 days: durations are computed by subtraction.
void test_periodic_survives_the_millis_wrap() {
  FanScheduler fan = scheduler();
  const uint32_t start = 0xFFFFFFFFUL - 2 * kMinuteMs;
  fan.restart(start);
  TEST_ASSERT_TRUE(periodic(fan, start));
  TEST_ASSERT_TRUE(periodic(fan, start + 4 * kMinuteMs));
  TEST_ASSERT_FALSE(periodic(fan, start + 5 * kMinuteMs));
  TEST_ASSERT_TRUE(periodic(fan, start + kHourMs));
}

// A long run without a mode change keeps the period aligned.
void test_periodic_stays_aligned_over_many_days() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  uint32_t now = 0;
  for (int hour = 0; hour < 24 * 60; ++hour) {
    now = static_cast<uint32_t>(hour) * kHourMs;
    TEST_ASSERT_TRUE(periodic(fan, now));
    TEST_ASSERT_FALSE(periodic(fan, now + 30 * kMinuteMs));
  }
}

// MAINTENANCE: off, even in the middle of an airing.
void test_off_policy_stops_the_fan() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, kMinuteMs));
  TEST_ASSERT_FALSE(fan.update(2 * kMinuteMs, FanPolicy::kOff, kDefaults, false, 0));
}

// HIBERNATION, time known: one airing a day, starting at `heure`.
void test_daily_airs_at_the_set_time() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_FALSE(dailyKnown(fan, 0, 12 * 60 + 59));
  TEST_ASSERT_TRUE(dailyKnown(fan, kMinuteMs, 13 * 60));
  TEST_ASSERT_TRUE(dailyKnown(fan, 5 * kMinuteMs, 13 * 60 + 4));
  TEST_ASSERT_FALSE(dailyKnown(fan, 6 * kMinuteMs, 13 * 60 + 5));
  TEST_ASSERT_FALSE(dailyKnown(fan, 12 * kHourMs, 1 * 60));
  TEST_ASSERT_TRUE(dailyKnown(fan, 24 * kHourMs + kMinuteMs, 13 * 60));
}

// Sampled every 30 s, the start minute may be missed by a few seconds: any
// sample within the airing's span starts it.
void test_daily_starts_within_the_span_of_the_set_time() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyKnown(fan, kMinuteMs, 13 * 60 + 2));
  TEST_ASSERT_TRUE(dailyKnown(fan, 5 * kMinuteMs, 13 * 60 + 6));
  TEST_ASSERT_FALSE(dailyKnown(fan, 6 * kMinuteMs, 13 * 60 + 7));
}

void test_daily_span_runs_past_midnight() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  const FanSettings lateEvening{5, 60, 23 * 60 + 58};
  TEST_ASSERT_TRUE(dailyKnown(fan, kMinuteMs, 1, lateEvening));
}

// Never two airings less than 12 hours apart.
void test_daily_keeps_twelve_hours_between_airings() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyKnown(fan, 0, 13 * 60));
  const FanSettings evening{5, 60, 20 * 60};
  TEST_ASSERT_FALSE(dailyKnown(fan, 7 * kHourMs, 20 * 60, evening));
  TEST_ASSERT_TRUE(dailyKnown(fan, 31 * kHourMs, 20 * 60, evening));
}

// HIBERNATION, time unknown: 24 hours after the restart, then after the
// previous airing.
void test_daily_without_time_airs_every_twenty_four_hours() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_FALSE(dailyUnknown(fan, 0));
  TEST_ASSERT_FALSE(dailyUnknown(fan, 24 * kHourMs - 1));
  TEST_ASSERT_TRUE(dailyUnknown(fan, 24 * kHourMs));
  TEST_ASSERT_FALSE(dailyUnknown(fan, 24 * kHourMs + 5 * kMinuteMs));
  TEST_ASSERT_FALSE(dailyUnknown(fan, 48 * kHourMs - kMinuteMs));
  TEST_ASSERT_TRUE(dailyUnknown(fan, 48 * kHourMs));
}

// The time coming back does not give a second airing within 12 hours.
void test_daily_time_coming_back_respects_the_gap() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyUnknown(fan, 24 * kHourMs));
  TEST_ASSERT_FALSE(dailyKnown(fan, 29 * kHourMs, 13 * 60));
  TEST_ASSERT_TRUE(dailyKnown(fan, 53 * kHourMs, 13 * 60));
}

// A shorter duration ends the airing in progress once reached.
void test_daily_airing_stops_at_a_shortened_duration() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  const FanSettings longer{10, 60, 13 * 60};
  TEST_ASSERT_TRUE(dailyKnown(fan, 0, 13 * 60, longer));
  TEST_ASSERT_TRUE(dailyKnown(fan, 6 * kMinuteMs, 13 * 60 + 6, longer));
  TEST_ASSERT_FALSE(dailyKnown(fan, 6 * kMinuteMs, 13 * 60 + 6));
}

// Leaving HIBERNATION and coming back forgets the previous airing.
void test_restart_forgets_the_previous_daily_airing() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyKnown(fan, 0, 13 * 60));
  fan.restart(kHourMs);
  TEST_ASSERT_FALSE(dailyUnknown(fan, 2 * kHourMs));
  TEST_ASSERT_TRUE(dailyUnknown(fan, 25 * kHourMs));
}

// "HH:MM" of the hibernation airing.
void test_parses_a_clock_time() {
  uint16_t minute = 0;
  TEST_ASSERT_TRUE(terrarium::control::parseClockTime("13:00", 5, minute));
  TEST_ASSERT_EQUAL_UINT16(780, minute);
  TEST_ASSERT_TRUE(terrarium::control::parseClockTime("00:00", 5, minute));
  TEST_ASSERT_EQUAL_UINT16(0, minute);
  TEST_ASSERT_TRUE(terrarium::control::parseClockTime("23:59", 5, minute));
  TEST_ASSERT_EQUAL_UINT16(1439, minute);
}

void test_refuses_a_malformed_clock_time() {
  uint16_t minute = 42;
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime("24:00", 5, minute));
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime("13:60", 5, minute));
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime("1:00", 4, minute));
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime("13:000", 6, minute));
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime(nullptr, 5, minute));
  TEST_ASSERT_EQUAL_UINT16(42, minute);
}

void test_formats_a_clock_time() {
  char text[terrarium::control::kClockTimeTextLength + 1];
  TEST_ASSERT_TRUE(terrarium::control::formatClockTime(14 * 60 + 5, text));
  TEST_ASSERT_EQUAL_STRING("14:05", text);
  TEST_ASSERT_FALSE(terrarium::control::formatClockTime(24 * 60, text));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_settings_within_bounds);
  RUN_TEST(test_refuses_settings_out_of_bounds);
  RUN_TEST(test_refuses_a_duration_not_below_the_period);
  RUN_TEST(test_the_defaults_are_within_bounds);
  RUN_TEST(test_patch_keeps_missing_fields);
  RUN_TEST(test_patch_replaces_every_field_given);
  RUN_TEST(test_periodic_runs_at_the_start_of_each_period);
  RUN_TEST(test_restart_begins_with_an_airing);
  RUN_TEST(test_periodic_follows_a_new_duration_without_restarting);
  RUN_TEST(test_periodic_survives_the_millis_wrap);
  RUN_TEST(test_periodic_stays_aligned_over_many_days);
  RUN_TEST(test_off_policy_stops_the_fan);
  RUN_TEST(test_daily_airs_at_the_set_time);
  RUN_TEST(test_daily_starts_within_the_span_of_the_set_time);
  RUN_TEST(test_daily_span_runs_past_midnight);
  RUN_TEST(test_daily_keeps_twelve_hours_between_airings);
  RUN_TEST(test_daily_without_time_airs_every_twenty_four_hours);
  RUN_TEST(test_daily_time_coming_back_respects_the_gap);
  RUN_TEST(test_daily_airing_stops_at_a_shortened_duration);
  RUN_TEST(test_restart_forgets_the_previous_daily_airing);
  RUN_TEST(test_parses_a_clock_time);
  RUN_TEST(test_refuses_a_malformed_clock_time);
  RUN_TEST(test_formats_a_clock_time);
  return UNITY_END();
}
