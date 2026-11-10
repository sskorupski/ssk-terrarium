#include <unity.h>

#include <cmath>
#include <cstdint>

#include "air_probes.h"

using terrarium::control::AirMeasure;
using terrarium::control::AirRejection;
using terrarium::control::AirSource;
using terrarium::control::combinationName;
using terrarium::control::combineAir;
using terrarium::control::isValidProbeSetup;
using terrarium::control::kDefaultProbeSetup;
using terrarium::control::parseCombination;
using terrarium::control::parseProbe;
using terrarium::control::parseProbeCommand;
using terrarium::control::parseRole;
using terrarium::control::priorityInteriorProbe;
using terrarium::control::Probe;
using terrarium::control::ProbeCombination;
using terrarium::control::ProbeRole;
using terrarium::control::ProbeSample;
using terrarium::control::ProbeSetup;
using terrarium::control::probeSetupFromStored;
using terrarium::control::probeSetupToStored;
using terrarium::control::SettingCommand;
using terrarium::control::sourceName;

namespace {

constexpr ProbeRole kIn = ProbeRole::kInterior;
constexpr ProbeRole kOut = ProbeRole::kExterior;

ProbeSetup setup(ProbeRole sht45, ProbeRole dht11, ProbeCombination combination) {
  return ProbeSetup{{sht45, dht11}, combination};
}

ProbeSample accepted(float temperatureC, float humidity) {
  return {AirRejection::kNone, temperatureC, humidity};
}

const ProbeSample kSilent{AirRejection::kSilent, NAN, NAN};

// RG-capteur-en-defaut: 102 % HR, the reading of 2026-11-09.
ProbeSample soaked(float temperatureC) {
  return {AirRejection::kHumidityOutOfRange, temperatureC, 102.0f};
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- Setup -------------------------------------------------------------------

void test_default_setup_is_the_sht45_alone_interior() {
  TEST_ASSERT_TRUE(isValidProbeSetup(kDefaultProbeSetup));
  TEST_ASSERT_EQUAL(kIn, kDefaultProbeSetup.roles[0]);
  TEST_ASSERT_EQUAL(kOut, kDefaultProbeSetup.roles[1]);
  TEST_ASSERT_EQUAL(ProbeCombination::kPrioritySht45, kDefaultProbeSetup.combination);
}

void test_setup_without_interior_probe_is_invalid() {
  TEST_ASSERT_FALSE(isValidProbeSetup(setup(kOut, kOut, ProbeCombination::kPrioritySht45)));
}

void test_either_probe_alone_interior_is_valid() {
  TEST_ASSERT_TRUE(isValidProbeSetup(setup(kIn, kOut, ProbeCombination::kAverage)));
  TEST_ASSERT_TRUE(isValidProbeSetup(setup(kOut, kIn, ProbeCombination::kColdest)));
  TEST_ASSERT_TRUE(isValidProbeSetup(setup(kIn, kIn, ProbeCombination::kPriorityDht11)));
}

void test_setup_out_of_range_is_invalid() {
  ProbeSetup bad = kDefaultProbeSetup;
  bad.combination = static_cast<ProbeCombination>(4);
  TEST_ASSERT_FALSE(isValidProbeSetup(bad));
  bad = kDefaultProbeSetup;
  bad.roles[1] = static_cast<ProbeRole>(2);
  TEST_ASSERT_FALSE(isValidProbeSetup(bad));
}

void test_stored_setup_round_trips() {
  const ProbeSetup all[] = {
      setup(kIn, kOut, ProbeCombination::kPrioritySht45),
      setup(kOut, kIn, ProbeCombination::kPriorityDht11),
      setup(kIn, kIn, ProbeCombination::kAverage),
      setup(kIn, kIn, ProbeCombination::kColdest),
  };
  for (const ProbeSetup& one : all) {
    ProbeSetup read = kDefaultProbeSetup;
    TEST_ASSERT_TRUE(probeSetupFromStored(probeSetupToStored(one), read));
    TEST_ASSERT_TRUE(read == one);
  }
}

void test_stored_setup_without_interior_probe_is_refused() {
  ProbeSetup read = setup(kIn, kIn, ProbeCombination::kAverage);
  const uint8_t noInterior = probeSetupToStored(setup(kOut, kOut, ProbeCombination::kAverage));
  TEST_ASSERT_FALSE(probeSetupFromStored(noInterior, read));
  TEST_ASSERT_TRUE(read == setup(kIn, kIn, ProbeCombination::kAverage));
}

void test_stored_garbage_is_refused() {
  ProbeSetup read = kDefaultProbeSetup;
  TEST_ASSERT_FALSE(probeSetupFromStored(0xFF, read));
  TEST_ASSERT_TRUE(read == kDefaultProbeSetup);
}

// --- Names -------------------------------------------------------------------

void test_names_round_trip() {
  Probe probe = Probe::kSht45;
  TEST_ASSERT_TRUE(parseProbe("dht11", probe));
  TEST_ASSERT_EQUAL(Probe::kDht11, probe);
  TEST_ASSERT_TRUE(parseProbe("SHT45", probe));
  TEST_ASSERT_EQUAL(Probe::kSht45, probe);
  TEST_ASSERT_FALSE(parseProbe("bme280", probe));

  ProbeRole role = kIn;
  TEST_ASSERT_TRUE(parseRole("exterieure", role));
  TEST_ASSERT_EQUAL(kOut, role);
  TEST_ASSERT_FALSE(parseRole("dehors", role));

  for (uint8_t i = 0; i < terrarium::control::kCombinationCount; ++i) {
    const auto combination = static_cast<ProbeCombination>(i);
    ProbeCombination parsed = ProbeCombination::kColdest;
    TEST_ASSERT_TRUE(parseCombination(combinationName(combination), parsed));
    TEST_ASSERT_EQUAL(combination, parsed);
  }
  TEST_ASSERT_EQUAL_STRING("priorite-sht45", combinationName(ProbeCombination::kPrioritySht45));
  TEST_ASSERT_EQUAL_STRING("plus-froide", combinationName(ProbeCombination::kColdest));
  ProbeCombination parsed = ProbeCombination::kAverage;
  TEST_ASSERT_FALSE(parseCombination("mediane", parsed));
  TEST_ASSERT_FALSE(parseCombination(nullptr, parsed));
}

void test_source_names() {
  TEST_ASSERT_NULL(sourceName(AirSource::kNone));
  TEST_ASSERT_EQUAL_STRING("sht45", sourceName(AirSource::kSht45));
  TEST_ASSERT_EQUAL_STRING("dht11", sourceName(AirSource::kDht11));
  TEST_ASSERT_EQUAL_STRING("moyenne", sourceName(AirSource::kAverage));
}

// --- Priority interior probe --------------------------------------------------

void test_priority_probe_follows_the_combination() {
  TEST_ASSERT_EQUAL(Probe::kSht45,
                    priorityInteriorProbe(setup(kIn, kIn, ProbeCombination::kPrioritySht45)));
  TEST_ASSERT_EQUAL(Probe::kDht11,
                    priorityInteriorProbe(setup(kIn, kIn, ProbeCombination::kPriorityDht11)));
  TEST_ASSERT_EQUAL(Probe::kSht45,
                    priorityInteriorProbe(setup(kIn, kIn, ProbeCombination::kAverage)));
  TEST_ASSERT_EQUAL(Probe::kSht45,
                    priorityInteriorProbe(setup(kIn, kIn, ProbeCombination::kColdest)));
}

void test_priority_probe_is_never_exterior() {
  TEST_ASSERT_EQUAL(Probe::kDht11,
                    priorityInteriorProbe(setup(kOut, kIn, ProbeCombination::kPrioritySht45)));
  TEST_ASSERT_EQUAL(Probe::kSht45,
                    priorityInteriorProbe(setup(kIn, kOut, ProbeCombination::kPriorityDht11)));
}

// --- Combination (RG-sondes-d-air) --------------------------------------------

void test_default_setup_regulates_on_the_sht45_alone() {
  const AirMeasure air =
      combineAir(kDefaultProbeSetup, {accepted(19.2f, 84.0f), accepted(12.0f, 60.0f)});
  TEST_ASSERT_EQUAL(AirSource::kSht45, air.source);
  TEST_ASSERT_EQUAL_FLOAT(19.2f, air.temperatureC);
  TEST_ASSERT_EQUAL_FLOAT(84.0f, air.humidityRhPercent);
  TEST_ASSERT_EQUAL(AirRejection::kNone, air.watchRejection);
  TEST_ASSERT_EQUAL(AirRejection::kNone, air.cause);
}

void test_exterior_probe_never_takes_over() {
  const AirMeasure air = combineAir(kDefaultProbeSetup, {soaked(15.2f), accepted(12.0f, 60.0f)});
  TEST_ASSERT_EQUAL(AirSource::kNone, air.source);
  TEST_ASSERT_EQUAL(AirRejection::kHumidityOutOfRange, air.cause);
}

void test_priority_falls_back_on_the_other_interior_probe() {
  const ProbeSetup both = setup(kIn, kIn, ProbeCombination::kPrioritySht45);
  const AirMeasure air = combineAir(both, {soaked(15.2f), accepted(16.0f, 80.0f)});
  TEST_ASSERT_EQUAL(AirSource::kDht11, air.source);
  TEST_ASSERT_EQUAL_FLOAT(16.0f, air.temperatureC);
  TEST_ASSERT_EQUAL_FLOAT(80.0f, air.humidityRhPercent);
  TEST_ASSERT_EQUAL(AirRejection::kNone, air.watchRejection);
  TEST_ASSERT_EQUAL(AirRejection::kNone, air.cause);
}

void test_priority_keeps_the_priority_probe_when_both_are_accepted() {
  const AirMeasure sht = combineAir(setup(kIn, kIn, ProbeCombination::kPrioritySht45),
                                    {accepted(19.0f, 85.0f), accepted(17.0f, 70.0f)});
  TEST_ASSERT_EQUAL(AirSource::kSht45, sht.source);
  const AirMeasure dht = combineAir(setup(kIn, kIn, ProbeCombination::kPriorityDht11),
                                    {accepted(19.0f, 85.0f), accepted(17.0f, 70.0f)});
  TEST_ASSERT_EQUAL(AirSource::kDht11, dht.source);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, dht.temperatureC);
}

void test_average_of_two_accepted_probes() {
  const AirMeasure air = combineAir(setup(kIn, kIn, ProbeCombination::kAverage),
                                    {accepted(19.0f, 85.0f), accepted(18.0f, 75.0f)});
  TEST_ASSERT_EQUAL(AirSource::kAverage, air.source);
  TEST_ASSERT_EQUAL_FLOAT(18.5f, air.temperatureC);
  TEST_ASSERT_EQUAL_FLOAT(80.0f, air.humidityRhPercent);
}

void test_average_of_one_accepted_probe_is_that_probe() {
  const AirMeasure air =
      combineAir(setup(kIn, kIn, ProbeCombination::kAverage), {kSilent, accepted(18.0f, 75.0f)});
  TEST_ASSERT_EQUAL(AirSource::kDht11, air.source);
  TEST_ASSERT_EQUAL_FLOAT(18.0f, air.temperatureC);
}

void test_coldest_takes_the_colder_probe_whole() {
  const AirMeasure air = combineAir(setup(kIn, kIn, ProbeCombination::kColdest),
                                    {accepted(19.0f, 85.0f), accepted(17.0f, 70.0f)});
  TEST_ASSERT_EQUAL(AirSource::kDht11, air.source);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, air.temperatureC);
  TEST_ASSERT_EQUAL_FLOAT(70.0f, air.humidityRhPercent);
}

void test_coldest_tie_goes_to_the_sht45() {
  const AirMeasure air = combineAir(setup(kIn, kIn, ProbeCombination::kColdest),
                                    {accepted(18.0f, 85.0f), accepted(18.0f, 70.0f)});
  TEST_ASSERT_EQUAL(AirSource::kSht45, air.source);
}

void test_dht11_alone_interior_regulates_alone() {
  const AirMeasure air = combineAir(setup(kOut, kIn, ProbeCombination::kPrioritySht45),
                                    {accepted(19.0f, 85.0f), accepted(17.0f, 70.0f)});
  TEST_ASSERT_EQUAL(AirSource::kDht11, air.source);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, air.temperatureC);
}

// --- No interior probe accepted (RG-capteur-en-defaut) ------------------------

void test_all_interior_silent_feeds_silence() {
  const AirMeasure air =
      combineAir(setup(kIn, kIn, ProbeCombination::kAverage), {kSilent, kSilent});
  TEST_ASSERT_EQUAL(AirSource::kNone, air.source);
  TEST_ASSERT_EQUAL(AirRejection::kSilent, air.watchRejection);
  TEST_ASSERT_EQUAL(AirRejection::kSilent, air.cause);
}

void test_rejected_reading_of_the_priority_probe_is_kept_for_a_disabled_safety() {
  const AirMeasure air =
      combineAir(setup(kIn, kIn, ProbeCombination::kPriorityDht11), {soaked(15.0f), soaked(16.0f)});
  TEST_ASSERT_EQUAL(AirSource::kNone, air.source);
  TEST_ASSERT_EQUAL(AirRejection::kHumidityOutOfRange, air.watchRejection);
  TEST_ASSERT_EQUAL_FLOAT(16.0f, air.temperatureC);
}

void test_silent_priority_probe_yields_to_the_one_that_answered() {
  const AirMeasure air =
      combineAir(setup(kIn, kIn, ProbeCombination::kPrioritySht45), {kSilent, soaked(16.0f)});
  TEST_ASSERT_EQUAL(AirSource::kNone, air.source);
  TEST_ASSERT_EQUAL(AirRejection::kHumidityOutOfRange, air.watchRejection);
  TEST_ASSERT_EQUAL_FLOAT(16.0f, air.temperatureC);
  // The published cause stays the priority probe's.
  TEST_ASSERT_EQUAL(AirRejection::kSilent, air.cause);
}

// --- Serial link ----------------------------------------------------------------

void test_serial_show_and_unknown() {
  ProbeSetup out = kDefaultProbeSetup;
  TEST_ASSERT_EQUAL(SettingCommand::kShow, parseProbeCommand("SONDES", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kShow, parseProbeCommand("SONDES\r", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kNone,
                    parseProbeCommand("MODE ACTIF", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kNone, parseProbeCommand(nullptr, kDefaultProbeSetup, out));
}

void test_serial_sets_a_role() {
  ProbeSetup out = kDefaultProbeSetup;
  TEST_ASSERT_EQUAL(SettingCommand::kSet,
                    parseProbeCommand("SONDE dht11 INTERIEURE", kDefaultProbeSetup, out));
  TEST_ASSERT_TRUE(out == setup(kIn, kIn, ProbeCombination::kPrioritySht45));
}

void test_serial_sets_a_combination() {
  ProbeSetup out = kDefaultProbeSetup;
  TEST_ASSERT_EQUAL(SettingCommand::kSet,
                    parseProbeCommand("COMBINAISON plus-froide\r", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(ProbeCombination::kColdest, out.combination);
}

void test_serial_refuses_a_setup_without_interior_probe() {
  ProbeSetup out = setup(kIn, kIn, ProbeCombination::kAverage);
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid,
                    parseProbeCommand("SONDE sht45 exterieure", kDefaultProbeSetup, out));
  TEST_ASSERT_TRUE(out == setup(kIn, kIn, ProbeCombination::kAverage));
}

void test_serial_refuses_bad_values() {
  ProbeSetup out = kDefaultProbeSetup;
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid,
                    parseProbeCommand("SONDE bme280 INTERIEURE", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid,
                    parseProbeCommand("SONDE dht11", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid,
                    parseProbeCommand("SONDE dht11 DEHORS", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid,
                    parseProbeCommand("COMBINAISON", kDefaultProbeSetup, out));
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid,
                    parseProbeCommand("COMBINAISON mediane", kDefaultProbeSetup, out));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_setup_is_the_sht45_alone_interior);
  RUN_TEST(test_setup_without_interior_probe_is_invalid);
  RUN_TEST(test_either_probe_alone_interior_is_valid);
  RUN_TEST(test_setup_out_of_range_is_invalid);
  RUN_TEST(test_stored_setup_round_trips);
  RUN_TEST(test_stored_setup_without_interior_probe_is_refused);
  RUN_TEST(test_stored_garbage_is_refused);
  RUN_TEST(test_names_round_trip);
  RUN_TEST(test_source_names);
  RUN_TEST(test_priority_probe_follows_the_combination);
  RUN_TEST(test_priority_probe_is_never_exterior);
  RUN_TEST(test_default_setup_regulates_on_the_sht45_alone);
  RUN_TEST(test_exterior_probe_never_takes_over);
  RUN_TEST(test_priority_falls_back_on_the_other_interior_probe);
  RUN_TEST(test_priority_keeps_the_priority_probe_when_both_are_accepted);
  RUN_TEST(test_average_of_two_accepted_probes);
  RUN_TEST(test_average_of_one_accepted_probe_is_that_probe);
  RUN_TEST(test_coldest_takes_the_colder_probe_whole);
  RUN_TEST(test_coldest_tie_goes_to_the_sht45);
  RUN_TEST(test_dht11_alone_interior_regulates_alone);
  RUN_TEST(test_all_interior_silent_feeds_silence);
  RUN_TEST(test_rejected_reading_of_the_priority_probe_is_kept_for_a_disabled_safety);
  RUN_TEST(test_silent_priority_probe_yields_to_the_one_that_answered);
  RUN_TEST(test_serial_show_and_unknown);
  RUN_TEST(test_serial_sets_a_role);
  RUN_TEST(test_serial_sets_a_combination);
  RUN_TEST(test_serial_refuses_a_setup_without_interior_probe);
  RUN_TEST(test_serial_refuses_bad_values);
  return UNITY_END();
}
