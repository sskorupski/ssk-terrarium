// cmd/mise-a-jour and the "mise-a-jour" events that answer it
// (RG-mise-a-jour-a-distance, RG-topics-mqtt). Parsing and formatting only.
// No hardware.
#pragma once

#include <cstddef>
#include <cstdint>

#include "commands.h"
#include "firmware_version.h"

namespace terrarium::update {

// A Skaron licence is a JWT of about 350 characters: header, claims and
// Ed25519 signature, in base64url.
constexpr size_t kLicenceMaxLength = 768;
constexpr size_t kLicenceSize = kLicenceMaxLength + 1;

// base64url and dots, 1 to kLicenceMaxLength characters: safe in an HTTP
// header and on one serial line.
bool isValidLicence(const char* text, size_t length);

enum class UpdateAction : uint8_t {
  kLicence,  // {"licence":"<jeton>"}
  kCheck,    // {"action":"verifier"}
  kInstall,  // {"action":"installer","version":"x.y.z"}
};

enum class UpdateCommandError : uint8_t {
  kNone,
  kMalformed,     // Not a JSON object.
  kUnknownField,  // A field the form does not carry, or none.
  kBadAction,     // "action" is neither "verifier" nor "installer".
  kBadVersion,    // "installer" without an "x.y.z" version.
  kBadLicence,    // An empty or too long token, or one with other characters.
};

struct UpdateCommand {
  UpdateAction action;
  SemVer version;              // kInstall.
  char licence[kLicenceSize];  // kLicence. Never published nor printed.
};

// On any error, `out` is left untouched.
UpdateCommandError parseUpdatePayload(const uint8_t* payload, size_t length, UpdateCommand& out);

const char* describe(UpdateCommandError error);

// "licence <jeton>" (or "LICENCE <jeton>") on the serial link.
enum class LicenceLine : uint8_t { kNone, kValid, kInvalid };
LicenceLine parseLicenceLine(const char* line, char (&out)[kLicenceSize]);

// Answers and steps, published on event as {"evenement":"mise-a-jour",…}.
enum class LicenceOutcome : uint8_t { kApplied, kNotSaved, kRefused };

enum class Refusal : uint8_t {
  kBusy,            // "mise a jour en cours"
  kNoLicence,       // "sans licence"
  kNotOffered,      // "version non proposee"
  kLicenceRefused,  // "licence refusee": Skaron answered 401 or 403.
  kQuota,           // "quota depasse": 429.
  kUnreachable,     // "service injoignable": anything else.
  kUnknownVersion,  // "version inconnue": built without a phase tag.
};

enum class Failure : uint8_t {
  kStalled,       // "telechargement interrompu"
  kBadSignature,  // "signature invalide"
  kBadImage,      // "image invalide"
  kTooLarge,      // "image trop grande"
  kNotNewer,      // "version non plus recente"
  kOtherPhase,    // "phase differente"
};

// Steps that carry a version and nothing else.
enum class VersionStep : uint8_t {
  kAvailable,  // "disponible": the version Skaron offers.
  kUpToDate,   // "a-jour": the version in place.
  kRestart,    // "redemarrage"
  kValidated,  // "validee": the new firmware declared itself valid.
  kCancelled,  // "annulee": the version the bootloader turned down.
};

// Faults under way, as a set. None prevents an installation: each is named
// in "acceptee", and in "avertissement" if it appears during the download
// (RG-mise-a-jour-pendant-un-defaut).
using UpdateFaults = uint8_t;
constexpr UpdateFaults kAirFault = 1U << 0;           // "defaut capteur": the interior air.
constexpr UpdateFaults kSht45Fault = 1U << 1;         // "sonde sht45"
constexpr UpdateFaults kDht11Fault = 1U << 2;         // "sonde dht11"
constexpr UpdateFaults kSubstrateRejected = 1U << 3;  // "substrat rejete"
constexpr UpdateFaults kSubstrateCut = 1U << 4;       // "coupure substrat"
constexpr UpdateFaults kHeatPaused = 1U << 5;         // "pause chauffe"
constexpr UpdateFaults kAllUpdateFaults = (1U << 6) - 1;

// The name of one fault, as "defauts" lists it; nullptr for anything else.
const char* faultName(UpdateFaults fault);

const char* describe(Refusal refusal);
const char* describe(Failure failure);

// Return false if truncated. `version` is "x.y.z", with build metadata if any.
bool formatLicenceEvent(LicenceOutcome outcome, char (&out)[supervision::kEventSize]);
bool formatRefusalEvent(Refusal refusal, char (&out)[supervision::kEventSize]);
bool formatVersionEvent(VersionStep step, const SemVer& version,
                        char (&out)[supervision::kEventSize]);
// "acceptee", with "defauts" when a fault is under way.
bool formatAcceptedEvent(const SemVer& version, UpdateFaults faults,
                         char (&out)[supervision::kEventSize]);
// "avertissement": the faults under way, at least one.
bool formatWarningEvent(const SemVer& version, UpdateFaults faults,
                        char (&out)[supervision::kEventSize]);
bool formatProgressEvent(const SemVer& version, uint8_t percent,
                         char (&out)[supervision::kEventSize]);
bool formatFailureEvent(const SemVer& version, Failure failure,
                        char (&out)[supervision::kEventSize]);

}  // namespace terrarium::update
