// Daily time windows of the PLAGES mode (RG-chauffage-par-plages). Pure logic:
// no hardware, no allocation; the time of day is passed in.
#pragma once

#include <cstddef>
#include <cstdint>

namespace terrarium::control {

constexpr size_t kMaxTimeWindows = 4;
constexpr uint16_t kMinutesPerDay = 24 * 60;
// "HH:MM-HH:MM", without terminator.
constexpr size_t kTimeWindowTextLength = 11;
// Up to four windows separated by spaces, or "aucune", plus the terminator.
constexpr size_t kTimeWindowsTextSize = kMaxTimeWindows * (kTimeWindowTextLength + 1);

// Minutes since midnight. The start is included, the end excluded; a window
// whose end comes before its start runs past midnight.
struct TimeWindow {
  uint16_t startMinute;
  uint16_t endMinute;
};

struct TimeWindows {
  uint8_t count;
  TimeWindow windows[kMaxTimeWindows];
};

// Whether a window is well formed: both ends within the day, start != end.
bool isValidTimeWindow(const TimeWindow& window);

// Whether a set is well formed: at most four windows, each valid. Used on
// what NVS gives back (RG-mode-au-demarrage).
bool isValidTimeWindows(const TimeWindows& windows);

// Parses exactly "HH:MM-HH:MM" from the first `length` bytes of `text`.
// Returns false and leaves `out` untouched on anything else.
bool parseTimeWindow(const char* text, size_t length, TimeWindow& out);

// Parses a serial argument: up to four windows separated by single spaces, or
// "AUCUNE" for none. Returns false and leaves `out` untouched on anything else.
bool parseTimeWindowList(const char* text, TimeWindows& out);

// Writes "01:35-06:30 14:05-17:05", or "aucune". Returns false if `windows`
// is not valid.
bool formatTimeWindows(const TimeWindows& windows, char (&out)[kTimeWindowsTextSize]);

// Writes one window as "HH:MM-HH:MM". Returns false if it is not valid.
bool formatTimeWindow(const TimeWindow& window, char (&out)[kTimeWindowTextLength + 1]);

// Whether `minuteOfDay` falls in any of the windows.
bool inAnyWindow(const TimeWindows& windows, uint16_t minuteOfDay);

// What PLAGES heats to (RG-chauffage-par-plages).
enum class ScheduledHeating : uint8_t {
  kSetpoint,  // In a window, or time unknown: like ACTIF.
  kFloor,     // Outside every window: only below the 12 C floor.
};

// Heats to the setpoint in a window, and whenever the time is not known yet:
// an unknown time never lowers heating to the floor (decision 01-sans-heure).
ScheduledHeating scheduledHeating(bool timeKnown, const TimeWindows& windows, uint16_t minuteOfDay);

}  // namespace terrarium::control
