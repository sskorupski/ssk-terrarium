// Day and night of the regulator (RG-jour-et-nuit): a fixed day, or seasons
// between which the day slides from one to the next. Pure logic: no hardware,
// no allocation; the date and time of day are passed in.
#pragma once

#include <cstddef>
#include <cstdint>

namespace terrarium::control {

constexpr size_t kMaxSeasons = 6;
constexpr uint16_t kDaysPerYear = 365;
// "MM-JJ" and "HH:MM-HH:MM", without terminator.
constexpr size_t kMonthDayTextLength = 5;
constexpr size_t kDayWindowTextLength = 11;

// Minutes since midnight: start included, end excluded.
struct DayWindow {
  uint16_t startMinute;
  uint16_t endMinute;
};

struct Season {
  uint16_t startDay;  // Day of a 365-day year, 0 = 1 January.
  DayWindow day;
};

enum class DayNightMode : uint8_t {
  kMetronome,  // "metronome": the same day every day.
  kSeasons,    // "saisons": the day slides from one season to the next.
};

struct DayNight {
  DayNightMode mode;
  DayWindow fixed;  // kMetronome.
  uint8_t seasonCount;
  Season seasons[kMaxSeasons];  // kSeasons, kept in date order.
};

enum class Moment : uint8_t { kDay, kNight };

// A day does not cross midnight, lasts at least an hour and leaves a night of
// at least an hour.
bool isValidDayWindow(const DayWindow& window);

// kMetronome: a valid fixed day. kSeasons: 1 to 6 seasons on distinct days of
// the year, each with a valid day.
bool isValidDayNight(const DayNight& settings);

// Puts the seasons in date order.
void sortSeasons(DayNight& settings);

// Day of a 365-day year for a calendar date, month 1-12; 29 February maps to
// 28 February. Out-of-range input gives 0.
uint16_t dayOfYear(int month, int dayOfMonth);

// Parses exactly "MM-JJ" (29 February refused). Returns false and leaves
// `out` untouched on anything else.
bool parseMonthDay(const char* text, size_t length, uint16_t& out);
bool formatMonthDay(uint16_t day, char (&out)[kMonthDayTextLength + 1]);

// Parses exactly "HH:MM-HH:MM", a valid day. Returns false and leaves `out`
// untouched on anything else.
bool parseDayWindow(const char* text, size_t length, DayWindow& out);
bool formatDayWindow(const DayWindow& window, char (&out)[kDayWindowTextLength + 1]);

// The day of `day` (0-364): the fixed one, or the one slid in a straight line
// from the running season towards the next, rounded to the minute.
DayWindow dayFor(const DayNight& settings, uint16_t day);

// Day or night at `minuteOfDay` of `day`; day while the time is unknown.
Moment momentAt(const DayNight& settings, bool timeKnown, uint16_t day, uint16_t minuteOfDay);

// NVS form (RG-mode-au-demarrage): a layout version, the mode, the fixed day,
// then the seasons, little-endian. decodeDayNight returns false on another
// version or an unknown mode; the caller then checks validity.
constexpr size_t kDayNightStoredSize = 3 + 4 + 1 + kMaxSeasons * 6;
void encodeDayNight(const DayNight& settings, uint8_t (&out)[kDayNightStoredSize]);
bool decodeDayNight(const uint8_t (&in)[kDayNightStoredSize], DayNight& out);

const char* momentName(Moment moment);            // "jour", "nuit".
const char* dayNightModeName(DayNightMode mode);  // "metronome", "saisons".
bool parseDayNightMode(const char* text, DayNightMode& out);

}  // namespace terrarium::control
