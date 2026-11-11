#include "firmware_version.h"

#include <cstdio>
#include <cstring>

namespace terrarium::update {

namespace {

constexpr char kTagPrefix[] = "phase-";
constexpr char kDirty[] = "dirty";

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isHex(char c) { return isDigit(c) || (c >= 'a' && c <= 'f'); }

// A number without leading zero, up to 65535; advances `text` past it.
bool readNumber(const char*& text, uint16_t& out) {
  if (!isDigit(*text)) return false;
  if (*text == '0' && isDigit(text[1])) return false;
  uint32_t value = 0;
  while (isDigit(*text)) {
    value = value * 10 + static_cast<uint32_t>(*text - '0');
    if (value > 0xFFFF) return false;
    ++text;
  }
  out = static_cast<uint16_t>(value);
  return true;
}

// Appends `length` bytes of `text` to `out`, with a dot first if not empty.
bool appendBuild(char (&out)[kBuildSize], const char* text, size_t length) {
  const size_t used = std::strlen(out);
  const size_t dot = used > 0 ? 1 : 0;
  if (used + dot + length >= kBuildSize) return false;
  if (dot) out[used] = '.';
  std::memcpy(out + used + dot, text, length);
  out[used + dot + length] = '\0';
  return true;
}

// What git describe appends to the tag: "-<count>-g<hash>", then "-dirty".
bool readSuffix(const char* text, char (&build)[kBuildSize]) {
  build[0] = '\0';
  if (*text == '\0') return true;
  if (*text != '-') return false;
  ++text;
  if (std::strcmp(text, kDirty) == 0) return appendBuild(build, kDirty, sizeof(kDirty) - 1);
  const char* count = text;
  uint16_t commits = 0;
  if (!readNumber(text, commits) || *text != '-' || text[1] != 'g') return false;
  if (!appendBuild(build, count, static_cast<size_t>(text - count))) return false;
  const char* hash = text + 1;  // "g" and the hex digits.
  text += 2;
  if (!isHex(*text)) return false;
  while (isHex(*text)) ++text;
  if (!appendBuild(build, hash, static_cast<size_t>(text - hash))) return false;
  if (*text == '\0') return true;
  return text[0] == '-' && std::strcmp(text + 1, kDirty) == 0 &&
         appendBuild(build, kDirty, sizeof(kDirty) - 1);
}

int compareNumber(uint16_t a, uint16_t b) { return a < b ? -1 : (a > b ? 1 : 0); }

}  // namespace

bool fromDescribe(const char* describe, SemVer& out) {
  if (describe == nullptr) return false;
  if (std::strncmp(describe, kTagPrefix, sizeof(kTagPrefix) - 1) != 0) return false;
  const char* text = describe + sizeof(kTagPrefix) - 1;
  SemVer next{};
  if (!readNumber(text, next.major)) return false;
  if (*text == '.') {
    ++text;
    if (!readNumber(text, next.minor)) return false;
  }
  if (!readSuffix(text, next.build)) return false;
  out = next;
  return true;
}

bool parseVersion(const char* text, SemVer& out) {
  if (text == nullptr) return false;
  SemVer next{};
  if (!readNumber(text, next.major) || *text++ != '.' || !readNumber(text, next.minor) ||
      *text++ != '.' || !readNumber(text, next.patch) || *text != '\0') {
    return false;
  }
  out = next;
  return true;
}

int compare(const SemVer& a, const SemVer& b) {
  if (const int major = compareNumber(a.major, b.major)) return major;
  if (const int minor = compareNumber(a.minor, b.minor)) return minor;
  return compareNumber(a.patch, b.patch);
}

bool format(const SemVer& version, char (&out)[kVersionTextSize]) {
  const int written =
      version.build[0] == '\0'
          ? std::snprintf(out, sizeof(out), "%u.%u.%u", version.major, version.minor, version.patch)
          : std::snprintf(out, sizeof(out), "%u.%u.%u+%s", version.major, version.minor,
                          version.patch, version.build);
  return written > 0 && static_cast<size_t>(written) < sizeof(out);
}

}  // namespace terrarium::update
