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
      "\"bornes\":{\"consigne\":[15.0,22.0],\"hysteresis\":[0.2,2.0],\"somme_max\":24.0,"
      "\"plages_max\":4,\"fuseau_max\":47,"
      "\"combinaisons\":[\"priorite-sht45\",\"priorite-dht11\",\"moyenne\",\"plus-froide\"]}}",
      out);
}

// Before the first NTP answer, whether we are in a window is unknown: null.
void test_unknown_time_has_no_window_state() {
  StateSnapshot s = sample();
  s.timeKnown = false;
  s.windows = TimeWindows{0, {}};
  char out[terrarium::supervision::kStateSize];
  TEST_ASSERT_GREATER_THAN(0, formatState(s, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"plages\":[],"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"heure_connue\":false,\"en_plage\":null,\"bornes\":"));
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
  char out[terrarium::supervision::kStateSize];
  const size_t length = formatState(s, out, sizeof(out));
  TEST_MESSAGE(out);
  TEST_ASSERT_GREATER_THAN(0, length);
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"plus-froide\"]}}"));
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
  RUN_TEST(test_invalid_readings_are_null);
  RUN_TEST(test_publishes_the_substrate_cut);
  RUN_TEST(test_rejected_reading_names_its_cause);
  RUN_TEST(test_unsafe_version_is_not_copied);
  RUN_TEST(test_too_small_buffer_gives_zero);
  RUN_TEST(test_unknown_time_has_no_window_state);
  RUN_TEST(test_longest_state_fits);
  RUN_TEST(test_publishes_each_probe_and_the_retained_one);
  RUN_TEST(test_no_retained_probe_is_null);
  return UNITY_END();
}
