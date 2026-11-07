#include "serial_command.h"

#include <cstring>

namespace terrarium::control {

namespace {

constexpr char kModePrefix[] = "MODE ";
constexpr size_t kMaxWireNameLength = 15;  // "MAINTENANCE" and "HIBERNATION" are 11.

}  // namespace

bool parseModeCommand(const char* line, Mode& out) {
  if (line == nullptr) return false;
  const size_t prefixLength = sizeof(kModePrefix) - 1;
  if (std::strncmp(line, kModePrefix, prefixLength) != 0) return false;

  const char* name = line + prefixLength;
  size_t length = std::strlen(name);
  if (length > 0 && name[length - 1] == '\r') --length;
  if (length == 0 || length > kMaxWireNameLength) return false;

  char wireName[kMaxWireNameLength + 1];
  std::memcpy(wireName, name, length);
  wireName[length] = '\0';
  return parseMode(wireName, out);
}

}  // namespace terrarium::control
