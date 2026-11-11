#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "config.h"
#include "ventilation.h"

using terrarium::control::FanMode;
using terrarium::control::FanPolicy;
using terrarium::control::FanRhythm;
using terrarium::control::FanScheduler;
using terrarium::control::FanSettings;
using terrarium::control::FanSettingsPatch;
using terrarium::control::Moment;
using terrarium::control::SaturationBehaviour;
using terrarium::control::SaturationOption;
using terrarium::control::SaturationVent;

namespace cfg = terrarium::config;

namespace {

constexpr uint32_t kMinuteMs = 60UL * 1000UL;
constexpr uint32_t kHourMs = 60UL * kMinuteMs;

constexpr terrarium::control::FanSettingsBounds kBounds{cfg::kFanDurationMinMin,
                                                        cfg::kFanDurationMaxMin,
                                                        cfg::kFanPeriodMinMin,
                                                        cfg::kFanPeriodMaxMin,
                                                        cfg::kFanSpreadMinPercent,
                                                        cfg::kFanSpreadMaxPercent,
                                                        cfg::kSaturationHumidityMinPercent,
                                                        cfg::kSaturationHumidityMaxPercent,
                                                        cfg::kSaturationWaitMinMin,
                                                        cfg::kSaturationWaitMaxMin,
                                                        cfg::kSaturationPauseMinMin,
                                                        cfg::kSaturationPauseMaxMin};

constexpr FanRhythm kMetronome{FanMode::kMetronome, 5, 60, 30};

FanSettings defaults() {
  FanSettings s{};
  s.day = kMetronome;
  s.night = kMetronome;
  s.hibernation = {13 * 60, 5};
  s.saturation = {false, SaturationBehaviour::kAiring, 95, 10, 5, 60};
  return s;
}

bool accepted(const FanSettings& settings) {
  return terrarium::control::isAcceptedFanSettings(settings, kBounds);
}

// A scripted random source: each call returns the next value.
uint32_t script[16];
uint8_t scriptLength = 0;
uint8_t scriptNext = 0;
uint32_t scripted() {
  const uint32_t value = scriptLength == 0 ? 1000 : script[scriptNext % scriptLength];
  ++scriptNext;
  return value;
}
void setScript(std::initializer_list<uint32_t> values) {
  scriptLength = 0;
  scriptNext = 0;
  for (uint32_t v : values) script[scriptLength++] = v;
}

FanScheduler scheduler() {
  return FanScheduler(cfg::kFanDailyMinGapMs, cfg::kFanDailyFallbackMs, kBounds, scripted);
}

bool periodic(FanScheduler& fan, uint32_t nowMs, const FanSettings& settings = defaults(),
              Moment moment = Moment::kDay) {
  return fan.update(nowMs, FanPolicy::kPeriodic, settings, moment, false, 0);
}

bool dailyKnown(FanScheduler& fan, uint32_t nowMs, uint16_t minuteOfDay,
                const FanSettings& settings = defaults()) {
  return fan.update(nowMs, FanPolicy::kDaily, settings, Moment::kDay, true, minuteOfDay);
}

bool dailyUnknown(FanScheduler& fan, uint32_t nowMs, const FanSettings& settings = defaults()) {
  return fan.update(nowMs, FanPolicy::kDaily, settings, Moment::kDay, false, 0);
}

FanSettings withHibernation(uint16_t minute, uint16_t duration) {
  FanSettings s = defaults();
  s.hibernation = {minute, duration};
  return s;
}

constexpr SaturationOption kAiring{true, SaturationBehaviour::kAiring, 95, 10, 5, 60};
constexpr SaturationOption kContinuous{true, SaturationBehaviour::kContinuous, 95, 10, 5, 60};

// One measure every 30 s, as the regulation loop does. Returns the last answer.
bool feed(SaturationVent& vent, const SaturationOption& option, uint32_t fromMs, uint32_t toMs,
          float humidity, bool allowed = true, bool valid = true) {
  bool on = false;
  for (uint32_t t = fromMs; t <= toMs; t += 30UL * 1000UL) {
    on = vent.update(t, option, allowed, valid, humidity);
  }
  return on;
}

}  // namespace

void setUp() { setScript({}); }
void tearDown() {}

// RG-reglages-bornes.
void test_accepts_the_defaults() { TEST_ASSERT_TRUE(accepted(defaults())); }

void test_firmware_defaults_are_within_bounds() {
  FanSettings s{};
  s.day = {FanMode::kMetronome, cfg::kFanDurationMin, cfg::kFanPeriodMin, cfg::kFanSpreadPercent};
  s.night = s.day;
  s.hibernation = {cfg::kFanDailyMinute, cfg::kFanDurationMin};
  s.saturation = {false,
                  SaturationBehaviour::kAiring,
                  cfg::kSaturationHumidityPercent,
                  cfg::kSaturationWaitMin,
                  cfg::kSaturationDurationMin,
                  cfg::kSaturationPauseMin};
  TEST_ASSERT_TRUE(accepted(s));
}

void test_refuses_a_rhythm_out_of_bounds() {
  for (FanRhythm bad :
       {FanRhythm{FanMode::kMetronome, 0, 60, 30}, FanRhythm{FanMode::kMetronome, 31, 60, 30},
        FanRhythm{FanMode::kMetronome, 5, 14, 30}, FanRhythm{FanMode::kMetronome, 5, 241, 30},
        FanRhythm{FanMode::kMetronome, 15, 15, 30}, FanRhythm{FanMode::kRubato, 5, 60, 9},
        FanRhythm{FanMode::kRubato, 5, 60, 51}}) {
    FanSettings day = defaults();
    day.day = bad;
    TEST_ASSERT_FALSE(accepted(day));
    FanSettings night = defaults();
    night.night = bad;
    TEST_ASSERT_FALSE(accepted(night));
  }
}

void test_refuses_hibernation_and_saturation_out_of_bounds() {
  TEST_ASSERT_FALSE(accepted(withHibernation(24 * 60, 5)));
  TEST_ASSERT_FALSE(accepted(withHibernation(13 * 60, 0)));
  TEST_ASSERT_FALSE(accepted(withHibernation(13 * 60, 31)));
  const SaturationOption bad[] = {{true, SaturationBehaviour::kAiring, 79, 10, 5, 60},
                                  {true, SaturationBehaviour::kAiring, 101, 10, 5, 60},
                                  {true, SaturationBehaviour::kAiring, 95, 0, 5, 60},
                                  {true, SaturationBehaviour::kAiring, 95, 61, 5, 60},
                                  {true, SaturationBehaviour::kAiring, 95, 10, 0, 60},
                                  {true, SaturationBehaviour::kAiring, 95, 10, 31, 60},
                                  {true, SaturationBehaviour::kAiring, 95, 10, 5, 14},
                                  {true, SaturationBehaviour::kAiring, 95, 10, 5, 241}};
  for (const SaturationOption& option : bad) {
    FanSettings s = defaults();
    s.saturation = option;
    TEST_ASSERT_FALSE(accepted(s));
  }
}

// cmd/config "ventilation" is partial at both levels.
void test_patch_keeps_missing_fields() {
  FanSettingsPatch patch{};
  patch.night.hasMode = true;
  patch.night.mode = FanMode::kRubato;
  patch.night.hasPeriod = true;
  patch.night.periodMin = 120;
  patch.saturation.hasActive = true;
  patch.saturation.active = true;
  const FanSettings next = terrarium::control::merged(defaults(), patch);
  TEST_ASSERT_EQUAL(FanMode::kMetronome, next.day.mode);
  TEST_ASSERT_EQUAL(FanMode::kRubato, next.night.mode);
  TEST_ASSERT_EQUAL_UINT16(5, next.night.durationMin);
  TEST_ASSERT_EQUAL_UINT16(120, next.night.periodMin);
  TEST_ASSERT_TRUE(next.saturation.active);
  TEST_ASSERT_EQUAL_UINT16(95, next.saturation.humidityPercent);
  TEST_ASSERT_EQUAL_UINT16(13 * 60, next.hibernation.dailyMinute);
}

// RG-ventilation-periodique, metronome: each airing lasts `duree`, the next
// one starts `periode` after the start of the previous one.
void test_metronome_airs_at_each_period() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, 0));
  TEST_ASSERT_TRUE(periodic(fan, 5 * kMinuteMs - 1));
  TEST_ASSERT_FALSE(periodic(fan, 5 * kMinuteMs));
  TEST_ASSERT_FALSE(periodic(fan, kHourMs - 1));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs));
  TEST_ASSERT_FALSE(periodic(fan, kHourMs + 5 * kMinuteMs));
}

// Sampled every 30 s for 40 days, the schedule does not drift.
void test_metronome_stays_aligned_over_many_days() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  for (uint32_t step = 0; step < 40UL * 24UL * 120UL; ++step) {
    const uint32_t now = step * 30UL * 1000UL;
    TEST_ASSERT_EQUAL((now % kHourMs) < 5 * kMinuteMs, periodic(fan, now));
  }
}

void test_restart_begins_with_an_airing() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_FALSE(periodic(fan, 30 * kMinuteMs));
  fan.restart(30 * kMinuteMs);
  TEST_ASSERT_TRUE(periodic(fan, 30 * kMinuteMs));
  TEST_ASSERT_FALSE(periodic(fan, 35 * kMinuteMs));
}

// Duration and interval are fixed at the start of each airing: a new setting
// waits for the next one (decision 03 of cadrage 2026-015).
void test_new_setting_applies_at_the_next_airing() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, 0));
  FanSettings longer = defaults();
  longer.day = {FanMode::kMetronome, 10, 30, 30};
  TEST_ASSERT_FALSE(periodic(fan, 6 * kMinuteMs, longer));
  TEST_ASSERT_FALSE(periodic(fan, 31 * kMinuteMs, longer));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs, longer));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs + 9 * kMinuteMs, longer));
  TEST_ASSERT_FALSE(periodic(fan, kHourMs + 10 * kMinuteMs, longer));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs + 30 * kMinuteMs, longer));
}

// Day and night each have their rhythm, taken at the start of an airing.
void test_night_rhythm_applies_from_the_next_airing() {
  FanScheduler fan = scheduler();
  FanSettings s = defaults();
  s.night = {FanMode::kMetronome, 10, 120, 30};
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, 0, s, Moment::kDay));
  TEST_ASSERT_FALSE(periodic(fan, 30 * kMinuteMs, s, Moment::kNight));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs, s, Moment::kNight));
  TEST_ASSERT_TRUE(periodic(fan, kHourMs + 9 * kMinuteMs, s, Moment::kNight));
  TEST_ASSERT_FALSE(periodic(fan, kHourMs + 10 * kMinuteMs, s, Moment::kNight));
  TEST_ASSERT_FALSE(periodic(fan, 2 * kHourMs, s, Moment::kNight));
  TEST_ASSERT_TRUE(periodic(fan, 3 * kHourMs, s, Moment::kNight));
}

// Rubato: duration then interval drawn within +/- spread, rounded to the
// minute. 2000 draws the top, 0 the bottom, 1000 the middle.
void test_rubato_draws_around_the_settings() {
  FanSettings s = defaults();
  s.day = {FanMode::kRubato, 5, 60, 30};
  setScript({2000, 2000, 0, 0});
  FanScheduler fan = scheduler();
  fan.restart(0);
  // First airing: 6.5 -> 7 minutes, then 78 minutes to the next.
  TEST_ASSERT_TRUE(periodic(fan, 0, s));
  TEST_ASSERT_TRUE(periodic(fan, 7 * kMinuteMs - 1, s));
  TEST_ASSERT_FALSE(periodic(fan, 7 * kMinuteMs, s));
  TEST_ASSERT_FALSE(periodic(fan, 78 * kMinuteMs - 1, s));
  TEST_ASSERT_TRUE(periodic(fan, 78 * kMinuteMs, s));
  // Second: 3.5 -> 4 minutes, then 42 minutes.
  TEST_ASSERT_TRUE(periodic(fan, 82 * kMinuteMs - 1, s));
  TEST_ASSERT_FALSE(periodic(fan, 82 * kMinuteMs, s));
  TEST_ASSERT_TRUE(periodic(fan, 120 * kMinuteMs, s));
}

// The draw never leaves the bounds, and the duration stays below the interval.
void test_rubato_stays_within_bounds() {
  FanSettings s = defaults();
  s.day = {FanMode::kRubato, 29, 30, 50};
  // Top for the duration (43.5 -> 30), bottom for the interval (15): 14.
  setScript({2000, 0});
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, 14 * kMinuteMs - 1, s));
  TEST_ASSERT_FALSE(periodic(fan, 14 * kMinuteMs, s));
  TEST_ASSERT_TRUE(periodic(fan, 15 * kMinuteMs, s));
}

void test_metronome_draws_nothing() {
  setScript({0});
  FanScheduler fan = scheduler();
  fan.restart(0);
  periodic(fan, 0);
  periodic(fan, kHourMs);
  TEST_ASSERT_EQUAL_UINT8(0, scriptNext);
}

void test_periodic_survives_the_millis_wrap() {
  FanScheduler fan = scheduler();
  const uint32_t start = 0xFFFFFFFFUL - 2 * kMinuteMs;
  fan.restart(start);
  TEST_ASSERT_TRUE(periodic(fan, start));
  TEST_ASSERT_TRUE(periodic(fan, start + 4 * kMinuteMs));
  TEST_ASSERT_FALSE(periodic(fan, start + 5 * kMinuteMs));
  TEST_ASSERT_TRUE(periodic(fan, start + kHourMs));
}

// MAINTENANCE: off, even in the middle of an airing.
void test_off_policy_stops_the_fan() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(periodic(fan, kMinuteMs));
  TEST_ASSERT_FALSE(fan.update(2 * kMinuteMs, FanPolicy::kOff, defaults(), Moment::kDay, false, 0));
}

// HIBERNATION, as in 2026-013 but with its own duration.
void test_daily_airs_at_the_set_time_for_its_duration() {
  FanScheduler fan = scheduler();
  const FanSettings s = withHibernation(13 * 60, 8);
  fan.restart(0);
  TEST_ASSERT_FALSE(dailyKnown(fan, 0, 12 * 60 + 59, s));
  TEST_ASSERT_TRUE(dailyKnown(fan, kMinuteMs, 13 * 60, s));
  TEST_ASSERT_TRUE(dailyKnown(fan, 8 * kMinuteMs, 13 * 60 + 7, s));
  TEST_ASSERT_FALSE(dailyKnown(fan, 9 * kMinuteMs, 13 * 60 + 8, s));
  TEST_ASSERT_TRUE(dailyKnown(fan, 24 * kHourMs + kMinuteMs, 13 * 60, s));
}

void test_daily_span_runs_past_midnight() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyKnown(fan, kMinuteMs, 1, withHibernation(23 * 60 + 58, 5)));
}

void test_daily_keeps_twelve_hours_between_airings() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyKnown(fan, 0, 13 * 60));
  const FanSettings evening = withHibernation(20 * 60, 5);
  TEST_ASSERT_FALSE(dailyKnown(fan, 7 * kHourMs, 20 * 60, evening));
  TEST_ASSERT_TRUE(dailyKnown(fan, 31 * kHourMs, 20 * 60, evening));
}

void test_daily_without_time_airs_every_twenty_four_hours() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_FALSE(dailyUnknown(fan, 24 * kHourMs - 1));
  TEST_ASSERT_TRUE(dailyUnknown(fan, 24 * kHourMs));
  TEST_ASSERT_FALSE(dailyUnknown(fan, 24 * kHourMs + 5 * kMinuteMs));
  TEST_ASSERT_TRUE(dailyUnknown(fan, 48 * kHourMs));
}

void test_restart_forgets_the_previous_daily_airing() {
  FanScheduler fan = scheduler();
  fan.restart(0);
  TEST_ASSERT_TRUE(dailyKnown(fan, 0, 13 * 60));
  fan.restart(kHourMs);
  TEST_ASSERT_FALSE(dailyUnknown(fan, 2 * kHourMs));
  TEST_ASSERT_TRUE(dailyUnknown(fan, 25 * kHourMs));
}

// Option saturation (decision 04 of cadrage 2026-015).
void test_saturation_waits_then_airs_then_pauses() {
  SaturationVent vent;
  TEST_ASSERT_FALSE(feed(vent, kAiring, 0, 10 * kMinuteMs - 30000, 96.0f));
  TEST_ASSERT_TRUE(vent.update(10 * kMinuteMs, kAiring, true, true, 96.0f));
  TEST_ASSERT_TRUE(vent.running());
  // Airing: 5 minutes, even if the air dries at once.
  TEST_ASSERT_TRUE(feed(vent, kAiring, 10 * kMinuteMs + 30000, 15 * kMinuteMs - 30000, 80.0f));
  TEST_ASSERT_FALSE(vent.update(15 * kMinuteMs, kAiring, true, true, 96.0f));
  // Pause: 60 minutes without a new trigger, however saturated.
  TEST_ASSERT_FALSE(feed(vent, kAiring, 15 * kMinuteMs, 84 * kMinuteMs, 99.0f));
  TEST_ASSERT_TRUE(feed(vent, kAiring, 84 * kMinuteMs, 86 * kMinuteMs, 99.0f));
}

void test_saturation_wait_restarts_below_the_threshold() {
  SaturationVent vent;
  feed(vent, kAiring, 0, 9 * kMinuteMs, 96.0f);
  vent.update(9 * kMinuteMs + 30000, kAiring, true, true, 94.9f);
  TEST_ASSERT_FALSE(feed(vent, kAiring, 10 * kMinuteMs, 19 * kMinuteMs, 96.0f));
  TEST_ASSERT_TRUE(feed(vent, kAiring, 19 * kMinuteMs + 30000, 20 * kMinuteMs, 96.0f));
}

void test_saturation_is_silent_without_an_air_measure() {
  SaturationVent silent;
  TEST_ASSERT_FALSE(feed(silent, kAiring, 0, 20 * kMinuteMs, 99.0f, true, false));
  // Continuous stops on a missing measure.
  SaturationVent continuous;
  feed(continuous, kContinuous, 0, 10 * kMinuteMs, 99.0f);
  TEST_ASSERT_TRUE(continuous.running());
  TEST_ASSERT_FALSE(continuous.update(11 * kMinuteMs, kContinuous, true, false, 0.0f));
}

void test_continuous_runs_while_saturated_and_at_most_its_duration() {
  SaturationVent vent;
  feed(vent, kContinuous, 0, 10 * kMinuteMs, 96.0f);
  TEST_ASSERT_TRUE(vent.running());
  TEST_ASSERT_FALSE(vent.update(12 * kMinuteMs, kContinuous, true, true, 94.0f));
  SaturationVent capped;
  TEST_ASSERT_TRUE(feed(capped, kContinuous, 0, 15 * kMinuteMs - 30000, 99.0f));
  TEST_ASSERT_FALSE(capped.update(15 * kMinuteMs, kContinuous, true, true, 99.0f));
}

void test_saturation_inactive_or_not_allowed_never_runs() {
  SaturationVent vent;
  SaturationOption off = kAiring;
  off.active = false;
  TEST_ASSERT_FALSE(feed(vent, off, 0, 30 * kMinuteMs, 99.0f));
  SaturationVent maintenance;
  TEST_ASSERT_FALSE(feed(maintenance, kAiring, 0, 30 * kMinuteMs, 99.0f, false));
  // Stopped mid-airing by MAINTENANCE.
  SaturationVent stopped;
  feed(stopped, kAiring, 0, 10 * kMinuteMs, 99.0f);
  TEST_ASSERT_FALSE(stopped.update(11 * kMinuteMs, kAiring, false, true, 99.0f));
  TEST_ASSERT_FALSE(stopped.running());
}

void test_names() {
  TEST_ASSERT_EQUAL_STRING("rubato", terrarium::control::fanModeName(FanMode::kRubato));
  TEST_ASSERT_EQUAL_STRING("continu",
                           terrarium::control::behaviourName(SaturationBehaviour::kContinuous));
  FanMode mode = FanMode::kMetronome;
  TEST_ASSERT_TRUE(terrarium::control::parseFanMode("rubato", mode));
  TEST_ASSERT_EQUAL(FanMode::kRubato, mode);
  TEST_ASSERT_FALSE(terrarium::control::parseFanMode("RUBATO", mode));
  SaturationBehaviour behaviour = SaturationBehaviour::kAiring;
  TEST_ASSERT_TRUE(terrarium::control::parseBehaviour("continu", behaviour));
  TEST_ASSERT_EQUAL(SaturationBehaviour::kContinuous, behaviour);
}

void test_parses_and_formats_a_clock_time() {
  uint16_t minute = 42;
  TEST_ASSERT_TRUE(terrarium::control::parseClockTime("13:00", 5, minute));
  TEST_ASSERT_EQUAL_UINT16(780, minute);
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime("24:00", 5, minute));
  TEST_ASSERT_FALSE(terrarium::control::parseClockTime(nullptr, 5, minute));
  char text[terrarium::control::kClockTimeTextLength + 1];
  TEST_ASSERT_TRUE(terrarium::control::formatClockTime(14 * 60 + 5, text));
  TEST_ASSERT_EQUAL_STRING("14:05", text);
}

// RG-mode-au-demarrage: the settings go to NVS as bytes, and come back.
void test_stored_settings_round_trip() {
  FanSettings s = defaults();
  s.night = {FanMode::kRubato, 7, 120, 45};
  s.hibernation = {14 * 60 + 30, 8};
  s.saturation = {true, SaturationBehaviour::kContinuous, 97, 5, 3, 30};
  uint8_t bytes[terrarium::control::kFanStoredSize];
  terrarium::control::encodeFanSettings(s, bytes);
  FanSettings back{};
  TEST_ASSERT_TRUE(terrarium::control::decodeFanSettings(bytes, back));
  TEST_ASSERT_EQUAL_MEMORY(&s.night, &back.night, sizeof(s.night));
  TEST_ASSERT_EQUAL_UINT16(14 * 60 + 30, back.hibernation.dailyMinute);
  TEST_ASSERT_EQUAL_UINT16(8, back.hibernation.durationMin);
  TEST_ASSERT_TRUE(back.saturation.active);
  TEST_ASSERT_EQUAL(SaturationBehaviour::kContinuous, back.saturation.behaviour);
  TEST_ASSERT_EQUAL_UINT16(30, back.saturation.pauseMin);
}

void test_stored_settings_refuse_unknown_bytes() {
  uint8_t bytes[terrarium::control::kFanStoredSize];
  terrarium::control::encodeFanSettings(defaults(), bytes);
  FanSettings back = defaults();
  bytes[0] = 99;  // Unknown layout version.
  TEST_ASSERT_FALSE(terrarium::control::decodeFanSettings(bytes, back));
  terrarium::control::encodeFanSettings(defaults(), bytes);
  bytes[1] = 7;  // Unknown day mode.
  TEST_ASSERT_FALSE(terrarium::control::decodeFanSettings(bytes, back));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_the_defaults);
  RUN_TEST(test_firmware_defaults_are_within_bounds);
  RUN_TEST(test_refuses_a_rhythm_out_of_bounds);
  RUN_TEST(test_refuses_hibernation_and_saturation_out_of_bounds);
  RUN_TEST(test_patch_keeps_missing_fields);
  RUN_TEST(test_metronome_airs_at_each_period);
  RUN_TEST(test_metronome_stays_aligned_over_many_days);
  RUN_TEST(test_restart_begins_with_an_airing);
  RUN_TEST(test_new_setting_applies_at_the_next_airing);
  RUN_TEST(test_night_rhythm_applies_from_the_next_airing);
  RUN_TEST(test_rubato_draws_around_the_settings);
  RUN_TEST(test_rubato_stays_within_bounds);
  RUN_TEST(test_metronome_draws_nothing);
  RUN_TEST(test_periodic_survives_the_millis_wrap);
  RUN_TEST(test_off_policy_stops_the_fan);
  RUN_TEST(test_daily_airs_at_the_set_time_for_its_duration);
  RUN_TEST(test_daily_span_runs_past_midnight);
  RUN_TEST(test_daily_keeps_twelve_hours_between_airings);
  RUN_TEST(test_daily_without_time_airs_every_twenty_four_hours);
  RUN_TEST(test_restart_forgets_the_previous_daily_airing);
  RUN_TEST(test_saturation_waits_then_airs_then_pauses);
  RUN_TEST(test_saturation_wait_restarts_below_the_threshold);
  RUN_TEST(test_saturation_is_silent_without_an_air_measure);
  RUN_TEST(test_continuous_runs_while_saturated_and_at_most_its_duration);
  RUN_TEST(test_saturation_inactive_or_not_allowed_never_runs);
  RUN_TEST(test_names);
  RUN_TEST(test_parses_and_formats_a_clock_time);
  RUN_TEST(test_stored_settings_round_trip);
  RUN_TEST(test_stored_settings_refuse_unknown_bytes);
  return UNITY_END();
}
