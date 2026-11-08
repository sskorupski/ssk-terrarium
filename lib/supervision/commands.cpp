#include "commands.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

namespace terrarium::supervision {

namespace {

constexpr size_t kMaxModeNameLength = 15;
constexpr char kSetpointField[] = "consigne";
constexpr char kHysteresisField[] = "hysteresis";

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
                               const control::HeatSettings& current, control::HeatSettings& out) {
  if (payload == nullptr || length == 0) return ConfigError::kMalformed;
  JsonDocument document;
  if (deserializeJson(document, payload, length) != DeserializationError::Ok ||
      !document.is<JsonObject>()) {
    return ConfigError::kMalformed;
  }
  const JsonObject object = document.as<JsonObject>();
  if (object.size() == 0) return ConfigError::kEmpty;

  control::HeatSettings next = current;
  for (JsonPair field : object) {
    float* target = nullptr;
    if (std::strcmp(field.key().c_str(), kSetpointField) == 0) {
      target = &next.setpointC;
    } else if (std::strcmp(field.key().c_str(), kHysteresisField) == 0) {
      target = &next.hysteresisC;
    } else {
      return ConfigError::kUnknownField;
    }
    // is<float>() also accepts integers; booleans, strings and null are refused.
    if (field.value().is<bool>() || !field.value().is<float>()) return ConfigError::kNotANumber;
    *target = field.value().as<float>();
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

bool formatRejectedCommandEvent(const char* subtopic, const char* reason, char (&out)[kEventSize]) {
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"commande\",\"sujet\":\"%s\",\"resultat\":\"refusee\","
                            "\"raison\":\"%s\"}",
                            subtopic, reason),
              sizeof(out));
}

}  // namespace terrarium::supervision
