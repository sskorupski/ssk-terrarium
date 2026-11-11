#include "serial_command.h"

#include <cstring>

namespace terrarium::control {

namespace {

constexpr char kModePrefix[] = "MODE ";
constexpr char kWifiPortalCommand[] = "WIFI CONFIGURER";
constexpr size_t kMaxWireNameLength = 15;  // "MAINTENANCE" and "HIBERNATION" are 11.
constexpr char kWindowsKeyword[] = "PLAGES";
constexpr char kTimezoneKeyword[] = "FUSEAU";
constexpr char kFanKeyword[] = "VENTILATION";
constexpr char kFanDuration[] = "DUREE ";
constexpr char kFanPeriod[] = "PERIODE ";
constexpr char kFanTime[] = "HEURE ";
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

// Reads 1 to 5 digits, up to 65535. Returns false on anything else.
bool parseMinutes(const char* text, uint16_t& out) {
  const size_t length = std::strlen(text);
  if (length == 0 || length > 5) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = value * 10U + static_cast<uint32_t>(text[i] - '0');
  }
  if (value > 65535U) return false;
  out = static_cast<uint16_t>(value);
  return true;
}

bool startsWith(const char* text, const char* prefix) {
  return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
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

SettingCommand parseFanCommand(const char* line, const FanSettings& current, FanSettings& out) {
  char argument[kMaxArgumentLength + 1];
  const SettingCommand command = splitCommand(line, kFanKeyword, argument);
  if (command != SettingCommand::kSet) return command;
  FanSettingsPatch patch{};
  bool parsed = false;
  if (startsWith(argument, kFanDuration)) {
    parsed = parseMinutes(argument + sizeof(kFanDuration) - 1, patch.durationMin);
    patch.hasDuration = parsed;
  } else if (startsWith(argument, kFanPeriod)) {
    parsed = parseMinutes(argument + sizeof(kFanPeriod) - 1, patch.periodMin);
    patch.hasPeriod = parsed;
  } else if (startsWith(argument, kFanTime)) {
    const char* time = argument + sizeof(kFanTime) - 1;
    parsed = parseClockTime(time, std::strlen(time), patch.dailyMinute);
    patch.hasDailyMinute = parsed;
  }
  if (!parsed) return SettingCommand::kInvalid;
  out = merged(current, patch);
  return SettingCommand::kSet;
}

}  // namespace terrarium::control
