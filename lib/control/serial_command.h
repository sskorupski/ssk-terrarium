// Commands typed on the USB serial link (RG-changement-de-mode-manuel). Pure
// logic: no hardware, no allocation.
#pragma once

#include "mode_machine.h"
#include "posix_tz.h"
#include "time_windows.h"
#include "ventilation.h"

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

// "VENTILATION", or "VENTILATION DUREE <minutes>", "VENTILATION PERIODE
// <minutes>", "VENTILATION HEURE <HH:MM>", with an optional trailing CR
// (RG-ventilation-periodique). kSet writes `current` with the one field
// changed; whether it is within bounds is checked by the caller.
SettingCommand parseFanCommand(const char* line, const FanSettings& current, FanSettings& out);

}  // namespace terrarium::control
