#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "commands.h"

using terrarium::control::HeatSettings;
using terrarium::control::Mode;
using terrarium::control::ProbeCombination;
using terrarium::control::ProbeRole;
using terrarium::control::ProbeSetup;
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

// RG-topics-mqtt: "mise_en_securite" is a partial object of booleans, per sensor.
void test_config_payload_carries_the_air_safety() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"mise_en_securite\":{\"air\":false}}", command));
  TEST_ASSERT_TRUE(command.hasAirSafety);
  TEST_ASSERT_FALSE(command.airSafety);
  TEST_ASSERT_FALSE(command.hasHeat);
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"mise_en_securite\":{\"air\":true}}", command));
  TEST_ASSERT_TRUE(command.airSafety);
}

void test_config_payload_refuses_a_bad_safety() {
  ConfigCommand command{};
  for (const char* json :
       {"{\"mise_en_securite\":false}", "{\"mise_en_securite\":{}}",
        "{\"mise_en_securite\":{\"air\":0}}", "{\"mise_en_securite\":{\"air\":\"non\"}}",
        "{\"mise_en_securite\":{\"substrat\":false}}"}) {
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadSafety, parseCommand(json, command), json);
  }
}

void test_probe_heating_event() {
  char out[kEventSize];
  TEST_ASSERT_TRUE(formatProbeHeatingEvent(terrarium::control::HeatReason::kHighHumidity, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"chauffe-sonde\",\"sonde\":\"sht45\",\"motif\":\"humidite-haute\"}", out);
  TEST_ASSERT_TRUE(
      formatProbeHeatingEvent(terrarium::control::HeatReason::kHumidityOutOfRange, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"chauffe-sonde\",\"sonde\":\"sht45\",\"motif\":\"humidite-hors-plage\"}",
      out);
}

void test_safety_event() {
  char out[kEventSize];
  TEST_ASSERT_TRUE(formatSafetyEvent(false, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"mise_en_securite\":{\"air\":false},\"resultat\":\"applique\"}",
      out);
  TEST_ASSERT_TRUE(formatSafetyEvent(true, out, false));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"mise_en_securite\":{\"air\":true},"
      "\"resultat\":\"applique_non_enregistre\"}",
      out);
}

// RG-topics-mqtt (2026-007): "sondes" is a partial object of roles.
void test_config_payload_carries_probe_roles() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"sondes\":{\"dht11\":\"interieure\"}}", command));
  TEST_ASSERT_FALSE(command.hasRole[0]);
  TEST_ASSERT_TRUE(command.hasRole[1]);
  TEST_ASSERT_EQUAL(ProbeRole::kInterior, command.roles[1]);
  TEST_ASSERT_FALSE(command.hasCombination);

  TEST_ASSERT_EQUAL(
      ConfigError::kNone,
      parseCommand("{\"sondes\":{\"sht45\":\"exterieure\",\"dht11\":\"interieure\"}}", command));
  TEST_ASSERT_TRUE(command.hasRole[0]);
  TEST_ASSERT_EQUAL(ProbeRole::kExterior, command.roles[0]);
  TEST_ASSERT_EQUAL(ProbeRole::kInterior, command.roles[1]);
}

void test_config_payload_refuses_bad_probe_roles() {
  for (const char* json :
       {"{\"sondes\":{}}", "{\"sondes\":{\"bme280\":\"interieure\"}}",
        "{\"sondes\":{\"dht11\":\"dehors\"}}", "{\"sondes\":{\"dht11\":true}}",
        "{\"sondes\":\"dht11\"}", "{\"consigne\":20,\"sondes\":{\"dht11\":1}}"}) {
    ConfigCommand command{};
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadProbes, parseCommand(json, command), json);
  }
}

void test_config_payload_carries_a_combination() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(ConfigError::kNone, parseCommand("{\"combinaison\":\"moyenne\"}", command));
  TEST_ASSERT_TRUE(command.hasCombination);
  TEST_ASSERT_EQUAL(ProbeCombination::kAverage, command.combination);
}

void test_config_payload_refuses_a_bad_combination() {
  for (const char* json :
       {"{\"combinaison\":\"mediane\"}", "{\"combinaison\":2}", "{\"combinaison\":null}"}) {
    ConfigCommand command{};
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadCombination, parseCommand(json, command), json);
  }
}

void test_probe_setup_events() {
  char out[kEventSize];
  const ProbeSetup both{{ProbeRole::kInterior, ProbeRole::kInterior}, ProbeCombination::kColdest};
  TEST_ASSERT_TRUE(formatProbeSetupEvent(both, SettingOutcome::kApplied, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"sondes\":{\"sht45\":\"interieure\",\"dht11\":\"interieure\"},"
      "\"combinaison\":\"plus-froide\",\"resultat\":\"applique\"}",
      out);
  TEST_ASSERT_TRUE(formatProbeSetupEvent(both, SettingOutcome::kNotSaved, out));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"resultat\":\"applique_non_enregistre\"}"));
  const ProbeSetup none{{ProbeRole::kExterior, ProbeRole::kExterior},
                        ProbeCombination::kPrioritySht45};
  TEST_ASSERT_TRUE(formatProbeSetupEvent(none, SettingOutcome::kNoInteriorProbe, out));
  TEST_ASSERT_NOT_NULL(
      std::strstr(out, "\"resultat\":\"refuse\",\"raison\":\"aucune sonde interieure\"}"));
}

// RG-topics-mqtt (2026-015): "ventilation" is partial at two levels; the
// bounds are checked by the loop (RG-reglages-bornes).
void test_config_payload_carries_the_ventilation() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(
      ConfigError::kNone,
      parseCommand("{\"ventilation\":{\"jour\":{\"mode\":\"rubato\",\"duree\":10,\"periode\":90,"
                   "\"ecart\":40},\"nuit\":{\"periode\":120},\"hibernation\":{\"heure\":\"14:30\","
                   "\"duree\":7},\"saturation\":{\"active\":true,\"comportement\":\"continu\","
                   "\"humidite\":97,\"attente\":5,\"duree\":3,\"pause\":30}}}",
                   command));
  TEST_ASSERT_TRUE(command.hasFan);
  const terrarium::control::FanSettingsPatch& f = command.fan;
  TEST_ASSERT_TRUE(f.day.hasMode);
  TEST_ASSERT_EQUAL(terrarium::control::FanMode::kRubato, f.day.mode);
  TEST_ASSERT_EQUAL_UINT16(10, f.day.durationMin);
  TEST_ASSERT_EQUAL_UINT16(90, f.day.periodMin);
  TEST_ASSERT_EQUAL_UINT16(40, f.day.spreadPercent);
  TEST_ASSERT_FALSE(f.night.hasMode);
  TEST_ASSERT_TRUE(f.night.hasPeriod);
  TEST_ASSERT_EQUAL_UINT16(14 * 60 + 30, f.hibernation.dailyMinute);
  TEST_ASSERT_EQUAL_UINT16(7, f.hibernation.durationMin);
  TEST_ASSERT_TRUE(f.saturation.active);
  TEST_ASSERT_EQUAL(terrarium::control::SaturationBehaviour::kContinuous, f.saturation.behaviour);
  TEST_ASSERT_EQUAL_UINT16(97, f.saturation.humidityPercent);
  TEST_ASSERT_EQUAL_UINT16(5, f.saturation.waitMin);
  TEST_ASSERT_EQUAL_UINT16(3, f.saturation.durationMin);
  TEST_ASSERT_EQUAL_UINT16(30, f.saturation.pauseMin);
}

// Out of bounds but well formed: parsed here, refused by the loop.
void test_config_payload_leaves_the_ventilation_bounds_to_the_loop() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"ventilation\":{\"nuit\":{\"duree\":45}}}", command));
  TEST_ASSERT_EQUAL_UINT16(45, command.fan.night.durationMin);
}

void test_config_payload_refuses_a_bad_ventilation() {
  for (const char* json :
       {"{\"ventilation\":{}}", "{\"ventilation\":5}", "{\"ventilation\":{\"jour\":{}}}",
        "{\"ventilation\":{\"duree\":5}}", "{\"ventilation\":{\"jour\":{\"duree\":5.5}}}",
        "{\"ventilation\":{\"jour\":{\"duree\":-1}}}",
        "{\"ventilation\":{\"jour\":{\"duree\":70000}}}",
        "{\"ventilation\":{\"jour\":{\"duree\":\"5\"}}}",
        "{\"ventilation\":{\"nuit\":{\"mode\":\"brise\"}}}",
        "{\"ventilation\":{\"nuit\":{\"heure\":\"13:00\"}}}",
        "{\"ventilation\":{\"hibernation\":{\"heure\":\"25:00\"}}}",
        "{\"ventilation\":{\"hibernation\":{\"periode\":60}}}",
        "{\"ventilation\":{\"saturation\":{\"active\":1}}}",
        "{\"ventilation\":{\"saturation\":{\"comportement\":\"souvent\"}}}",
        "{\"ventilation\":{\"saturation\":{\"humidite\":95.5}}}",
        "{\"consigne\":20,\"ventilation\":{\"jour\":{\"periode\":null}}}"}) {
    ConfigCommand command{};
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadFan, parseCommand(json, command), json);
  }
  TEST_ASSERT_EQUAL_STRING("ventilation invalide", describe(ConfigError::kBadFan));
}

// RG-jour-et-nuit: "jour_nuit" is whole, checked here, seasons sorted.
void test_config_payload_carries_a_day_night() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(
      ConfigError::kNone,
      parseCommand("{\"jour_nuit\":{\"mode\":\"metronome\",\"jour\":\"07:30-20:00\"}}", command));
  TEST_ASSERT_TRUE(command.hasDayNight);
  TEST_ASSERT_EQUAL(terrarium::control::DayNightMode::kMetronome, command.dayNight.mode);
  TEST_ASSERT_EQUAL_UINT16(7 * 60 + 30, command.dayNight.fixed.startMinute);
  ConfigCommand seasons{};
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":["
                                 "{\"debut\":\"09-22\",\"jour\":\"07:30-19:30\"},"
                                 "{\"debut\":\"03-20\",\"jour\":\"07:00-20:00\"}]}}",
                                 seasons));
  TEST_ASSERT_EQUAL(terrarium::control::DayNightMode::kSeasons, seasons.dayNight.mode);
  TEST_ASSERT_EQUAL_UINT8(2, seasons.dayNight.seasonCount);
  TEST_ASSERT_EQUAL_UINT16(78, seasons.dayNight.seasons[0].startDay);
}

void test_config_payload_refuses_a_bad_day_night() {
  for (const char* json :
       {"{\"jour_nuit\":{}}", "{\"jour_nuit\":{\"mode\":\"metronome\"}}",
        "{\"jour_nuit\":{\"mode\":\"metronome\",\"jour\":\"21:00-08:00\"}}",
        "{\"jour_nuit\":{\"mode\":\"metronome\",\"jour\":\"08:00-21:00\",\"saisons\":[]}}",
        "{\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":[]}}",
        "{\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":[{\"debut\":\"02-29\","
        "\"jour\":\"08:00-21:00\"}]}}",
        "{\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":[{\"debut\":\"03-20\","
        "\"jour\":\"08:00-21:00\"},{\"debut\":\"03-20\",\"jour\":\"07:00-21:00\"}]}}",
        "{\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":[{\"debut\":\"03-20\"}]}}",
        "{\"jour_nuit\":{\"mode\":\"lune\",\"jour\":\"08:00-21:00\"}}"}) {
    ConfigCommand command{};
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadDayNight, parseCommand(json, command), json);
  }
  TEST_ASSERT_EQUAL_STRING("jour_nuit invalide", describe(ConfigError::kBadDayNight));
}

void test_config_payload_refuses_seven_seasons() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(ConfigError::kBadDayNight,
                    parseCommand("{\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":["
                                 "{\"debut\":\"01-01\",\"jour\":\"08:00-18:00\"},"
                                 "{\"debut\":\"02-01\",\"jour\":\"08:00-18:00\"},"
                                 "{\"debut\":\"03-01\",\"jour\":\"08:00-18:00\"},"
                                 "{\"debut\":\"04-01\",\"jour\":\"08:00-18:00\"},"
                                 "{\"debut\":\"05-01\",\"jour\":\"08:00-18:00\"},"
                                 "{\"debut\":\"06-01\",\"jour\":\"08:00-18:00\"},"
                                 "{\"debut\":\"07-01\",\"jour\":\"08:00-18:00\"}]}}",
                                 command));
}

// RG-chauffage-par-hysteresis (2026-016): "chauffage_nuit" is partial.
void test_config_payload_carries_the_night_heating() {
  ConfigCommand command{};
  TEST_ASSERT_EQUAL(ConfigError::kNone,
                    parseCommand("{\"chauffage_nuit\":{\"consigne\":17}}", command));
  TEST_ASSERT_TRUE(command.hasNightHeat);
  TEST_ASSERT_TRUE(command.nightHeat.hasSetpoint);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, command.nightHeat.setpointC);
  TEST_ASSERT_FALSE(command.nightHeat.hasHysteresis);
  TEST_ASSERT_FALSE(command.hasHeat);
}

void test_config_payload_refuses_a_bad_night_heating() {
  for (const char* json :
       {"{\"chauffage_nuit\":{}}", "{\"chauffage_nuit\":17}",
        "{\"chauffage_nuit\":{\"consigne\":\"17\"}}", "{\"chauffage_nuit\":{\"consigne\":true}}",
        "{\"chauffage_nuit\":{\"plancher\":12}}"}) {
    ConfigCommand command{};
    TEST_ASSERT_EQUAL_MESSAGE(ConfigError::kBadNightHeat, parseCommand(json, command), json);
  }
  TEST_ASSERT_EQUAL_STRING("chauffage_nuit invalide", describe(ConfigError::kBadNightHeat));
}

// Events of 2026-015 and 2026-016.
void test_setting_events() {
  char out[kEventSize];
  TEST_ASSERT_TRUE(formatFieldEvent("ventilation", SettingOutcome::kApplied, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"champ\":\"ventilation\",\"resultat\":\"applique\"}", out);
  TEST_ASSERT_TRUE(formatFieldEvent("jour_nuit", SettingOutcome::kNotSaved, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"champ\":\"jour_nuit\","
      "\"resultat\":\"applique_non_enregistre\"}",
      out);
  TEST_ASSERT_TRUE(formatFieldEvent("ventilation", SettingOutcome::kInvalidFan, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"champ\":\"ventilation\","
      "\"resultat\":\"refuse\",\"raison\":\"ventilation invalide\"}",
      out);
  TEST_ASSERT_TRUE(formatNightHeatEvent(HeatSettings{17.0f, 0.5f}, true, out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"reglage\",\"chauffage_nuit\":{\"consigne\":17.0,"
      "\"hysteresis\":0.5},\"resultat\":\"applique\"}",
      out);
  TEST_ASSERT_TRUE(formatNightHeatEvent(HeatSettings{23.0f, 1.5f}, false, out));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"resultat\":\"refuse\",\"raison\":\"hors bornes\"}"));
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
  RUN_TEST(test_config_payload_carries_the_air_safety);
  RUN_TEST(test_config_payload_refuses_a_bad_safety);
  RUN_TEST(test_safety_event);
  RUN_TEST(test_probe_heating_event);
  RUN_TEST(test_config_payload_carries_probe_roles);
  RUN_TEST(test_config_payload_refuses_bad_probe_roles);
  RUN_TEST(test_config_payload_carries_a_combination);
  RUN_TEST(test_config_payload_refuses_a_bad_combination);
  RUN_TEST(test_probe_setup_events);
  RUN_TEST(test_config_payload_carries_the_ventilation);
  RUN_TEST(test_config_payload_leaves_the_ventilation_bounds_to_the_loop);
  RUN_TEST(test_config_payload_refuses_a_bad_ventilation);
  RUN_TEST(test_config_payload_carries_a_day_night);
  RUN_TEST(test_config_payload_refuses_a_bad_day_night);
  RUN_TEST(test_config_payload_refuses_seven_seasons);
  RUN_TEST(test_config_payload_carries_the_night_heating);
  RUN_TEST(test_config_payload_refuses_a_bad_night_heating);
  RUN_TEST(test_setting_events);
  return UNITY_END();
}
