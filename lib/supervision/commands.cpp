#include "commands.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

namespace terrarium::supervision {

namespace {

constexpr size_t kMaxModeNameLength = 15;
constexpr char kSetpointField[] = "consigne";
constexpr char kHysteresisField[] = "hysteresis";
constexpr char kWindowsField[] = "plages";
constexpr char kTimezoneField[] = "fuseau";
constexpr char kSafetyField[] = "mise_en_securite";
constexpr char kAirSensor[] = "air";
constexpr char kProbesField[] = "sondes";
constexpr char kCombinationField[] = "combinaison";
constexpr char kFanField[] = "ventilation";
constexpr char kDayNightField[] = "jour_nuit";
constexpr char kNightHeatField[] = "chauffage_nuit";
constexpr char kSubstrateField[] = "securite_substrat";

// A whole number of minutes or percent, 0 to 65535: 5.5, booleans and strings
// are refused.
bool readUint(JsonVariantConst value, uint16_t& out) {
  if (value.is<bool>() || !value.is<uint16_t>()) return false;
  out = value.as<uint16_t>();
  return true;
}

bool readText(JsonVariantConst value, const char*& out) {
  if (!value.is<const char*>()) return false;
  out = value.as<const char*>();
  return out != nullptr;
}

// A non-empty object whose every key `field` accepts.
template <typename Field>
bool forEachField(JsonVariantConst value, Field field) {
  if (!value.is<JsonObjectConst>()) return false;
  const JsonObjectConst object = value.as<JsonObjectConst>();
  if (object.size() == 0) return false;
  for (JsonPairConst pair : object) {
    if (!field(pair.key().c_str(), pair.value())) return false;
  }
  return true;
}

bool parseRhythm(JsonVariantConst value, control::FanRhythmPatch& out) {
  return forEachField(value, [&out](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "mode") == 0) {
      const char* text = nullptr;
      return (out.hasMode = readText(v, text) && control::parseFanMode(text, out.mode));
    }
    if (std::strcmp(key, "duree") == 0) return (out.hasDuration = readUint(v, out.durationMin));
    if (std::strcmp(key, "periode") == 0) return (out.hasPeriod = readUint(v, out.periodMin));
    if (std::strcmp(key, "ecart") == 0) return (out.hasSpread = readUint(v, out.spreadPercent));
    return false;
  });
}

bool parseHibernation(JsonVariantConst value, control::HibernationPatch& out) {
  return forEachField(value, [&out](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "heure") == 0) {
      const char* text = nullptr;
      return (out.hasTime = readText(v, text) &&
                            control::parseClockTime(text, std::strlen(text), out.dailyMinute));
    }
    if (std::strcmp(key, "duree") == 0) return (out.hasDuration = readUint(v, out.durationMin));
    return false;
  });
}

bool parseSaturation(JsonVariantConst value, control::SaturationPatch& out) {
  return forEachField(value, [&out](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "active") == 0) {
      if (!v.is<bool>()) return false;
      out.active = v.as<bool>();
      return (out.hasActive = true);
    }
    if (std::strcmp(key, "comportement") == 0) {
      const char* text = nullptr;
      return (out.hasBehaviour = readText(v, text) && control::parseBehaviour(text, out.behaviour));
    }
    if (std::strcmp(key, "humidite") == 0) {
      return (out.hasHumidity = readUint(v, out.humidityPercent));
    }
    if (std::strcmp(key, "attente") == 0) return (out.hasWait = readUint(v, out.waitMin));
    if (std::strcmp(key, "duree") == 0) return (out.hasDuration = readUint(v, out.durationMin));
    if (std::strcmp(key, "pause") == 0) return (out.hasPause = readUint(v, out.pauseMin));
    return false;
  });
}

// {"jour":{...},"nuit":{...},"hibernation":{...},"saturation":{...}}, any of
// them, each partial and non-empty.
bool parseFan(JsonVariantConst value, control::FanSettingsPatch& out) {
  control::FanSettingsPatch patch{};
  const bool ok = forEachField(value, [&patch](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "jour") == 0) return parseRhythm(v, patch.day);
    if (std::strcmp(key, "nuit") == 0) return parseRhythm(v, patch.night);
    if (std::strcmp(key, "hibernation") == 0) return parseHibernation(v, patch.hibernation);
    if (std::strcmp(key, "saturation") == 0) return parseSaturation(v, patch.saturation);
    return false;
  });
  if (ok) out = patch;
  return ok;
}

bool parseDayWindowText(JsonVariantConst value, control::DayWindow& out) {
  const char* text = nullptr;
  return readText(value, text) && control::parseDayWindow(text, std::strlen(text), out);
}

// {"mode":"metronome","jour":"HH:MM-HH:MM"} or
// {"mode":"saisons","saisons":[{"debut":"MM-JJ","jour":"HH:MM-HH:MM"},...]}.
bool parseDayNight(JsonVariantConst value, control::DayNight& out) {
  if (!value.is<JsonObjectConst>()) return false;
  const JsonObjectConst object = value.as<JsonObjectConst>();
  control::DayNight next{};
  const char* mode = nullptr;
  if (object.size() != 2 || !readText(object["mode"], mode) ||
      !control::parseDayNightMode(mode, next.mode)) {
    return false;
  }
  if (next.mode == control::DayNightMode::kMetronome) {
    if (!parseDayWindowText(object["jour"], next.fixed)) return false;
  } else {
    const JsonVariantConst list = object["saisons"];
    if (!list.is<JsonArrayConst>() || list.size() > control::kMaxSeasons) return false;
    for (JsonVariantConst item : list.as<JsonArrayConst>()) {
      if (!item.is<JsonObjectConst>() || item.as<JsonObjectConst>().size() != 2) return false;
      control::Season& season = next.seasons[next.seasonCount];
      const char* start = nullptr;
      if (!readText(item["debut"], start) ||
          !control::parseMonthDay(start, std::strlen(start), season.startDay) ||
          !parseDayWindowText(item["jour"], season.day)) {
        return false;
      }
      ++next.seasonCount;
    }
    control::sortSeasons(next);
  }
  if (!control::isValidDayNight(next)) return false;
  out = next;
  return true;
}

bool readFloat(JsonVariantConst value, float& out) {
  // is<float>() also accepts integers; booleans, strings and null are refused.
  if (value.is<bool>() || !value.is<float>()) return false;
  out = value.as<float>();
  return true;
}

bool parseNightHeat(JsonVariantConst value, NightHeatPatch& out) {
  NightHeatPatch patch{};
  const bool ok = forEachField(value, [&patch](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "consigne") == 0)
      return (patch.hasSetpoint = readFloat(v, patch.setpointC));
    if (std::strcmp(key, "hysteresis") == 0) {
      return (patch.hasHysteresis = readFloat(v, patch.hysteresisC));
    }
    return false;
  });
  if (ok) out = patch;
  return ok;
}

// A temperature to the tenth of a degree: 22.1 and 22 are, 22.05 is not.
bool readTenths(JsonVariantConst value, int16_t& out) {
  float celsius = 0.0f;
  return readFloat(value, celsius) && control::tenthsFromCelsius(celsius, out);
}

bool parseSubstrateThresholds(JsonVariantConst value, control::SubstrateThresholdsPatch& out) {
  return forEachField(value, [&out](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "coupure") == 0) return (out.hasCut = readTenths(v, out.cutTenths));
    if (std::strcmp(key, "reprise") == 0) return (out.hasResume = readTenths(v, out.resumeTenths));
    return false;
  });
}

bool parseSilentProbe(JsonVariantConst value, control::SubstrateSettingsPatch& out) {
  return forEachField(value, [&out](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "comportement") == 0) {
      const char* text = nullptr;
      return (out.hasBehaviour =
                  readText(v, text) && control::parseSilentProbeBehaviour(text, out.behaviour));
    }
    if (std::strcmp(key, "delai") == 0) return (out.hasDelay = readUint(v, out.delayMin));
    return false;
  });
}

// {"active":…,"approche":…,"jour":{…},"nuit":{…},"sonde_muette":{…}}, any of
// them, the objects partial and non-empty.
bool parseSubstrate(JsonVariantConst value, control::SubstrateSettingsPatch& out) {
  control::SubstrateSettingsPatch patch{};
  const bool ok = forEachField(value, [&patch](const char* key, JsonVariantConst v) {
    if (std::strcmp(key, "active") == 0) {
      if (!v.is<bool>()) return false;
      patch.active = v.as<bool>();
      return (patch.hasActive = true);
    }
    if (std::strcmp(key, "approche") == 0) {
      return (patch.hasApproach = readTenths(v, patch.approachTenths));
    }
    if (std::strcmp(key, "jour") == 0) return parseSubstrateThresholds(v, patch.day);
    if (std::strcmp(key, "nuit") == 0) return parseSubstrateThresholds(v, patch.night);
    if (std::strcmp(key, "sonde_muette") == 0) return parseSilentProbe(v, patch);
    return false;
  });
  if (ok) out = patch;
  return ok;
}

// {"sht45": "interieure"|"exterieure", "dht11": ...}, at least one probe.
bool parseProbes(JsonVariantConst value, ConfigCommand& out) {
  if (!value.is<JsonObjectConst>()) return false;
  const JsonObjectConst object = value.as<JsonObjectConst>();
  if (object.size() == 0) return false;
  for (JsonPairConst pair : object) {
    control::Probe probe = control::Probe::kSht45;
    control::ProbeRole role = control::ProbeRole::kInterior;
    if (!control::parseProbe(pair.key().c_str(), probe) || !pair.value().is<const char*>() ||
        !control::parseRole(pair.value().as<const char*>(), role)) {
      return false;
    }
    out.hasRole[static_cast<uint8_t>(probe)] = true;
    out.roles[static_cast<uint8_t>(probe)] = role;
  }
  return true;
}

// {"air": true|false}: the only sensor with a safety today.
bool parseSafety(JsonVariantConst value, bool& out) {
  if (!value.is<JsonObjectConst>()) return false;
  const JsonObjectConst object = value.as<JsonObjectConst>();
  if (object.size() != 1) return false;
  const JsonVariantConst air = object[kAirSensor];
  if (!air.is<bool>()) return false;
  out = air.as<bool>();
  return true;
}

bool parseWindows(JsonVariantConst value, control::TimeWindows& out) {
  if (!value.is<JsonArrayConst>()) return false;
  const JsonArrayConst list = value.as<JsonArrayConst>();
  if (list.size() > control::kMaxTimeWindows) return false;
  control::TimeWindows parsed{0, {}};
  for (JsonVariantConst item : list) {
    if (!item.is<const char*>()) return false;
    const char* text = item.as<const char*>();
    if (!control::parseTimeWindow(text, std::strlen(text), parsed.windows[parsed.count])) {
      return false;
    }
    ++parsed.count;
  }
  out = parsed;
  return true;
}

// Appends `text` at `used`; false if it does not fit.
bool append(char* out, size_t size, size_t& used, const char* text) {
  const size_t length = std::strlen(text);
  if (used + length >= size) return false;
  std::memcpy(out + used, text, length + 1);
  used += length;
  return true;
}

bool fits(int written, size_t size) { return written > 0 && static_cast<size_t>(written) < size; }

}  // namespace

bool parseModePayload(const uint8_t* payload, size_t length, control::Mode& out) {
  if (payload == nullptr || length == 0 || length > kMaxModeNameLength) return false;
  char name[kMaxModeNameLength + 1];
  std::memcpy(name, payload, length);
  name[length] = '\0';
  return control::parseMode(name, out);
}

ConfigError parseConfigPayload(const uint8_t* payload, size_t length,
                               const control::HeatSettings& current, ConfigCommand& out) {
  if (payload == nullptr || length == 0) return ConfigError::kMalformed;
  JsonDocument document;
  if (deserializeJson(document, payload, length) != DeserializationError::Ok ||
      !document.is<JsonObject>()) {
    return ConfigError::kMalformed;
  }
  const JsonObject object = document.as<JsonObject>();
  if (object.size() == 0) return ConfigError::kEmpty;

  ConfigCommand next{};
  next.heat = current;
  for (JsonPair field : object) {
    const char* key = field.key().c_str();
    if (std::strcmp(key, kWindowsField) == 0) {
      if (!parseWindows(field.value(), next.windows)) return ConfigError::kBadWindows;
      next.hasWindows = true;
      continue;
    }
    if (std::strcmp(key, kTimezoneField) == 0) {
      const char* text =
          field.value().is<const char*>() ? field.value().as<const char*>() : nullptr;
      if (text == nullptr || !control::parsePosixTimezone(text, std::strlen(text), next.timezone)) {
        return ConfigError::kBadTimezone;
      }
      next.hasTimezone = true;
      continue;
    }
    if (std::strcmp(key, kSafetyField) == 0) {
      if (!parseSafety(field.value(), next.airSafety)) return ConfigError::kBadSafety;
      next.hasAirSafety = true;
      continue;
    }
    if (std::strcmp(key, kProbesField) == 0) {
      if (!parseProbes(field.value(), next)) return ConfigError::kBadProbes;
      continue;
    }
    if (std::strcmp(key, kCombinationField) == 0) {
      if (!field.value().is<const char*>() ||
          !control::parseCombination(field.value().as<const char*>(), next.combination)) {
        return ConfigError::kBadCombination;
      }
      next.hasCombination = true;
      continue;
    }
    if (std::strcmp(key, kFanField) == 0) {
      if (!parseFan(field.value(), next.fan)) return ConfigError::kBadFan;
      next.hasFan = true;
      continue;
    }
    if (std::strcmp(key, kDayNightField) == 0) {
      if (!parseDayNight(field.value(), next.dayNight)) return ConfigError::kBadDayNight;
      next.hasDayNight = true;
      continue;
    }
    if (std::strcmp(key, kNightHeatField) == 0) {
      if (!parseNightHeat(field.value(), next.nightHeat)) return ConfigError::kBadNightHeat;
      next.hasNightHeat = true;
      continue;
    }
    if (std::strcmp(key, kSubstrateField) == 0) {
      if (!parseSubstrate(field.value(), next.substrate)) return ConfigError::kBadSubstrate;
      next.hasSubstrate = true;
      continue;
    }
    float* target = nullptr;
    if (std::strcmp(key, kSetpointField) == 0) {
      target = &next.heat.setpointC;
    } else if (std::strcmp(key, kHysteresisField) == 0) {
      target = &next.heat.hysteresisC;
    } else {
      return ConfigError::kUnknownField;
    }
    // is<float>() also accepts integers; booleans, strings and null are refused.
    if (field.value().is<bool>() || !field.value().is<float>()) return ConfigError::kNotANumber;
    *target = field.value().as<float>();
    next.hasHeat = true;
  }
  out = next;
  return ConfigError::kNone;
}

const char* describe(ConfigError error) {
  switch (error) {
    case ConfigError::kNone:
      return "accepte";
    case ConfigError::kMalformed:
      return "JSON invalide";
    case ConfigError::kUnknownField:
      return "champ inconnu";
    case ConfigError::kNotANumber:
      return "valeur non numerique";
    case ConfigError::kEmpty:
      return "aucun champ";
    case ConfigError::kBadWindows:
      return "plages invalides";
    case ConfigError::kBadTimezone:
      return "fuseau invalide";
    case ConfigError::kBadSafety:
      return "mise en securite invalide";
    case ConfigError::kBadProbes:
      return "sondes invalides";
    case ConfigError::kBadCombination:
      return "combinaison invalide";
    case ConfigError::kBadFan:
      return "ventilation invalide";
    case ConfigError::kBadDayNight:
      return "jour_nuit invalide";
    case ConfigError::kBadNightHeat:
      return "chauffage_nuit invalide";
    case ConfigError::kBadSubstrate:
      return "securite_substrat invalide";
  }
  return "commande invalide";
}

bool formatModeEvent(control::Mode mode, ModeOutcome outcome, char (&out)[kEventSize]) {
  const char* result = "applique";
  if (outcome == ModeOutcome::kAlreadyActive) result = "deja_actif";
  if (outcome == ModeOutcome::kNotSaved) result = "applique_non_enregistre";
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"mode\",\"mode\":\"%s\",\"resultat\":\"%s\"}",
                            control::toString(mode), result),
              sizeof(out));
}

bool formatSettingsEvent(const control::HeatSettings& settings, bool applied,
                         char (&out)[kEventSize]) {
  const int written =
      applied ? std::snprintf(out, sizeof(out),
                              "{\"evenement\":\"reglage\",\"consigne\":%.1f,\"hysteresis\":%.1f,"
                              "\"resultat\":\"applique\"}",
                              settings.setpointC, settings.hysteresisC)
              : std::snprintf(out, sizeof(out),
                              "{\"evenement\":\"reglage\",\"consigne\":%.1f,\"hysteresis\":%.1f,"
                              "\"resultat\":\"refuse\",\"raison\":\"hors bornes\"}",
                              settings.setpointC, settings.hysteresisC);
  return fits(written, sizeof(out));
}

bool formatWindowsEvent(const control::TimeWindows& windows, char (&out)[kEventSize]) {
  if (!control::isValidTimeWindows(windows)) return false;
  size_t used = 0;
  out[0] = '\0';
  if (!append(out, sizeof(out), used, "{\"evenement\":\"reglage\",\"plages\":[")) return false;
  for (uint8_t i = 0; i < windows.count; ++i) {
    char one[control::kTimeWindowTextLength + 1];
    control::formatTimeWindow(windows.windows[i], one);
    if ((i > 0 && !append(out, sizeof(out), used, ",")) || !append(out, sizeof(out), used, "\"") ||
        !append(out, sizeof(out), used, one) || !append(out, sizeof(out), used, "\"")) {
      return false;
    }
  }
  return append(out, sizeof(out), used, "],\"resultat\":\"applique\"}");
}

bool formatTimezoneEvent(const char* timezone, char (&out)[kEventSize]) {
  if (!control::isValidPosixTimezone(timezone)) return false;
  return fits(
      std::snprintf(out, sizeof(out),
                    "{\"evenement\":\"reglage\",\"fuseau\":\"%s\",\"resultat\":\"applique\"}",
                    timezone),
      sizeof(out));
}

bool formatSafetyEvent(bool airSafety, char (&out)[kEventSize], bool saved) {
  return fits(
      std::snprintf(out, sizeof(out),
                    "{\"evenement\":\"reglage\",\"mise_en_securite\":{\"air\":%s},"
                    "\"resultat\":\"%s\"}",
                    airSafety ? "true" : "false", saved ? "applique" : "applique_non_enregistre"),
      sizeof(out));
}

bool formatProbeHeatingEvent(control::HeatReason reason, char (&out)[kEventSize]) {
  const char* name = control::heatReasonName(reason);
  if (name == nullptr) return false;
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"chauffe-sonde\",\"sonde\":\"sht45\","
                            "\"motif\":\"%s\"}",
                            name),
              sizeof(out));
}

bool formatProbeSetupEvent(const control::ProbeSetup& setup, SettingOutcome outcome,
                           char (&out)[kEventSize]) {
  const char* result = "\"applique\"";
  if (outcome == SettingOutcome::kNotSaved) result = "\"applique_non_enregistre\"";
  if (outcome == SettingOutcome::kNoInteriorProbe) {
    result = "\"refuse\",\"raison\":\"aucune sonde interieure\"";
  }
  return fits(
      std::snprintf(out, sizeof(out),
                    "{\"evenement\":\"reglage\",\"sondes\":{\"%s\":\"%s\",\"%s\":\"%s\"},"
                    "\"combinaison\":\"%s\",\"resultat\":%s}",
                    control::probeName(control::Probe::kSht45), control::roleName(setup.roles[0]),
                    control::probeName(control::Probe::kDht11), control::roleName(setup.roles[1]),
                    control::combinationName(setup.combination), result),
      sizeof(out));
}

bool formatFieldEvent(const char* field, SettingOutcome outcome, char (&out)[kEventSize]) {
  const char* result = "\"applique\"";
  if (outcome == SettingOutcome::kNotSaved) result = "\"applique_non_enregistre\"";
  if (outcome == SettingOutcome::kInvalidFan) {
    result = "\"refuse\",\"raison\":\"ventilation invalide\"";
  }
  if (outcome == SettingOutcome::kInvalidSubstrate) {
    result = "\"refuse\",\"raison\":\"securite_substrat invalide\"";
  }
  return fits(
      std::snprintf(out, sizeof(out),
                    "{\"evenement\":\"reglage\",\"champ\":\"%s\",\"resultat\":%s}", field, result),
      sizeof(out));
}

bool formatSubstrateSafetyEvent(bool active, char (&out)[kEventSize]) {
  return fits(std::snprintf(out, sizeof(out), "{\"evenement\":\"securite-substrat\",\"active\":%s}",
                            active ? "true" : "false"),
              sizeof(out));
}

bool formatNightHeatEvent(const control::HeatSettings& settings, bool applied,
                          char (&out)[kEventSize]) {
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"reglage\",\"chauffage_nuit\":{\"consigne\":%.1f,"
                            "\"hysteresis\":%.1f},\"resultat\":%s}",
                            settings.setpointC, settings.hysteresisC,
                            applied ? "\"applique\"" : "\"refuse\",\"raison\":\"hors bornes\""),
              sizeof(out));
}

bool formatRejectedCommandEvent(const char* subtopic, const char* reason, char (&out)[kEventSize]) {
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"commande\",\"sujet\":\"%s\",\"resultat\":\"refusee\","
                            "\"raison\":\"%s\"}",
                            subtopic, reason),
              sizeof(out));
}

}  // namespace terrarium::supervision
