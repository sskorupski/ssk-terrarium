#include "serial_command.h"

#include <cstring>

namespace terrarium::control {

namespace {

constexpr char kModePrefix[] = "MODE ";
constexpr char kWifiPortalCommand[] = "WIFI CONFIGURER";
constexpr size_t kMaxWireNameLength = 15;  // "MAINTENANCE" and "HIBERNATION" are 11.
constexpr char kWindowsKeyword[] = "PLAGES";
constexpr char kTimezoneKeyword[] = "FUSEAU";
// Longest argument: four windows, or a time zone.
constexpr size_t kMaxArgumentLength = kTimeWindowsTextSize;

// Splits "<keyword>[ <argument>][\r]". Returns kNone if `line` is not this
// keyword, kShow for the keyword alone, kSet with the argument copied into
// `argument`, or kInvalid for an empty or over-long argument.
SettingCommand splitCommand(const char* line, const char* keyword,
                            char (&argument)[kMaxArgumentLength + 1]) {
  if (line == nullptr) return SettingCommand::kNone;
  size_t length = std::strlen(line);
  if (length > 0 && line[length - 1] == '\r') --length;
  const size_t keywordLength = std::strlen(keyword);
  if (length < keywordLength || std::strncmp(line, keyword, keywordLength) != 0) {
    return SettingCommand::kNone;
  }
  if (length == keywordLength) return SettingCommand::kShow;
  if (line[keywordLength] != ' ') return SettingCommand::kNone;
  const size_t argumentLength = length - keywordLength - 1;
  if (argumentLength == 0 || argumentLength > kMaxArgumentLength) return SettingCommand::kInvalid;
  std::memcpy(argument, line + keywordLength + 1, argumentLength);
  argument[argumentLength] = '\0';
  return SettingCommand::kSet;
}

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

bool isWifiPortalCommand(const char* line) {
  if (line == nullptr) return false;
  size_t length = std::strlen(line);
  if (length > 0 && line[length - 1] == '\r') --length;
  return length == sizeof(kWifiPortalCommand) - 1 &&
         std::strncmp(line, kWifiPortalCommand, length) == 0;
}

SettingCommand parseWindowsCommand(const char* line, TimeWindows& out) {
  char argument[kMaxArgumentLength + 1];
  const SettingCommand command = splitCommand(line, kWindowsKeyword, argument);
  if (command != SettingCommand::kSet) return command;
  return parseTimeWindowList(argument, out) ? SettingCommand::kSet : SettingCommand::kInvalid;
}

SettingCommand parseTimezoneCommand(const char* line, char (&out)[kTimezoneSize]) {
  char argument[kMaxArgumentLength + 1];
  const SettingCommand command = splitCommand(line, kTimezoneKeyword, argument);
  if (command != SettingCommand::kSet) return command;
  return parsePosixTimezone(argument, std::strlen(argument), out) ? SettingCommand::kSet
                                                                  : SettingCommand::kInvalid;
}

}  // namespace terrarium::control
