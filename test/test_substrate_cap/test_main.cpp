#include <unity.h>

#include <cmath>
#include <cstdint>

#include "substrate_cap.h"

using terrarium::control::decodeSubstrateSettings;
using terrarium::control::encodeSubstrateSettings;
using terrarium::control::isAcceptedSubstrateSettings;
using terrarium::control::kNoSubstratePart;
using terrarium::control::kSubstrateStoredSize;
using terrarium::control::merged;
using terrarium::control::Moment;
using terrarium::control::parseSilentProbeBehaviour;
using terrarium::control::SilentProbeBehaviour;
using terrarium::control::silentProbeBehaviourName;
using terrarium::control::SubstrateCap;
using terrarium::control::SubstrateCapChange;
using terrarium::control::SubstrateSettings;
using terrarium::control::SubstrateSettingsBounds;
using terrarium::control::SubstrateSettingsPatch;
using terrarium::control::tenthsFromCelsius;
using terrarium::control::thresholdsFor;

namespace {

// RG-coupure-par-le-substrat, before any setting: active, cut at 24 °C and
// resume strictly below 18.5 °C day and night, no approach, a silent probe
// lifts the cut after 2 min.
constexpr SubstrateSettings kDefaults{
    true, 0, {240, 185}, {240, 185}, SilentProbeBehaviour::kResumeOnAir, 2};
// RG-reglages-bornes: cut 20..28 °C, resume from 15 °C and at least 2 °C below
// the cut, delay 1..30 min, approach 0..4 °C.
constexpr SubstrateSettingsBounds kBounds{200, 280, 150, 20, 0, 40, 1, 30};
constexpr uint32_t kMaxSilentMs = 2UL * 60UL * 1000UL;
constexpr float kHoldAirFloorC = 4.0f;
constexpr uint32_t kSliceMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kMinuteMs = 60UL * 1000UL;

SubstrateCap makeCap() { return SubstrateCap(kHoldAirFloorC, kSliceMs); }

// One reading with a valid air at 19 °C, in the day.
SubstrateCapChange feed(SubstrateCap& cap, uint32_t nowMs, bool valid, float temperatureC,
                        const SubstrateSettings& settings = kDefaults,
                        Moment moment = Moment::kDay) {
  return cap.update(nowMs, settings, moment, valid, temperatureC, true, 19.0f);
}

SubstrateSettings withApproach(int16_t tenths) {
  SubstrateSettings s = kDefaults;
  s.approachTenths = tenths;
  return s;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- The cut, unchanged without settings (objective S3) ---------------------

void test_does_not_cut_below_the_threshold() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 0, true, 23.9f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_cuts_when_the_substrate_reaches_the_threshold() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, feed(cap, 0, true, 24.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_stays_cut_down_to_the_resume_threshold() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000, true, 20.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 60000, true, 18.5f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_resumes_strictly_below_the_resume_threshold() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedCold, feed(cap, 30000, true, 18.4f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_does_not_cut_again_between_the_thresholds() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  feed(cap, 30000, true, 18.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 60000, true, 23.9f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_never_cuts_without_a_probe() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 0, false, 99.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, kMaxSilentMs * 10, false, 99.0f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_ignores_implausible_readings() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 0, true, 60.1f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000, true, NAN));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_accepts_the_edges_of_the_plausible_range() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, feed(cap, 0, true, 60.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedCold, feed(cap, 30000, true, -20.0f));
}

void test_an_implausible_reading_does_not_resume() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000, true, -20.1f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_holds_the_cut_while_the_probe_is_briefly_silent() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000, false, 0.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000 + kMaxSilentMs, false, 0.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_lifts_the_cut_once_the_probe_is_silent_too_long() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  feed(cap, 30000, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedSilent,
                    feed(cap, 30000 + kMaxSilentMs + 1, false, 0.0f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_a_valid_reading_restarts_the_silence() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  feed(cap, 30000, false, 0.0f);
  feed(cap, 90000, true, 22.0f);
  feed(cap, 120000, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 120000 + kMaxSilentMs, false, 0.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_silence_survives_the_millis_wrap() {
  SubstrateCap cap = makeCap();
  const uint32_t nearWrap = UINT32_MAX - 1000;
  feed(cap, nearWrap - 30000, true, 24.0f);
  feed(cap, nearWrap, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, nearWrap + 60000, false, 0.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedSilent,
                    feed(cap, nearWrap + kMaxSilentMs + 1, false, 0.0f));
}

void test_cuts_again_after_a_silent_resume() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  feed(cap, 30000, false, 0.0f);
  feed(cap, 30000 + kMaxSilentMs + 1, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, feed(cap, 300000, true, 24.5f));
}

// --- Thresholds of the moment (RG-jour-et-nuit, decision 04) ----------------

SubstrateSettings dayAndNight() {
  SubstrateSettings s = kDefaults;
  s.night = {220, 180};  // 22 °C and 18 °C, the owner's night (user journey, step 2).
  return s;
}

void test_the_night_thresholds_cut_at_night() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone,
                    feed(cap, 0, true, 22.1f, dayAndNight(), Moment::kDay));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut,
                    feed(cap, 30000, true, 22.1f, dayAndNight(), Moment::kNight));
}

void test_a_cut_holds_at_daybreak_above_the_day_resume() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 22.1f, dayAndNight(), Moment::kNight);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone,
                    feed(cap, 30000, true, 20.0f, dayAndNight(), Moment::kDay));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_a_cut_lifts_at_nightfall_below_the_night_resume() {
  SubstrateSettings s = kDefaults;
  s.night = {240, 200};
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, s, Moment::kDay);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000, true, 19.5f, s, Moment::kDay));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedCold,
                    feed(cap, 60000, true, 19.5f, s, Moment::kNight));
}

void test_a_reading_at_the_new_cut_cuts_at_nightfall() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 23.0f, dayAndNight(), Moment::kDay);
  TEST_ASSERT_FALSE(cap.cut());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut,
                    feed(cap, 30000, true, 23.0f, dayAndNight(), Moment::kNight));
}

void test_thresholds_for_the_moment() {
  const SubstrateSettings s = dayAndNight();
  TEST_ASSERT_EQUAL_INT16(240, thresholdsFor(s, Moment::kDay).cutTenths);
  TEST_ASSERT_EQUAL_INT16(180, thresholdsFor(s, Moment::kNight).resumeTenths);
}

// --- Silent probe (decision 03) ---------------------------------------------

SubstrateSettings holding() {
  SubstrateSettings s = kDefaults;
  s.silentBehaviour = SilentProbeBehaviour::kHold;
  return s;
}

void test_the_set_delay_lifts_a_silent_cut() {
  SubstrateSettings s = kDefaults;
  s.silentDelayMin = 10;
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, s);
  feed(cap, 30000, false, 0.0f, s);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 30000 + 10 * kMinuteMs, false, 0.0f, s));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedSilent,
                    feed(cap, 30000 + 10 * kMinuteMs + 1, false, 0.0f, s));
}

void test_hold_keeps_a_silent_cut() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, holding());
  feed(cap, 30000, false, 0.0f, holding());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone,
                    feed(cap, 30000 + 60 * kMinuteMs, false, 0.0f, holding()));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_hold_lifts_the_cut_once_the_air_falls_below_4_c() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, holding());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone,
                    cap.update(30000, holding(), Moment::kDay, false, 0.0f, true, 4.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedFrost,
                    cap.update(60000, holding(), Moment::kDay, false, 0.0f, true, 3.9f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_hold_ignores_the_floor_without_an_air_measure() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, holding());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone,
                    cap.update(30000, holding(), Moment::kDay, false, 0.0f, false, 0.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_hold_ignores_the_floor_with_a_valid_substrate() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, holding());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone,
                    cap.update(30000, holding(), Moment::kDay, true, 20.0f, true, 3.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_hold_resumes_on_a_valid_reading_below_the_resume() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f, holding());
  feed(cap, 30000, false, 0.0f, holding());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedCold, feed(cap, 60000, true, 18.0f, holding()));
}

// --- Disabled (decision 01) -------------------------------------------------

SubstrateSettings disabled() {
  SubstrateSettings s = kDefaults;
  s.active = false;
  return s;
}

void test_disabled_never_cuts() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, feed(cap, 0, true, 40.0f, disabled()));
  TEST_ASSERT_FALSE(cap.cut());
  TEST_ASSERT_TRUE(cap.filter(0, true));
}

void test_disabling_lifts_a_cut() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedDisabled,
                    feed(cap, 30000, true, 25.0f, disabled()));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_reenabling_cuts_on_the_next_hot_reading() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 25.0f, disabled());
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, feed(cap, 30000, true, 25.0f));
}

// --- Part of heating and the approach zone (decision 06) --------------------

void test_a_cut_switches_heating_off() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 24.0f);
  TEST_ASSERT_FALSE(cap.filter(0, true));
  TEST_ASSERT_EQUAL_INT8(0, cap.partTenths());
}

void test_the_part_is_whole_outside_the_zone() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 23.9f);
  TEST_ASSERT_TRUE(cap.filter(0, true));
  TEST_ASSERT_EQUAL_INT8(10, cap.partTenths());
}

void test_the_part_is_unknown_when_disabled_or_without_a_reading() {
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 20.0f, disabled());
  cap.filter(0, true);
  TEST_ASSERT_EQUAL_INT8(kNoSubstratePart, cap.partTenths());
  feed(cap, 30000, false, 0.0f);
  TEST_ASSERT_TRUE(cap.filter(30000, true));
  TEST_ASSERT_EQUAL_INT8(kNoSubstratePart, cap.partTenths());
}

void test_heating_slows_halfway_in_the_zone() {
  // Approach 2 °C under a 24 °C cut: at 23 °C, 5 minutes of each 10.
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(0, true));
  TEST_ASSERT_EQUAL_INT8(5, cap.partTenths());
  feed(cap, 5 * kMinuteMs - 30000, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(5 * kMinuteMs - 30000, true));
  feed(cap, 5 * kMinuteMs, true, 23.0f, s);
  TEST_ASSERT_FALSE(cap.filter(5 * kMinuteMs, true));
  feed(cap, kSliceMs - 30000, true, 23.0f, s);
  TEST_ASSERT_FALSE(cap.filter(kSliceMs - 30000, true));
  // The next slice starts on time, heating first.
  feed(cap, kSliceMs, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(kSliceMs, true));
}

void test_the_part_is_whole_at_the_entry_of_the_zone() {
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 22.0f, s);
  cap.filter(0, true);
  TEST_ASSERT_EQUAL_INT8(10, cap.partTenths());
  feed(cap, kSliceMs - 30000, true, 22.0f, s);
  TEST_ASSERT_TRUE(cap.filter(kSliceMs - 30000, true));
}

void test_the_part_rounds_to_the_minute() {
  // (24 - 23.65) / 2 = 0.175 of 10 minutes: 1.75, rounded to 2 minutes.
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 23.65f, s);
  cap.filter(0, true);
  TEST_ASSERT_EQUAL_INT8(2, cap.partTenths());
  // (24 - 23.85) / 2 = 0.075: 0.75 minute, rounded to 1.
  SubstrateCap other = makeCap();
  feed(other, 0, true, 23.85f, s);
  other.filter(0, true);
  TEST_ASSERT_EQUAL_INT8(1, other.partTenths());
}

void test_the_part_is_fixed_at_the_start_of_the_slice() {
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 23.0f, s);
  cap.filter(0, true);
  // The substrate cools within the slice: the part stays 5 minutes.
  feed(cap, 5 * kMinuteMs, true, 22.2f, s);
  TEST_ASSERT_FALSE(cap.filter(5 * kMinuteMs, true));
  TEST_ASSERT_EQUAL_INT8(5, cap.partTenths());
  // The next slice takes the last reading: 9 minutes.
  feed(cap, kSliceMs, true, 22.2f, s);
  cap.filter(kSliceMs, true);
  TEST_ASSERT_EQUAL_INT8(9, cap.partTenths());
}

void test_leaving_the_zone_ends_the_slice() {
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 23.5f, s);
  cap.filter(0, true);
  feed(cap, 5 * kMinuteMs, true, 21.9f, s);
  TEST_ASSERT_TRUE(cap.filter(5 * kMinuteMs, true));
  TEST_ASSERT_EQUAL_INT8(10, cap.partTenths());
}

void test_without_demand_no_slice_starts() {
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 23.0f, s);
  TEST_ASSERT_FALSE(cap.filter(0, false));
  TEST_ASSERT_EQUAL_INT8(5, cap.partTenths());
  // The slice starts when the mode asks for heat.
  feed(cap, 7 * kMinuteMs, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(7 * kMinuteMs, true));
  feed(cap, 12 * kMinuteMs - 30000, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(12 * kMinuteMs - 30000, true));
  feed(cap, 12 * kMinuteMs, true, 23.0f, s);
  TEST_ASSERT_FALSE(cap.filter(12 * kMinuteMs, true));
}

void test_no_approach_without_a_valid_reading() {
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  feed(cap, 0, true, 23.0f, s);
  cap.filter(0, true);
  feed(cap, 6 * kMinuteMs, false, 0.0f, s);
  TEST_ASSERT_TRUE(cap.filter(6 * kMinuteMs, true));
}

void test_the_zone_follows_the_moment() {
  SubstrateSettings s = withApproach(20);
  s.night = {220, 180};
  SubstrateCap cap = makeCap();
  feed(cap, 0, true, 21.0f, s, Moment::kNight);
  cap.filter(0, true);
  TEST_ASSERT_EQUAL_INT8(5, cap.partTenths());
  feed(cap, 30000, true, 21.0f, s, Moment::kDay);
  TEST_ASSERT_TRUE(cap.filter(30000, true));
  TEST_ASSERT_EQUAL_INT8(10, cap.partTenths());
}

void test_slices_survive_the_millis_wrap() {
  SubstrateCap cap = makeCap();
  const SubstrateSettings s = withApproach(20);
  const uint32_t start = UINT32_MAX - 2 * kMinuteMs;
  feed(cap, start, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(start, true));
  feed(cap, start + 5 * kMinuteMs, true, 23.0f, s);
  TEST_ASSERT_FALSE(cap.filter(start + 5 * kMinuteMs, true));
  feed(cap, start + kSliceMs, true, 23.0f, s);
  TEST_ASSERT_TRUE(cap.filter(start + kSliceMs, true));
}

// --- Settings and their bounds (RG-reglages-bornes) -------------------------

void test_the_defaults_are_accepted() {
  TEST_ASSERT_TRUE(isAcceptedSubstrateSettings(kDefaults, kBounds));
}

void test_the_cut_stays_within_20_and_28_c() {
  SubstrateSettings s = kDefaults;
  s.day = {280, 260};
  TEST_ASSERT_TRUE(isAcceptedSubstrateSettings(s, kBounds));
  s.day = {281, 260};
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
  s.night = {199, 170};
  s.day = kDefaults.day;
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
}

void test_the_resume_stays_2_c_below_the_cut_and_above_15_c() {
  SubstrateSettings s = kDefaults;
  s.day = {240, 220};
  TEST_ASSERT_TRUE(isAcceptedSubstrateSettings(s, kBounds));
  s.day = {240, 230};  // User journey, step 4.
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
  s.day = {200, 150};
  TEST_ASSERT_TRUE(isAcceptedSubstrateSettings(s, kBounds));
  s.day = {200, 149};
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
}

void test_the_delay_and_approach_stay_within_bounds() {
  SubstrateSettings s = kDefaults;
  s.silentDelayMin = 0;
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
  s.silentDelayMin = 31;
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
  s.silentDelayMin = 30;
  s.approachTenths = 40;
  TEST_ASSERT_TRUE(isAcceptedSubstrateSettings(s, kBounds));
  s.approachTenths = 41;
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
  s.approachTenths = -1;
  TEST_ASSERT_FALSE(isAcceptedSubstrateSettings(s, kBounds));
}

void test_a_patch_changes_only_what_it_names() {
  SubstrateSettingsPatch patch{};
  patch.night.hasCut = true;
  patch.night.cutTenths = 220;
  patch.hasActive = true;
  patch.active = false;
  const SubstrateSettings next = merged(kDefaults, patch);
  TEST_ASSERT_FALSE(next.active);
  TEST_ASSERT_EQUAL_INT16(220, next.night.cutTenths);
  TEST_ASSERT_EQUAL_INT16(185, next.night.resumeTenths);
  TEST_ASSERT_EQUAL_INT16(240, next.day.cutTenths);
  TEST_ASSERT_EQUAL(SilentProbeBehaviour::kResumeOnAir, next.silentBehaviour);
}

void test_tenths_of_a_degree() {
  int16_t tenths = 0;
  TEST_ASSERT_TRUE(tenthsFromCelsius(22.1f, tenths));
  TEST_ASSERT_EQUAL_INT16(221, tenths);
  TEST_ASSERT_TRUE(tenthsFromCelsius(24.0f, tenths));
  TEST_ASSERT_EQUAL_INT16(240, tenths);
  TEST_ASSERT_TRUE(tenthsFromCelsius(-0.5f, tenths));
  TEST_ASSERT_EQUAL_INT16(-5, tenths);
  TEST_ASSERT_FALSE(tenthsFromCelsius(22.05f, tenths));
  TEST_ASSERT_FALSE(tenthsFromCelsius(NAN, tenths));
  TEST_ASSERT_FALSE(tenthsFromCelsius(1e9f, tenths));
}

void test_behaviour_names() {
  SilentProbeBehaviour b = SilentProbeBehaviour::kResumeOnAir;
  TEST_ASSERT_TRUE(parseSilentProbeBehaviour("maintien", b));
  TEST_ASSERT_EQUAL(SilentProbeBehaviour::kHold, b);
  TEST_ASSERT_TRUE(parseSilentProbeBehaviour("reprise-air", b));
  TEST_ASSERT_EQUAL(SilentProbeBehaviour::kResumeOnAir, b);
  TEST_ASSERT_FALSE(parseSilentProbeBehaviour("Maintien", b));
  TEST_ASSERT_EQUAL_STRING("maintien", silentProbeBehaviourName(SilentProbeBehaviour::kHold));
}

// --- NVS form (RG-mode-au-demarrage) ----------------------------------------

void test_settings_round_trip_through_nvs() {
  SubstrateSettings s = dayAndNight();
  s.active = false;
  s.approachTenths = 20;
  s.silentBehaviour = SilentProbeBehaviour::kHold;
  s.silentDelayMin = 12;
  uint8_t stored[kSubstrateStoredSize];
  encodeSubstrateSettings(s, stored);
  SubstrateSettings back = kDefaults;
  TEST_ASSERT_TRUE(decodeSubstrateSettings(stored, back));
  TEST_ASSERT_FALSE(back.active);
  TEST_ASSERT_EQUAL_INT16(20, back.approachTenths);
  TEST_ASSERT_EQUAL_INT16(220, back.night.cutTenths);
  TEST_ASSERT_EQUAL_INT16(180, back.night.resumeTenths);
  TEST_ASSERT_EQUAL_INT16(240, back.day.cutTenths);
  TEST_ASSERT_EQUAL(SilentProbeBehaviour::kHold, back.silentBehaviour);
  TEST_ASSERT_EQUAL_UINT16(12, back.silentDelayMin);
}

void test_an_unreadable_record_is_refused() {
  uint8_t stored[kSubstrateStoredSize];
  encodeSubstrateSettings(kDefaults, stored);
  SubstrateSettings back = kDefaults;
  stored[0] ^= 0xFF;  // Another layout.
  TEST_ASSERT_FALSE(decodeSubstrateSettings(stored, back));
  encodeSubstrateSettings(kDefaults, stored);
  stored[1] = 2;  // "active" neither 0 nor 1.
  TEST_ASSERT_FALSE(decodeSubstrateSettings(stored, back));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_does_not_cut_below_the_threshold);
  RUN_TEST(test_cuts_when_the_substrate_reaches_the_threshold);
  RUN_TEST(test_stays_cut_down_to_the_resume_threshold);
  RUN_TEST(test_resumes_strictly_below_the_resume_threshold);
  RUN_TEST(test_does_not_cut_again_between_the_thresholds);
  RUN_TEST(test_never_cuts_without_a_probe);
  RUN_TEST(test_ignores_implausible_readings);
  RUN_TEST(test_accepts_the_edges_of_the_plausible_range);
  RUN_TEST(test_an_implausible_reading_does_not_resume);
  RUN_TEST(test_holds_the_cut_while_the_probe_is_briefly_silent);
  RUN_TEST(test_lifts_the_cut_once_the_probe_is_silent_too_long);
  RUN_TEST(test_a_valid_reading_restarts_the_silence);
  RUN_TEST(test_silence_survives_the_millis_wrap);
  RUN_TEST(test_cuts_again_after_a_silent_resume);
  RUN_TEST(test_the_night_thresholds_cut_at_night);
  RUN_TEST(test_a_cut_holds_at_daybreak_above_the_day_resume);
  RUN_TEST(test_a_cut_lifts_at_nightfall_below_the_night_resume);
  RUN_TEST(test_a_reading_at_the_new_cut_cuts_at_nightfall);
  RUN_TEST(test_thresholds_for_the_moment);
  RUN_TEST(test_the_set_delay_lifts_a_silent_cut);
  RUN_TEST(test_hold_keeps_a_silent_cut);
  RUN_TEST(test_hold_lifts_the_cut_once_the_air_falls_below_4_c);
  RUN_TEST(test_hold_ignores_the_floor_without_an_air_measure);
  RUN_TEST(test_hold_ignores_the_floor_with_a_valid_substrate);
  RUN_TEST(test_hold_resumes_on_a_valid_reading_below_the_resume);
  RUN_TEST(test_disabled_never_cuts);
  RUN_TEST(test_disabling_lifts_a_cut);
  RUN_TEST(test_reenabling_cuts_on_the_next_hot_reading);
  RUN_TEST(test_a_cut_switches_heating_off);
  RUN_TEST(test_the_part_is_whole_outside_the_zone);
  RUN_TEST(test_the_part_is_unknown_when_disabled_or_without_a_reading);
  RUN_TEST(test_heating_slows_halfway_in_the_zone);
  RUN_TEST(test_the_part_is_whole_at_the_entry_of_the_zone);
  RUN_TEST(test_the_part_rounds_to_the_minute);
  RUN_TEST(test_the_part_is_fixed_at_the_start_of_the_slice);
  RUN_TEST(test_leaving_the_zone_ends_the_slice);
  RUN_TEST(test_without_demand_no_slice_starts);
  RUN_TEST(test_no_approach_without_a_valid_reading);
  RUN_TEST(test_the_zone_follows_the_moment);
  RUN_TEST(test_slices_survive_the_millis_wrap);
  RUN_TEST(test_the_defaults_are_accepted);
  RUN_TEST(test_the_cut_stays_within_20_and_28_c);
  RUN_TEST(test_the_resume_stays_2_c_below_the_cut_and_above_15_c);
  RUN_TEST(test_the_delay_and_approach_stay_within_bounds);
  RUN_TEST(test_a_patch_changes_only_what_it_names);
  RUN_TEST(test_tenths_of_a_degree);
  RUN_TEST(test_behaviour_names);
  RUN_TEST(test_settings_round_trip_through_nvs);
  RUN_TEST(test_an_unreadable_record_is_refused);
  return UNITY_END();
}
