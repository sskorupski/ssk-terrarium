// The decisions of a remote update, apart from the network and the flash:
// which command is refused and why, what Skaron's answer means, whether the
// signed release is the one asked for, how the download progresses, and when
// a new firmware is valid (RG-mise-a-jour-refusee-pendant-un-defaut,
// RG-image-signee, RG-regulation-pendant-la-mise-a-jour,
// RG-retour-arriere-automatique). No hardware, no allocation.
#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>

#include "firmware_version.h"
#include "minisign.h"
#include "update_command.h"

namespace terrarium::update {

// Product and target in Skaron: ssk-terrarium, esp32-phase-<N>.
constexpr char kProduct[] = "ssk-terrarium";
constexpr char kArch[] = "esp32";

// What the board knows when a command arrives.
struct Situation {
  bool busy;          // Another check or installation runs.
  bool fault;         // A fault is under way (RG-mise-a-jour-refusee-pendant-un-defaut).
  bool hasLicence;    // A Skaron licence is held.
  bool versionKnown;  // The firmware was built from a phase tag.
};

// verifier: refused while busy, without licence or without a known version.
// A fault does not prevent a check. Returns false and sets `out` if refused.
bool admitCheck(const Situation& situation, Refusal& out);

// installer: refused also during a fault, and for a version that is not newer
// than `current` (Skaron never offers one). Returns false and sets `out`.
bool admitInstall(const Situation& situation, const SemVer& current, const SemVer& requested,
                  Refusal& out);

// "/v1/ssk-terrarium/esp32/phase-<N>/<version>". Returns false if it does
// not fit.
bool formatSkaronPath(const SemVer& current, char* out, size_t size);

constexpr size_t kUrlSize = 2048;  // A presigned URL is about 700 characters.

// What Skaron offers: the version, the presigned URL of the image, valid
// fifteen minutes, and the minisign signature.
struct Offer {
  SemVer version;
  char url[kUrlSize];
  char signature[kSignatureTextSize];
};

enum class Answer : uint8_t {
  kOffer,     // 200 and a valid offer.
  kUpToDate,  // 204: nothing newer in the licence's channel.
  kRefused,   // `refusal` says why.
};

// The fields of Skaron's answer worth reading: the release notes may be long.
void offerFilter(JsonDocument& filter);

// Skaron's HTTP status, and for a 200 its body filtered by offerFilter.
// A 200 whose body is not a valid offer (no "x.y.z", no https URL, no
// signature) counts as an unreachable service.
Answer readAnswer(int status, JsonVariantConst body, Offer& offer, Refusal& refusal);

// The trusted comment the owner signs: "ssk-terrarium esp32-phase-<N> x.y.z".
// Returns false if it does not fit.
bool formatTrustedComment(const SemVer& version, char* out, size_t size);

// Whether the signed release is the one asked for: kNone, or the failure.
// The comment must name this product, this firmware's phase, and the
// requested version, which must be newer than the one in place: no compromised
// Skaron can replay an older signed image, nor one of another phase.
enum class ReleaseCheck : uint8_t { kOk, kOtherPhase, kNotNewer, kNotRequested };
ReleaseCheck checkRelease(const char* trustedComment, const SemVer& current,
                          const SemVer& requested);

// Progress of the download into the inactive partition.
class DownloadProgress {
 public:
  explicit constexpr DownloadProgress(uint32_t stallMs) : stallMs_(stallMs) {}

  // `contentLength` from the HTTP answer, -1 if unknown. Returns false and
  // sets `out` if the image cannot fit (kTooLarge) or has no size (kBadImage).
  bool start(int64_t contentLength, size_t partitionBytes, uint32_t nowMs, Failure& out);

  // Counts `bytes` received at `nowMs`. Returns the 10 % step just crossed
  // (10, 20 … 100), or 0.
  uint8_t advance(size_t bytes, uint32_t nowMs);

  // No byte received for stallMs (RG-regulation-pendant-la-mise-a-jour).
  bool stalled(uint32_t nowMs) const { return nowMs - lastProgressMs_ >= stallMs_; }
  bool complete() const { return received_ >= total_; }
  size_t remaining() const { return complete() ? 0 : total_ - received_; }

 private:
  uint32_t stallMs_;
  size_t total_ = 0;
  size_t received_ = 0;
  uint8_t lastStep_ = 0;
  uint32_t lastProgressMs_ = 0;
};

// A new firmware is valid once it has retained an air measure and joined the
// broker, both within the deadline after boot (RG-retour-arriere-automatique).
enum class Validation : uint8_t { kPending, kValid, kExpired };

class ValidationWatch {
 public:
  explicit constexpr ValidationWatch(uint32_t deadlineMs) : deadlineMs_(deadlineMs) {}

  // `nowMs` since boot. Each condition, once met, stays met.
  Validation update(uint32_t nowMs, bool airRetained, bool brokerJoined);

 private:
  uint32_t deadlineMs_;
  bool airRetained_ = false;
  bool brokerJoined_ = false;
  bool expired_ = false;
};

// The version noted in NVS before restarting into a new firmware, read back
// at boot by a firmware that is not under verification.
enum class AttemptNote : uint8_t {
  kCancelled,  // Another version was tried and turned down: publish "annulee".
  kStale,      // This very version, validated, or unreadable: just clear it.
};
AttemptNote readAttemptNote(const char* note, bool runningKnown, const SemVer& running,
                            SemVer& attempted);

}  // namespace terrarium::update
