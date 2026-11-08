#include "identity.h"

#include <cstdio>
#include <cstring>

namespace terrarium::supervision {

namespace {

// 32 characters, so that a byte maps to one without bias (byte & 31).
constexpr char kPasswordAlphabet[] = "abcdefghijkmnpqrstuvwxyz23456789";
static_assert(sizeof(kPasswordAlphabet) - 1 == 32, "alphabet must have 32 characters");

}  // namespace

void formatDeviceId(const uint8_t (&mac)[6], char (&out)[kDeviceIdSize]) {
  std::snprintf(out, sizeof(out), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3],
                mac[4], mac[5]);
}

void formatApSsid(const uint8_t (&mac)[6], char (&out)[kApSsidSize]) {
  std::snprintf(out, sizeof(out), "ssk-terrarium-%02X%02X", mac[4], mac[5]);
}

void makeApPassword(const uint8_t (&random)[kApPasswordLength],
                    char (&out)[kApPasswordLength + 1]) {
  for (size_t i = 0; i < kApPasswordLength; ++i) out[i] = kPasswordAlphabet[random[i] & 31];
  out[kApPasswordLength] = '\0';
}

bool isValidApPassword(const char* password) {
  if (password == nullptr) return false;
  for (size_t i = 0; i < kApPasswordLength; ++i) {
    if (password[i] == '\0' || std::strchr(kPasswordAlphabet, password[i]) == nullptr) {
      return false;
    }
  }
  return password[kApPasswordLength] == '\0';
}

bool formatTopic(const char* deviceId, const char* suffix, char* out, size_t size) {
  const int written = std::snprintf(out, size, "ssk-terrarium/%s/%s", deviceId, suffix);
  return written > 0 && static_cast<size_t>(written) < size;
}

}  // namespace terrarium::supervision
