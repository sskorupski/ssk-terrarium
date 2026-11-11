#include "update_command.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

namespace terrarium::update {

namespace {

constexpr char kLicenceField[] = "licence";
constexpr char kActionField[] = "action";
constexpr char kVersionField[] = "version";
constexpr char kCheckAction[] = "verifier";
constexpr char kInstallAction[] = "installer";
constexpr char kSerialKeyword[] = "licence ";
constexpr char kSerialKeywordUpper[] = "LICENCE ";
constexpr size_t kSerialKeywordLength = sizeof(kSerialKeyword) - 1;

// base64url and the dots of a JWT: safe in an HTTP header, on one line.
bool isLicenceChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '_' || c == '.';
}

bool fits(int written, size_t size) { return written > 0 && static_cast<size_t>(written) < size; }

const char* stepName(VersionStep step) {
  switch (step) {
    case VersionStep::kAvailable:
      return "disponible";
    case VersionStep::kUpToDate:
      return "a-jour";
    case VersionStep::kAccepted:
      return "acceptee";
    case VersionStep::kRestart:
      return "redemarrage";
    case VersionStep::kValidated:
      return "validee";
    case VersionStep::kCancelled:
      return "annulee";
  }
  return "inconnue";
}

}  // namespace

bool isValidLicence(const char* text, size_t length) {
  if (text == nullptr || length == 0 || length > kLicenceMaxLength) return false;
  for (size_t i = 0; i < length; ++i) {
    if (!isLicenceChar(text[i])) return false;
  }
  return true;
}

UpdateCommandError parseUpdatePayload(const uint8_t* payload, size_t length, UpdateCommand& out) {
  if (payload == nullptr || length == 0) return UpdateCommandError::kMalformed;
  JsonDocument document;
  if (deserializeJson(document, payload, length) != DeserializationError::Ok ||
      !document.is<JsonObject>()) {
    return UpdateCommandError::kMalformed;
  }
  const JsonObject object = document.as<JsonObject>();
  const JsonVariantConst licence = object[kLicenceField];
  if (!licence.isNull()) {
    if (object.size() != 1) return UpdateCommandError::kUnknownField;
    if (!licence.is<JsonString>()) return UpdateCommandError::kBadLicence;
    const JsonString token = licence.as<JsonString>();
    if (!isValidLicence(token.c_str(), token.size())) return UpdateCommandError::kBadLicence;
    out.action = UpdateAction::kLicence;
    std::memcpy(out.licence, token.c_str(), token.size());
    out.licence[token.size()] = '\0';
    return UpdateCommandError::kNone;
  }

  const JsonVariantConst action = object[kActionField];
  if (action.isNull()) return UpdateCommandError::kUnknownField;
  if (!action.is<const char*>()) return UpdateCommandError::kBadAction;
  if (std::strcmp(action.as<const char*>(), kCheckAction) == 0) {
    if (object.size() != 1) return UpdateCommandError::kUnknownField;
    out.action = UpdateAction::kCheck;
    return UpdateCommandError::kNone;
  }
  if (std::strcmp(action.as<const char*>(), kInstallAction) != 0) {
    return UpdateCommandError::kBadAction;
  }
  const JsonVariantConst version = object[kVersionField];
  SemVer requested{};
  if (!version.is<const char*>() || !parseVersion(version.as<const char*>(), requested)) {
    return UpdateCommandError::kBadVersion;
  }
  if (object.size() != 2) return UpdateCommandError::kUnknownField;
  out.action = UpdateAction::kInstall;
  out.version = requested;
  return UpdateCommandError::kNone;
}

const char* describe(UpdateCommandError error) {
  switch (error) {
    case UpdateCommandError::kNone:
      return "accepte";
    case UpdateCommandError::kMalformed:
      return "JSON invalide";
    case UpdateCommandError::kUnknownField:
      return "champ inconnu";
    case UpdateCommandError::kBadAction:
      return "action inconnue (verifier, installer)";
    case UpdateCommandError::kBadVersion:
      return "version invalide (x.y.z)";
    case UpdateCommandError::kBadLicence:
      return "licence invalide";
  }
  return "commande invalide";
}

LicenceLine parseLicenceLine(const char* line, char (&out)[kLicenceSize]) {
  if (line == nullptr) return LicenceLine::kNone;
  size_t length = std::strlen(line);
  if (length > 0 && line[length - 1] == '\r') --length;
  const bool keywordAlone =
      length == kSerialKeywordLength - 1 && (std::strncmp(line, kSerialKeyword, length) == 0 ||
                                             std::strncmp(line, kSerialKeywordUpper, length) == 0);
  if (keywordAlone) return LicenceLine::kInvalid;
  if (std::strncmp(line, kSerialKeyword, kSerialKeywordLength) != 0 &&
      std::strncmp(line, kSerialKeywordUpper, kSerialKeywordLength) != 0) {
    return LicenceLine::kNone;
  }
  const char* token = line + kSerialKeywordLength;
  const size_t tokenLength = length - kSerialKeywordLength;
  if (!isValidLicence(token, tokenLength)) return LicenceLine::kInvalid;
  std::memcpy(out, token, tokenLength);
  out[tokenLength] = '\0';
  return LicenceLine::kValid;
}

const char* describe(Refusal refusal) {
  switch (refusal) {
    case Refusal::kFault:
      return "defaut en cours";
    case Refusal::kBusy:
      return "mise a jour en cours";
    case Refusal::kNoLicence:
      return "sans licence";
    case Refusal::kNotOffered:
      return "version non proposee";
    case Refusal::kLicenceRefused:
      return "licence refusee";
    case Refusal::kQuota:
      return "quota depasse";
    case Refusal::kUnreachable:
      return "service injoignable";
    case Refusal::kUnknownVersion:
      return "version inconnue";
  }
  return "refusee";
}

const char* describe(Failure failure) {
  switch (failure) {
    case Failure::kFault:
      return "defaut en cours";
    case Failure::kStalled:
      return "telechargement interrompu";
    case Failure::kBadSignature:
      return "signature invalide";
    case Failure::kBadImage:
      return "image invalide";
    case Failure::kTooLarge:
      return "image trop grande";
    case Failure::kNotNewer:
      return "version non plus recente";
    case Failure::kOtherPhase:
      return "phase differente";
  }
  return "echec";
}

bool formatLicenceEvent(LicenceOutcome outcome, char (&out)[supervision::kEventSize]) {
  const char* result = "applique";
  if (outcome == LicenceOutcome::kNotSaved) result = "applique_non_enregistre";
  if (outcome == LicenceOutcome::kRefused) result = "refuse";
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"mise-a-jour\",\"etape\":\"licence\","
                            "\"resultat\":\"%s\"}",
                            result),
              sizeof(out));
}

bool formatRefusalEvent(Refusal refusal, char (&out)[supervision::kEventSize]) {
  return fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"mise-a-jour\",\"etape\":\"refusee\","
                            "\"raison\":\"%s\"}",
                            describe(refusal)),
              sizeof(out));
}

bool formatVersionEvent(VersionStep step, const SemVer& version,
                        char (&out)[supervision::kEventSize]) {
  char text[kVersionTextSize];
  return format(version, text) &&
         fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"mise-a-jour\",\"etape\":\"%s\",\"version\":\"%s\"}",
                            stepName(step), text),
              sizeof(out));
}

bool formatProgressEvent(const SemVer& version, uint8_t percent,
                         char (&out)[supervision::kEventSize]) {
  char text[kVersionTextSize];
  return format(version, text) &&
         fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"mise-a-jour\",\"etape\":\"telechargement\","
                            "\"version\":\"%s\",\"progression\":%u}",
                            text, percent),
              sizeof(out));
}

bool formatFailureEvent(const SemVer& version, Failure failure,
                        char (&out)[supervision::kEventSize]) {
  char text[kVersionTextSize];
  return format(version, text) &&
         fits(std::snprintf(out, sizeof(out),
                            "{\"evenement\":\"mise-a-jour\",\"etape\":\"echec\",\"version\":\"%s\","
                            "\"raison\":\"%s\"}",
                            text, describe(failure)),
              sizeof(out));
}

}  // namespace terrarium::update
