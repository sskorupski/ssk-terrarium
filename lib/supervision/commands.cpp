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

bool formatRejectedCommandEvent(const char* subtopic, const char* reason, char (&out)[kEventSize]) {
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"commande\",\"sujet\":\"%s\",\"resultat\":\"refusee\","
                            "\"raison\":\"%s\"}",
                            subtopic, reason),
              sizeof(out));
}

}  // namespace terrarium::supervision
