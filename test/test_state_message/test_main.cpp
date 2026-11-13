#include <unity.h>

#include <cstring>

#include "config.h"
#include "state_message.h"

using terrarium::control::AirRejection;
using terrarium::control::AirSource;
using terrarium::control::Mode;
using terrarium::control::ProbeCombination;
using terrarium::control::ProbeRole;
using terrarium::control::TimeWindows;
using terrarium::supervision::formatState;
using terrarium::supervision::StateSnapshot;

namespace cfg = terrarium::config;

namespace {

// The bounds the firmware uses, from config.h.
constexpr terrarium::control::HeatSettingsBounds kBounds{
    cfg::kSetpointMinC, cfg::kSetpointMaxC, cfg::kHeatHysteresisMinC, cfg::kHeatHysteresisMaxC,
    cfg::kAlertTempHighC};
constexpr terrarium::control::FanSettingsBounds kFanBounds{cfg::kFanDurationMinMin,
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

constexpr terrarium::control::SubstrateSettingsBounds kSubstrateBounds{
    cfg::kSubstrateCutMinTenths,      cfg::kSubstrateCutMaxTenths,
    cfg::kSubstrateResumeMinTenths,   cfg::kSubstrateGapMinTenths,
    cfg::kSubstrateApproachMinTenths, cfg::kSubstrateApproachMaxTenths,
    cfg::kSubstrateSilentDelayMinMin, cfg::kSubstrateSilentDelayMaxMin};
constexpr terrarium::control::SubstrateSettings kSubstrateDefaults{
    true,
    cfg::kSubstrateApproachTenths,
    {cfg::kSubstrateCutAtTenths, cfg::kSubstrateResumeBelowTenths},
    {cfg::kSubstrateCutAtTenths, cfg::kSubstrateResumeBelowTenths},
    terrarium::control::SilentProbeBehaviour::kResumeOnAir,
    cfg::kSubstrateSilentDelayMin};

using terrarium::control::DayNightMode;
using terrarium::control::FanMode;
using terrarium::control::Moment;
using terrarium::control::SaturationBehaviour;

StateSnapshot sample() {
  StateSnapshot s{};
  s.airValid = true;
  s.airTemperatureC = 19.24f;
  s.airHumidityRhPercent = 84.4f;
  s.substrateValid = true;
  s.substrateTemperatureC = 20.5f;
  s.airRejection = AirRejection::kNone;
  s.airSafety = true;
  s.airSource = AirSource::kSht45;
  s.combination = ProbeCombination::kPrioritySht45;
  s.probes[0] = {ProbeRole::kInterior, AirRejection::kNone, 19.24f, 84.4f, false};
  s.probes[1] = {ProbeRole::kExterior, AirRejection::kNone, 11.0f, 61.0f, false};
  s.heater = true;
  s.mode = Mode::kActive;
  s.setpointC = 19.0f;
  s.hysteresisC = 0.5f;
  s.uptimeS = 3600;
  s.firmwareVersion = "phase-1.1";
  s.windows = TimeWindows{2, {{95, 390}, {845, 1025}}};
  std::strcpy(s.timezone, "CET-1CEST,M3.5.0,M10.5.0/3");
  s.timeKnown = true;
  s.inWindow = false;
  s.bounds = kBounds;
  s.nightHeat = {19.0f, 0.5f};
  s.dayNight.mode = DayNightMode::kMetronome;
  s.dayNight.fixed = {8 * 60, 21 * 60};
  s.today = {8 * 60, 21 * 60};
  s.moment = Moment::kDay;
  s.fanSettings.day = {FanMode::kMetronome, 5, 60, 30};
  s.fanSettings.night = s.fanSettings.day;
  s.fanSettings.hibernation = {13 * 60, 5};
  s.fanSettings.saturation = {false, SaturationBehaviour::kAiring, 95, 10, 5, 60};
  s.fanBounds = kFanBounds;
  s.substrateSettings = kSubstrateDefaults;
  s.substratePartTenths = 10;
  s.substrateBounds = kSubstrateBounds;
  return s;
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-topics-mqtt: the state carries measures, outputs, mode, setpoint, uptime,
// and the bounds of the settings (RG-reglages-bornes).
void test_formats_the_whole_state() {
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(sample(), out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(
      "{\"air\":{\"temperature\":19.2,\"humidite\":84},\"sonde_retenue\":\"sht45\","
      "\"sondes\":{\"sht45\":{\"role\":\"interieure\",\"temperature\":19.2,\"humidite\":84,"
      "\"cause\":null,\"defaut\":false},\"dht11\":{\"role\":\"exterieure\",\"temperature\":11.0,"
      "\"humidite\":61,\"cause\":null,\"defaut\":false}},"
      "\"substrat\":{\"temperature\":20.5},"
      "\"sorties\":{\"chauffage\":true,\"eclairage\":false,\"pompe\":false,\"ventilateur\":false},"
      "\"mode\":\"ACTIF\",\"consigne\":19.0,\"hysteresis\":0.5,\"defaut_capteur\":false,"
      "\"cause_mesure_air\":null,\"mise_en_securite\":{\"air\":true},"
      "\"combinaison\":\"priorite-sht45\",\"pause_chauffage\":false,"
      "\"coupure_substrat\":false,\"uptime_s\":3600,\"version\":\"phase-1.1\","
      "\"plages\":[\"01:35-06:30\",\"14:05-17:05\"],\"fuseau\":\"CET-1CEST,M3.5.0,M10.5.0/3\","
      "\"heure_connue\":true,\"en_plage\":false,"
      "\"jour_nuit\":{\"mode\":\"metronome\",\"jour\":\"08:00-21:00\"},"
      "\"jour_aujourdhui\":\"08:00-21:00\",\"moment\":\"jour\","
      "\"ventilation\":{\"jour\":{\"mode\":\"metronome\",\"duree\":5,\"periode\":60,\"ecart\":30},"
      "\"nuit\":{\"mode\":\"metronome\",\"duree\":5,\"periode\":60,\"ecart\":30},"
      "\"hibernation\":{\"heure\":\"13:00\",\"duree\":5},\"saturation\":{\"active\":false,"
      "\"comportement\":\"aeration\",\"humidite\":95,\"attente\":10,\"duree\":5,\"pause\":60}},"
      "\"aeration_saturation\":false,\"chauffage_nuit\":{\"consigne\":19.0,\"hysteresis\":0.5},"
      "\"securite_substrat\":{\"active\":true,\"approche\":0.0,"
      "\"jour\":{\"coupure\":24.0,\"reprise\":18.5},\"nuit\":{\"coupure\":24.0,\"reprise\":18.5},"
      "\"sonde_muette\":{\"comportement\":\"reprise-air\",\"delai\":2}},"
      "\"seuils_substrat\":{\"coupure\":24.0,\"reprise\":18.5},\"part_substrat\":1.00,"
      "\"bornes\":{\"consigne\":[15.0,22.0],\"hysteresis\":[0.2,2.0],\"somme_max\":24.0,"
      "\"plages_max\":4,\"fuseau_max\":47,"
      "\"combinaisons\":[\"priorite-sht45\",\"priorite-dht11\",\"moyenne\",\"plus-froide\"],"
      "\"saisons_max\":6,\"ventilation\":{\"duree\":[1,30],\"periode\":[15,240],\"ecart\":[10,50],"
      "\"humidite\":[80,100],\"attente\":[1,60],\"pause\":[15,240]},"
      "\"substrat\":{\"coupure\":[20.0,28.0],\"reprise_min\":15.0,\"ecart_min\":2.0,"
      "\"delai\":[1,30],\"approche\":[0.0,4.0],\"comportements\":[\"reprise-air\",\"maintien\"]}}}",
      out);
}

// RG-topics-mqtt (2026-018): the thresholds of the moment, and the part of
// heating the approach allows.
void test_the_substrate_thresholds_follow_the_moment() {
  StateSnapshot s = sample();
  s.moment = Moment::kNight;
  s.substrateSettings.night = {220, 180};
  s.substrateSettings.approachTenths = 20;
  s.substratePartTenths = 5;
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(std::strstr(
      out, "\"seuils_substrat\":{\"coupure\":22.0,\"reprise\":18.0},\"part_substrat\":0.50,"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"approche\":2.0,"));
}

// A disabled cut publishes no thresholds and no part, and never cuts.
void test_a_disabled_substrate_cut_has_no_thresholds() {
  StateSnapshot s = sample();
  s.substrateSettings.active = false;
  s.substratePartTenths = terrarium::control::kNoSubstratePart;
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"securite_substrat\":{\"active\":false,"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"seuils_substrat\":null,\"part_substrat\":null,"));
}

// A cut forbids all heating: part 0.
void test_a_substrate_cut_allows_no_heating() {
  StateSnapshot s = sample();
  s.substrateCut = true;
  s.substratePartTenths = 0;
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"part_substrat\":0.00,"));
}

// Before the first NTP answer, whether we are in a window is unknown: null.
void test_unknown_time_has_no_window_state() {
  StateSnapshot s = sample();
  s.timeKnown = false;
  s.windows = TimeWindows{0, {}};
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"plages\":[],"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"heure_connue\":false,\"en_plage\":null,\"jour_nuit\":"));
}

// Four windows, the longest time zone, every measure and the bounds still fit
// the state buffer, which the MQTT buffer holds with its topic (mqtt_link.cpp).
void test_longest_state_fits() {
  StateSnapshot s = sample();
  s.mode = Mode::kHibernation;
  s.heater = false;
  s.windows = TimeWindows{4, {{0, 60}, {120, 180}, {240, 300}, {1380, 1439}}};
  std::memset(s.timezone, 'A', terrarium::control::kTimezoneMaxLength - 1);
  s.timezone[terrarium::control::kTimezoneMaxLength - 1] = '0';
  s.timezone[terrarium::control::kTimezoneMaxLength] = '\0';
  s.airTemperatureC = -10.5f;
  s.airHumidityRhPercent = 100.0f;
  s.substrateTemperatureC = -10.5f;
  s.setpointC = cfg::kSetpointMaxC;
  s.hysteresisC = cfg::kHeatHysteresisMaxC;
  s.uptimeS = 4294967295UL;
  s.firmwareVersion = "phase-12.34-567-gabcdef0-dirty";
  s.airRejection = AirRejection::kTemperatureOutOfRange;
  s.airSafety = false;
  s.airSource = AirSource::kAverage;
  s.combination = ProbeCombination::kPrioritySht45;
  for (auto& probe : s.probes) {
    probe = {ProbeRole::kExterior, AirRejection::kNone, -10.5f, 100.0f, false};
  }
  s.dayNight.mode = DayNightMode::kSeasons;
  s.dayNight.seasonCount = terrarium::control::kMaxSeasons;
  for (uint8_t i = 0; i < terrarium::control::kMaxSeasons; ++i) {
    s.dayNight.seasons[i] = {static_cast<uint16_t>(300 + i), {10 * 60 + 10, 22 * 60 + 22}};
  }
  s.today = {10 * 60 + 10, 22 * 60 + 22};
  s.moment = Moment::kNight;
  s.nightHeat = {cfg::kSetpointMaxC, cfg::kHeatHysteresisMaxC};
  s.fanSettings.day = {FanMode::kMetronome, 65535, 65535, 65535};
  s.fanSettings.night = s.fanSettings.day;
  s.fanSettings.hibernation = {23 * 60 + 59, 65535};
  s.fanSettings.saturation = {true, SaturationBehaviour::kContinuous, 65535, 65535, 65535, 65535};
  s.saturationRunning = true;
  s.fanBounds = {65535, 65535, 65535, 65535, 65535, 65535,
                 65535, 65535, 65535, 65535, 65535, 65535};
  s.substrateSettings = {true,
                         -32768,
                         {-32768, -32768},
                         {-32768, -32768},
                         terrarium::control::SilentProbeBehaviour::kResumeOnAir,
                         65535};
  s.substratePartTenths = 10;
  s.substrateBounds = {-32768, -32768, -32768, -32768, -32768, -32768, 65535, 65535};
  char out[terrarium::supervision::kStateSize];
  const size_t length = formatState(s, out, sizeof(out));
  TEST_MESSAGE(out);
  TEST_ASSERT_GREATER_THAN(0, length);
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"maintien\"]}}}"));
}

void test_invalid_readings_are_null() {
  StateSnapshot s = sample();
  s.airValid = false;
  s.substrateValid = false;
  s.sensorFault = true;
  char out[terrarium::supervision::kStateSize];
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"air\":{\"temperature\":null,\"humidite\":null}"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"substrat\":{\"temperature\":null}"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"defaut_capteur\":true"));
}

// RG-coupure-par-le-substrat: the state says whether the substrate cut holds.
void test_publishes_the_substrate_cut() {
  StateSnapshot s = sample();
  s.substrateCut = true;
  char out[terrarium::supervision::kStateSize];
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"pause_chauffage\":false,\"coupure_substrat\":true,"));
}

// RG-topics-mqtt: a rejected reading names its cause, and is never published.
void test_rejected_reading_names_its_cause() {
  StateSnapshot s = sample();
  s.airValid = false;
  s.airRejection = AirRejection::kHumidityOutOfRange;
  s.airSafety = false;
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"air\":{\"temperature\":null,\"humidite\":null}"));
  TEST_ASSERT_NOT_NULL(strstr(
      out, "\"cause_mesure_air\":\"humidite-hors-plage\",\"mise_en_securite\":{\"air\":false}"));
}

// A version string is quoted as is: one that would break the JSON is replaced.
void test_unsafe_version_is_not_copied() {
  StateSnapshot s = sample();
  s.firmwareVersion = "v1\"}";
  char out[terrarium::supervision::kStateSize];
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"version\":\"inconnue\""));
  s.firmwareVersion = nullptr;
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"version\":\"inconnue\""));
}

// RG-jour-et-nuit: seasons in date order; today's day null without time; an
// invalid hibernation time is never published as a time.
void test_publishes_seasons_and_the_moment() {
  StateSnapshot s = sample();
  s.dayNight.mode = DayNightMode::kSeasons;
  s.dayNight.seasonCount = 2;
  s.dayNight.seasons[0] = {78, {7 * 60, 20 * 60}};
  s.dayNight.seasons[1] = {171, {6 * 60, 22 * 60}};
  s.today = {6 * 60 + 30, 20 * 60 + 59};
  s.moment = Moment::kNight;
  s.fanSettings.hibernation.dailyMinute = 24 * 60;
  s.saturationRunning = true;
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(
      std::strstr(out,
                  "\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":[{\"debut\":\"03-20\",\"jour\":"
                  "\"07:00-20:00\"},{\"debut\":\"06-21\",\"jour\":\"06:00-22:00\"}]},"
                  "\"jour_aujourdhui\":\"06:30-20:59\",\"moment\":\"nuit\""));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"hibernation\":{\"heure\":null,"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"aeration_saturation\":true"));
  s.timeKnown = false;
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"jour_aujourdhui\":null"));
}

void test_too_small_buffer_gives_zero() {
  char out[32];
  TEST_ASSERT_EQUAL_size_t(0, formatState(sample(), out, sizeof(out)));
}

// RG-topics-mqtt (2026-007): each probe, its role, and the one that decides.
void test_publishes_each_probe_and_the_retained_one() {
  StateSnapshot s = sample();
  s.airSource = AirSource::kDht11;
  s.airTemperatureC = 16.0f;
  s.airHumidityRhPercent = 80.0f;
  s.combination = ProbeCombination::kColdest;
  s.probes[0] = {ProbeRole::kInterior, AirRejection::kHumidityOutOfRange, 15.2f, 102.0f, true};
  s.probes[1] = {ProbeRole::kInterior, AirRejection::kNone, 16.0f, 80.0f, false};
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(
      strstr(out, "\"air\":{\"temperature\":16.0,\"humidite\":80},\"sonde_retenue\":\"dht11\""));
  // A rejected reading is never published: null, with its cause.
  TEST_ASSERT_NOT_NULL(
      strstr(out,
             "\"sht45\":{\"role\":\"interieure\",\"temperature\":null,\"humidite\":null,"
             "\"cause\":\"humidite-hors-plage\",\"defaut\":true}"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"combinaison\":\"plus-froide\""));
}

void test_no_retained_probe_is_null() {
  StateSnapshot s = sample();
  s.airValid = false;
  s.airSource = AirSource::kNone;
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"sonde_retenue\":null,"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_formats_the_whole_state);
  RUN_TEST(test_the_substrate_thresholds_follow_the_moment);
  RUN_TEST(test_a_disabled_substrate_cut_has_no_thresholds);
  RUN_TEST(test_a_substrate_cut_allows_no_heating);
  RUN_TEST(test_invalid_readings_are_null);
  RUN_TEST(test_publishes_the_substrate_cut);
  RUN_TEST(test_rejected_reading_names_its_cause);
  RUN_TEST(test_unsafe_version_is_not_copied);
  RUN_TEST(test_publishes_seasons_and_the_moment);
  RUN_TEST(test_too_small_buffer_gives_zero);
  RUN_TEST(test_unknown_time_has_no_window_state);
  RUN_TEST(test_longest_state_fits);
  RUN_TEST(test_publishes_each_probe_and_the_retained_one);
  RUN_TEST(test_no_retained_probe_is_null);
  return UNITY_END();
}
