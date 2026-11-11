// The firmware's version in the semver sense, as Skaron compares it
// (RG-mise-a-jour-a-distance). Pure logic: no hardware, no allocation.
#pragma once

#include <cstddef>
#include <cstdint>

namespace terrarium::update {

constexpr size_t kBuildSize = 48;  // "<commits>.g<hash>.dirty", with room to spare.
// "65535.65535.65535", "+" and the build metadata.
constexpr size_t kVersionTextSize = 18 + kBuildSize;

struct SemVer {
  uint16_t major;  // The phase: phase-N is N.0.0.
  uint16_t minor;
  uint16_t patch;
  char build[kBuildSize];  // Empty on a tagged build.
};

// From `git describe --tags --always --dirty`: "phase-N" gives N.0.0,
// "phase-N.M" gives N.M.0, and a build between two tags carries the commit
// count and hash as build metadata ("phase-1.2-3-g1a2b3c4" gives
// 1.2.0+3.g1a2b3c4, with ".dirty" for uncommitted changes). Returns false,
// leaving `out` untouched, on anything else ("inconnue", a bare hash).
bool fromDescribe(const char* describe, SemVer& out);

// "x.y.z" exactly, as a command or Skaron names a version: digits only, no
// leading zero, no pre-release, no build metadata. Returns false, leaving
// `out` untouched, on anything else.
bool parseVersion(const char* text, SemVer& out);

// Semver precedence: build metadata is ignored, so a build between two tags
// ranks as its tag. Negative, zero or positive, like strcmp.
int compare(const SemVer& a, const SemVer& b);

// "x.y.z", then "+build" if any. Returns false if it does not fit.
bool format(const SemVer& version, char (&out)[kVersionTextSize]);

}  // namespace terrarium::update
