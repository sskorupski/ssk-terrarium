// Commands typed on the USB serial link (RG-changement-de-mode-manuel). Pure
// logic: no hardware, no allocation.
#pragma once

#include <cstddef>

#include "mode_machine.h"
#include "posix_tz.h"
#include "time_windows.h"

namespace terrarium::control {

// Parses "MODE <wire name>", e.g. "MODE ACTIF", with an optional trailing CR.
// Returns false and leaves `out` untouched on anything else, including nullptr.
bool parseModeCommand(const char* line, Mode& out);

// Whether `line` is "WIFI CONFIGURER", with an optional trailing CR: reopens
// the configuration portal (RG-configuration-wifi-sur-place).
bool isWifiPortalCommand(const char* line);

// A command that shows or replaces a setting.
enum class SettingCommand : uint8_t {
  kNone,     // Not this command.
  kShow,     // The keyword alone: print the current value.
  kSet,      // A valid new value, written to `out`.
  kInvalid,  // The keyword with a value that is refused; `out` untouched.
};

// "PLAGES", "PLAGES AUCUNE" or "PLAGES 01:35-06:30 14:05-17:05", with an
// optional trailing CR (RG-chauffage-par-plages).
SettingCommand parseWindowsCommand(const char* line, TimeWindows& out);

// "FUSEAU" or "FUSEAU <POSIX TZ>", with an optional trailing CR
// (ADR-heure-par-ntp).
SettingCommand parseTimezoneCommand(const char* line, char (&out)[kTimezoneSize]);

// "CONFIG <json>", with an optional trailing CR: the same settings as
// cmd/config (decision 05 of cadrage 2026-015). Returns the JSON and its
// length, or nullptr if `line` is not this command or has no JSON.
const char* configPayload(const char* line, size_t& length);

// Whether `line` is `keyword` alone, with an optional trailing CR: "JOUR" and
// "VENTILATION" show their settings.
bool isShowCommand(const char* line, const char* keyword);

}  // namespace terrarium::control
