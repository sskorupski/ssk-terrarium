#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "commands.h"

using terrarium::control::HeatSettings;
using terrarium::control::Mode;
using terrarium::control::TimeWindows;
using namespace terrarium::supervision;

namespace {

constexpr HeatSettings kCurrent{19.0f, 0.5f};

ConfigError parseCommand(const char* json, ConfigCommand& out) {
  return parseConfigPayload(reinterpret_cast<const uint8_t*>(json), std::strlen(json), kCurrent,
                            out);
}

// The heat part of a command, for the tests written before the windows.
ConfigError parseConfig(const char* json, HeatSettings& out) {
  ConfigCommand command{};
  const ConfigError error = parseCommand(json, command);
  if (error == ConfigError::kNone && command.hasHeat) out = command.heat;
  return error;
}

bool parseModeText(const char* text, Mode& out) {
  return parseModePayload(reinterpret_cast<const uint8_t*>(text), std::strlen(text), out);
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-topics-mqtt: cmd/mode carries a bare mode name.
void test_mode_payload_is_a_bare_name() {
  Mode out = Mode::kMaintenance;
  TEST_ASSERT_TRUE(parseModeText("HIBERNATION", out));
  TEST_ASSERT_EQUAL(Mode::kHibernation, out);
  TEST_ASSERT_TRUE(parseModeText("ACTIF", out));
  TEST_ASSERT_EQUAL(Mode::kActive, out);
}

void test_mode_payload_rejects_anything_else() {
  Mode out = Mode::kHibernation;
  for (const char* bad :
       {"actif", "MODE ACTIF", "ACTIF ", "", "ACTIFX", "\"ACTIF\"", "MAINTENANCEMAINTENANCE"}) {
    TEST_ASSERT_FALSE_MESSAGE(parseModeText(bad, out), bad);
  }
  TEST_ASSERT_EQUAL(Mode::kHibernation, out);
  TEST_ASSERT_FALSE(parseModePayload(nullptr, 0, out));
}

// A payload is not NUL-terminated: only `length` bytes count.
void test_mode_payload_reads_only_its_length() {
  Mode out = Mode::kMaintenance;
  const char raw[] = "ACTIFXYZ";
  TEST_ASSERT_TRUE(parseModePayload(reinterpret_cast<const uint8_t*>(raw), 5, out));
  TEST_ASSERT_EQUAL(Mode::kActive, out);
}

// cmd/config is a partial JSON: missing fields keep their current value.
void test_config_payload_merges_with_current_settings() {
  HeatSettings out{};
  TEST_ASSERT_EQUAL(ConfigError::kNone, parseConfig("{\"consigne\":20.5}", out));
  TEST_ASSERT_EQUAL_FLOAT(20.5f, out.setpointC);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, out.hysteresisC);
  TEST_ASSERT_EQUAL(ConfigError::kNone, parseConfig("{\"hysteresis\":1}", out));
  TEST_ASSERT_EQUAL_FLOAT(19.0f, out.setpointC);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out.hysteresisC);
  TEST_ASSERT_EQUAL(ConfigError::kNone, parseConfig("{\"consigne\":18,\"hysteresis\":0.3}", out));
  TEST_ASSERT_EQUAL_FLOAT(18.0f, out.setpointC);
  TEST_ASSERT_EQUAL_FLOAT(0.3f, out.hysteresisC);
}

// RG-reglages-bornes: a malformed command is refused whole.
void test_config_payload_refuses_malformed_json() {
  HeatSettings out = kCurrent;
  TEST_ASSERT_EQUAL(ConfigError::kMalformed, parseConfig("{\"consigne\":", out));
  TEST_ASSERT_EQUAL(ConfigError::kMalformed, parseConfig("20", out));
  TEST_ASSERT_EQUAL(ConfigError::kMalformed, parseConfig("[20]", out));
  TEST_ASSERT_EQUAL(ConfigError::kMalformed, parseConfig("", out));
  TEST_ASSERT_EQUAL_FLOAT(19.0f, out.setpointC);
}

void test_config_payload_refuses_unknown_or_non_numeric_fields() {
  HeatSettings out = kCurrent;
  TEST_ASSERT_EQUAL(ConfigError::kUnknownField, parseConfig("{\"consigne\":20,\"seuil\":3}", out));
  TEST_ASSERT_EQUAL(ConfigError::kNotANumber, parseConfig("{\"consigne\":\"20\"}", out));
  TEST_ASSERT_EQUAL(ConfigError::kNotANumber, parseConfig("{\"consigne\":true}", out));
  TEST_ASSERT_EQUAL(ConfigError::kNotANumber, parseConfig("{\"consigne\":null}", out));
  TEST_ASSERT_EQUAL(ConfigError::kEmpty, parseConfig("{}", out));
  TEST_ASSERT_EQUAL_FLOAT(19.0f, out.setpointC);
}

// Bounds are the regulation loop's call: the parser passes any number on.
void test_config_payload_does_not_judge_bounds() {
  HeatSettings out{};
  TEST_ASSERT_EQUAL(ConfigError::kNone, parseConfig("{\"consigne\":35}", out));
  TEST_ASSERT_EQUAL_FLOAT(35.0f, out.setpointC);
}

// RG-topics-mqtt: a retained command arrives right after subscribing; it is
// never applied.
void test_gate_refuses_commands_right_after_subscribing() {
  CommandGate gate(3000);
  TEST_ASSERT_FALSE(gate.accepts(0));
  gate.subscribed(1000);
  TEST_ASSERT_FALSE(gate.accepts(1000));
  TEST_ASSERT_FALSE(gate.accepts(3999));
  TEST_ASSERT_TRUE(gate.accepts(4000));
}

void test_gate_closes_on_disconnection() {
  CommandGate gate(3000);
  gate.subscribed(0);
  TEST_ASSERT_TRUE(gate.accepts(5000));
  gate.disconnected();
  TEST_ASSERT_FALSE(gate.accepts(6000));
}

void test_events_read_without_the_code() {
  char out[kEventSize];
  TEST_ASSERT_TRUE(formatModeEvent(Mode::kHibernation, ModeOutcome::kApplied, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mode\",\"mode\":\"HIBERNATION\",\"resultat\":\"applique\"}", out);
  TEST_ASSERT_TRUE(formatSettingsEvent(HeatSettings{20.0f, 0.5f}, true, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"consigne\":20.0,\"hysteresis\":0.5,\"resultat\":\"applique\"}",
      out);
  TEST_ASSERT_TRUE(formatSettingsEvent(HeatSettings{30.0f, 0.5f}, false, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"consigne\":30.0,\"hysteresis\":0.5,\"resultat\":\"refuse\","
      "\"raison\":\"hors bornes\"}",
      out);
  TEST_ASSERT_TRUE(formatRejectedCommandEvent("cmd/config", "JSON invalide", out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"commande\",\"sujet\":\"cmd/config\",\"resultat\":\"refusee\","
      "\"raison\":\"JSON invalide\"}",
      out);
}

void test_each_config_error_reads_in_french() {
  TEST_ASSERT_EQUAL_STRING("champ inconnu", describe(ConfigError::kUnknownField));
}

// RG-topics-mqtt: cmd/config carries the windows, which replace the current ones.
void test_config_payload_carries_windows() {
  ConfigCommand out{};
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"plages\":[\"01:35-06:30\",\"14:05-17:05\"]}", out));
  TEST_ASSERT_FALSE(out.hasHeat);
  TEST_ASSERT_FALSE(out.hasTimezone);
  TEST_ASSERT_TRUE(out.hasWindows);
  TEST_ASSERT_EQUAL_UINT8(2, out.windows.count);
  TEST_ASSERT_EQUAL_UINT16(14 * 60 + 5, out.windows.windows[1].startMinute);
  TEST_ASSERT_EQUAL(ConfigError::kNone, parseCommand("{\"plages\":[]}", out));
  TEST_ASSERT_TRUE(out.hasWindows);
  TEST_ASSERT_EQUAL_UINT8(0, out.windows.count);
}

// RG-reglages-bornes: one bad window refuses the whole command.
void test_config_payload_refuses_bad_windows() {
  ConfigCommand out{};
  for (const char* bad :
       {"{\"plages\":[\"25:00-01:00\"]}", "{\"plages\":\"01:35-06:30\"}",
        "{\"plages\":[\"08:00-08:00\"]}", "{\"plages\":[1]}", "{\"plages\":null}",
        "{\"consigne\":20,\"plages\":[\"x\"]}",
        "{\"plages\":[\"00:00-01:00\",\"02:00-03:00\",\"04:00-05:00\",\"06:00-07:00\","
        "\"08:00-09:00\"]}"}) {
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadWindows, parseCommand(bad, out), bad);
  }
}

// ADR-heure-par-ntp: cmd/config carries the time zone, refused unless valid.
void test_config_payload_carries_a_timezone() {
  ConfigCommand out{};
  TEST_ASSERT_EQUAL(
      ConfigError::kNone,
      parseCommand("{\"fuseau\":\"CET-1CEST,M3.5.0,M10.5.0/3\",\"consigne\":20}", out));
  TEST_ASSERT_TRUE(out.hasTimezone);
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out.timezone);
  TEST_ASSERT_TRUE(out.hasHeat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, out.heat.setpointC);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, out.heat.hysteresisC);
  TEST_ASSERT_EQUAL(ConfigError::kBadTimezone, parseCommand("{\"fuseau\":\"Europe/Paris\"}", out));
  TEST_ASSERT_EQUAL(ConfigError::kBadTimezone, parseCommand("{\"fuseau\":1}", out));
}

void test_window_and_timezone_events() {
  char out[kEventSize];
  const TimeWindows windows{2, {{95, 390}, {845, 1025}}};
  TEST_ASSERT_TRUE(formatWindowsEvent(windows, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"plages\":[\"01:35-06:30\",\"14:05-17:05\"],"
      "\"resultat\":\"applique\"}",
      out);
  TEST_ASSERT_TRUE(formatWindowsEvent(TimeWindows{0, {}}, out));
  TEST_ASSERT_EQUAL_STRING("{\"evenement\":\"reglage\",\"plages\":[],\"resultat\":\"applique\"}",
                           out);
  TEST_ASSERT_TRUE(formatTimezoneEvent("CET-1CEST,M3.5.0,M10.5.0/3", out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"fuseau\":\"CET-1CEST,M3.5.0,M10.5.0/3\","
      "\"resultat\":\"applique\"}",
      out);
  TEST_ASSERT_EQUAL_STRING("plages invalides", describe(ConfigError::kBadWindows));
  TEST_ASSERT_EQUAL_STRING("fuseau invalide", describe(ConfigError::kBadTimezone));
}

void test_mode_payload_accepts_plages() {
  Mode out = Mode::kMaintenance;
  TEST_ASSERT_TRUE(parseModeText("PLAGES", out));
  TEST_ASSERT_EQUAL(Mode::kSchedule, out);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mode_payload_is_a_bare_name);
  RUN_TEST(test_mode_payload_rejects_anything_else);
  RUN_TEST(test_mode_payload_reads_only_its_length);
  RUN_TEST(test_config_payload_merges_with_current_settings);
  RUN_TEST(test_config_payload_refuses_malformed_json);
  RUN_TEST(test_config_payload_refuses_unknown_or_non_numeric_fields);
  RUN_TEST(test_config_payload_does_not_judge_bounds);
  RUN_TEST(test_gate_refuses_commands_right_after_subscribing);
  RUN_TEST(test_gate_closes_on_disconnection);
  RUN_TEST(test_events_read_without_the_code);
  RUN_TEST(test_each_config_error_reads_in_french);
  RUN_TEST(test_config_payload_carries_windows);
  RUN_TEST(test_config_payload_refuses_bad_windows);
  RUN_TEST(test_config_payload_carries_a_timezone);
  RUN_TEST(test_window_and_timezone_events);
  RUN_TEST(test_mode_payload_accepts_plages);
  return UNITY_END();
}
