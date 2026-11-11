#include "install_flow.h"

#include <cstdio>
#include <cstring>

namespace terrarium::update {

namespace {

constexpr char kVersionField[] = "version";
constexpr char kUrlField[] = "url";
constexpr char kSignatureField[] = "signature";
constexpr char kHttps[] = "https://";
constexpr char kTargetPrefix[] = "-phase-";  // After kArch: "esp32-phase-<N>".

constexpr int kHttpOk = 200;
constexpr int kHttpNoContent = 204;
constexpr int kHttpUnauthorized = 401;
constexpr int kHttpForbidden = 403;
constexpr int kHttpTooManyRequests = 429;

bool fits(int written, size_t size) { return written > 0 && static_cast<size_t>(written) < size; }

// Copies a string field of at most size - 1 characters.
bool copyField(JsonVariantConst value, char* out, size_t size) {
  if (!value.is<const char*>()) return false;
  const char* text = value.as<const char*>();
  const size_t length = std::strlen(text);
  if (length == 0 || length >= size) return false;
  std::memcpy(out, text, length + 1);
  return true;
}

// "esp32-phase-<N> " at `text`; advances past the space.
bool readTarget(const char*& text, uint16_t& phase) {
  const size_t archLength = std::strlen(kArch);
  if (std::strncmp(text, kArch, archLength) != 0) return false;
  text += archLength;
  if (std::strncmp(text, kTargetPrefix, sizeof(kTargetPrefix) - 1) != 0) return false;
  text += sizeof(kTargetPrefix) - 1;
  // The phase reads as a version's major part: digits, no leading zero.
  char number[8];
  size_t length = 0;
  while (text[length] >= '0' && text[length] <= '9' && length + 1 < sizeof(number)) {
    number[length] = text[length];
    ++length;
  }
  if (length == 0 || text[length] != ' ') return false;
  number[length] = '\0';
  char asVersion[sizeof(number) + 4];
  std::snprintf(asVersion, sizeof(asVersion), "%s.0.0", number);
  SemVer parsed{};
  if (!parseVersion(asVersion, parsed)) return false;
  phase = parsed.major;
  text += length + 1;
  return true;
}

}  // namespace

bool admitCheck(const Situation& situation, Refusal& out) {
  if (situation.busy) {
    out = Refusal::kBusy;
  } else if (!situation.versionKnown) {
    out = Refusal::kUnknownVersion;
  } else if (!situation.hasLicence) {
    out = Refusal::kNoLicence;
  } else {
    return true;
  }
  return false;
}

bool admitInstall(const Situation& situation, const SemVer& current, const SemVer& requested,
                  Refusal& out) {
  if (!admitCheck(situation, out)) {
    return false;
  } else if (compare(requested, current) <= 0) {
    out = Refusal::kNotOffered;
  } else {
    return true;
  }
  return false;
}

bool formatSkaronPath(const SemVer& current, char* out, size_t size) {
  char version[kVersionTextSize];
  return format(current, version) && fits(std::snprintf(out, size, "/v1/%s/%s/phase-%u/%s",
                                                        kProduct, kArch, current.major, version),
                                          size);
}

void offerFilter(JsonDocument& filter) {
  filter[kVersionField] = true;
  filter[kUrlField] = true;
  filter[kSignatureField] = true;
}

Answer readAnswer(int status, JsonVariantConst body, Offer& offer, Refusal& refusal) {
  if (status == kHttpNoContent) return Answer::kUpToDate;
  if (status == kHttpUnauthorized || status == kHttpForbidden) {
    refusal = Refusal::kLicenceRefused;
    return Answer::kRefused;
  }
  if (status == kHttpTooManyRequests) {
    refusal = Refusal::kQuota;
    return Answer::kRefused;
  }
  refusal = Refusal::kUnreachable;
  if (status != kHttpOk) return Answer::kRefused;
  const JsonVariantConst version = body[kVersionField];
  if (!version.is<const char*>() || !parseVersion(version.as<const char*>(), offer.version) ||
      !copyField(body[kUrlField], offer.url, sizeof(offer.url)) ||
      std::strncmp(offer.url, kHttps, sizeof(kHttps) - 1) != 0 ||
      std::strlen(offer.url) == sizeof(kHttps) - 1 ||
      !copyField(body[kSignatureField], offer.signature, sizeof(offer.signature))) {
    return Answer::kRefused;
  }
  return Answer::kOffer;
}

bool formatTrustedComment(const SemVer& version, char* out, size_t size) {
  char text[kVersionTextSize];
  return format(version, text) && fits(std::snprintf(out, size, "%s %s%s%u %s", kProduct, kArch,
                                                     kTargetPrefix, version.major, text),
                                       size);
}

ReleaseCheck checkRelease(const char* trustedComment, const SemVer& current,
                          const SemVer& requested) {
  if (trustedComment == nullptr) return ReleaseCheck::kNotRequested;
  const size_t productLength = std::strlen(kProduct);
  const char* text = trustedComment;
  if (std::strncmp(text, kProduct, productLength) != 0 || text[productLength] != ' ') {
    return ReleaseCheck::kNotRequested;
  }
  text += productLength + 1;
  uint16_t phase = 0;
  SemVer signedVersion{};
  if (!readTarget(text, phase) || !parseVersion(text, signedVersion)) {
    return ReleaseCheck::kNotRequested;
  }
  if (phase != current.major || signedVersion.major != phase) return ReleaseCheck::kOtherPhase;
  if (compare(signedVersion, current) <= 0) return ReleaseCheck::kNotNewer;
  if (compare(signedVersion, requested) != 0) return ReleaseCheck::kNotRequested;
  return ReleaseCheck::kOk;
}

bool DownloadProgress::start(int64_t contentLength, size_t partitionBytes, uint32_t nowMs,
                             Failure& out) {
  if (contentLength <= 0) {
    out = Failure::kBadImage;
    return false;
  }
  if (static_cast<uint64_t>(contentLength) > partitionBytes) {
    out = Failure::kTooLarge;
    return false;
  }
  total_ = static_cast<size_t>(contentLength);
  received_ = 0;
  lastStep_ = 0;
  lastProgressMs_ = nowMs;
  return true;
}

uint8_t DownloadProgress::advance(size_t bytes, uint32_t nowMs) {
  if (bytes == 0) return 0;
  lastProgressMs_ = nowMs;
  received_ = bytes >= remaining() ? total_ : received_ + bytes;
  const uint64_t percent = static_cast<uint64_t>(received_) * 100U / total_;
  const uint8_t step = static_cast<uint8_t>(percent / 10U * 10U);
  if (step <= lastStep_) return 0;
  lastStep_ = step;
  return step;
}

Validation ValidationWatch::update(uint32_t nowMs, bool airRetained, bool brokerJoined) {
  if (expired_ || (!(airRetained_ && brokerJoined_) && nowMs >= deadlineMs_)) {
    expired_ = true;
    return Validation::kExpired;
  }
  airRetained_ = airRetained_ || airRetained;
  brokerJoined_ = brokerJoined_ || brokerJoined;
  return airRetained_ && brokerJoined_ ? Validation::kValid : Validation::kPending;
}

AttemptNote readAttemptNote(const char* note, bool runningKnown, const SemVer& running,
                            SemVer& attempted) {
  SemVer noted{};
  if (!parseVersion(note, noted)) return AttemptNote::kStale;
  if (runningKnown && compare(noted, running) == 0) return AttemptNote::kStale;
  attempted = noted;
  return AttemptNote::kCancelled;
}

}  // namespace terrarium::update
