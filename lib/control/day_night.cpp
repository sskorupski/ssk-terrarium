#include "day_night.h"

#include <cstring>

#include "ventilation.h"

namespace terrarium::control {

namespace {

constexpr uint16_t kMinutesInDay = 24 * 60;
constexpr uint16_t kMinDayMinutes = 60;
constexpr uint16_t kMinNightMinutes = 60;
constexpr uint8_t kDaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

bool digit(char c) { return c >= '0' && c <= '9'; }

// Rounds num / den to the nearest integer, halves away from zero; den > 0.
int32_t roundedDivision(int32_t num, int32_t den) {
  return num >= 0 ? (2 * num + den) / (2 * den) : -((-2 * num + den) / (2 * den));
}

uint16_t slide(uint16_t from, uint16_t to, uint16_t elapsed, uint16_t span) {
  const int32_t delta = static_cast<int32_t>(to) - static_cast<int32_t>(from);
  return static_cast<uint16_t>(static_cast<int32_t>(from) + roundedDivision(delta * elapsed, span));
}

// Days from `from` to `to`, forward, looping over the year.
uint16_t daysBetween(uint16_t from, uint16_t to) {
  return static_cast<uint16_t>((to + kDaysPerYear - from) % kDaysPerYear);
}

}  // namespace

bool isValidDayWindow(const DayWindow& window) {
  if (window.endMinute >= kMinutesInDay || window.startMinute >= window.endMinute) return false;
  const uint16_t length = window.endMinute - window.startMinute;
  return length >= kMinDayMinutes && kMinutesInDay - length >= kMinNightMinutes;
}

bool isValidDayNight(const DayNight& settings) {
  if (settings.mode == DayNightMode::kMetronome) return isValidDayWindow(settings.fixed);
  if (settings.mode != DayNightMode::kSeasons) return false;
  if (settings.seasonCount == 0 || settings.seasonCount > kMaxSeasons) return false;
  for (uint8_t i = 0; i < settings.seasonCount; ++i) {
    if (settings.seasons[i].startDay >= kDaysPerYear ||
        !isValidDayWindow(settings.seasons[i].day)) {
      return false;
    }
    for (uint8_t j = 0; j < i; ++j) {
      if (settings.seasons[j].startDay == settings.seasons[i].startDay) return false;
    }
  }
  return true;
}

void sortSeasons(DayNight& settings) {
  const uint8_t count = settings.seasonCount <= kMaxSeasons ? settings.seasonCount : kMaxSeasons;
  for (uint8_t i = 1; i < count; ++i) {
    const Season moving = settings.seasons[i];
    uint8_t j = i;
    while (j > 0 && settings.seasons[j - 1].startDay > moving.startDay) {
      settings.seasons[j] = settings.seasons[j - 1];
      --j;
    }
    settings.seasons[j] = moving;
  }
}

uint16_t dayOfYear(int month, int dayOfMonth) {
  if (month < 1 || month > 12 || dayOfMonth < 1) return 0;
  uint16_t day = 0;
  for (int m = 1; m < month; ++m) day += kDaysInMonth[m - 1];
  const int capped = dayOfMonth > kDaysInMonth[month - 1] ? kDaysInMonth[month - 1] : dayOfMonth;
  return static_cast<uint16_t>(day + capped - 1);
}

bool parseMonthDay(const char* text, size_t length, uint16_t& out) {
  if (text == nullptr || length != kMonthDayTextLength) return false;
  if (!digit(text[0]) || !digit(text[1]) || text[2] != '-' || !digit(text[3]) || !digit(text[4])) {
    return false;
  }
  const int month = (text[0] - '0') * 10 + (text[1] - '0');
  const int dayOfMonth = (text[3] - '0') * 10 + (text[4] - '0');
  if (month < 1 || month > 12 || dayOfMonth < 1 || dayOfMonth > kDaysInMonth[month - 1]) {
    return false;
  }
  out = dayOfYear(month, dayOfMonth);
  return true;
}

bool formatMonthDay(uint16_t day, char (&out)[kMonthDayTextLength + 1]) {
  if (day >= kDaysPerYear) return false;
  int month = 0;
  while (day >= kDaysInMonth[month]) day -= kDaysInMonth[month++];
  const int m = month + 1;
  const int d = day + 1;
  out[0] = static_cast<char>('0' + m / 10);
  out[1] = static_cast<char>('0' + m % 10);
  out[2] = '-';
  out[3] = static_cast<char>('0' + d / 10);
  out[4] = static_cast<char>('0' + d % 10);
  out[5] = '\0';
  return true;
}

bool parseDayWindow(const char* text, size_t length, DayWindow& out) {
  if (text == nullptr || length != kDayWindowTextLength || text[5] != '-') return false;
  DayWindow window{};
  if (!parseClockTime(text, kClockTimeTextLength, window.startMinute) ||
      !parseClockTime(text + 6, kClockTimeTextLength, window.endMinute) ||
      !isValidDayWindow(window)) {
    return false;
  }
  out = window;
  return true;
}

bool formatDayWindow(const DayWindow& window, char (&out)[kDayWindowTextLength + 1]) {
  char start[kClockTimeTextLength + 1];
  char end[kClockTimeTextLength + 1];
  if (!formatClockTime(window.startMinute, start) || !formatClockTime(window.endMinute, end)) {
    return false;
  }
  std::memcpy(out, start, kClockTimeTextLength);
  out[kClockTimeTextLength] = '-';
  std::memcpy(out + kClockTimeTextLength + 1, end, kClockTimeTextLength + 1);
  return true;
}

DayWindow dayFor(const DayNight& settings, uint16_t day) {
  if (settings.mode != DayNightMode::kSeasons || settings.seasonCount == 0) return settings.fixed;
  const uint8_t count = settings.seasonCount <= kMaxSeasons ? settings.seasonCount : kMaxSeasons;
  if (count == 1) return settings.seasons[0].day;
  // The running season: the last one started on or before `day`, or the last
  // of the year when `day` comes before the first.
  uint8_t running = count - 1;
  for (uint8_t i = 0; i < count; ++i) {
    if (settings.seasons[i].startDay <= day) running = i;
  }
  const Season& from = settings.seasons[running];
  const Season& to = settings.seasons[(running + 1) % count];
  const uint16_t span = daysBetween(from.startDay, to.startDay);
  const uint16_t elapsed = daysBetween(from.startDay, day % kDaysPerYear);
  return {slide(from.day.startMinute, to.day.startMinute, elapsed, span),
          slide(from.day.endMinute, to.day.endMinute, elapsed, span)};
}

Moment momentAt(const DayNight& settings, bool timeKnown, uint16_t day, uint16_t minuteOfDay) {
  if (!timeKnown) return Moment::kDay;
  const DayWindow window = dayFor(settings, day);
  return minuteOfDay >= window.startMinute && minuteOfDay < window.endMinute ? Moment::kDay
                                                                             : Moment::kNight;
}

namespace {

constexpr uint8_t kDayNightLayout = 1;

void put16(uint8_t*& p, uint16_t v) {
  *p++ = static_cast<uint8_t>(v & 0xFF);
  *p++ = static_cast<uint8_t>(v >> 8);
}

uint16_t get16(const uint8_t*& p) {
  const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
  p += 2;
  return v;
}

}  // namespace

void encodeDayNight(const DayNight& s, uint8_t (&out)[kDayNightStoredSize]) {
  uint8_t* p = out;
  *p++ = kDayNightLayout;
  *p++ = static_cast<uint8_t>(s.mode);
  *p++ = 0;  // Reserved.
  put16(p, s.fixed.startMinute);
  put16(p, s.fixed.endMinute);
  const uint8_t count = s.seasonCount <= kMaxSeasons ? s.seasonCount : kMaxSeasons;
  *p++ = count;
  for (size_t i = 0; i < kMaxSeasons; ++i) {
    const Season season = i < count ? s.seasons[i] : Season{0, {0, 0}};
    put16(p, season.startDay);
    put16(p, season.day.startMinute);
    put16(p, season.day.endMinute);
  }
}

bool decodeDayNight(const uint8_t (&in)[kDayNightStoredSize], DayNight& out) {
  const uint8_t* p = in;
  if (*p++ != kDayNightLayout) return false;
  const uint8_t mode = *p++;
  ++p;
  if (mode > static_cast<uint8_t>(DayNightMode::kSeasons)) return false;
  DayNight s{};
  s.mode = static_cast<DayNightMode>(mode);
  s.fixed.startMinute = get16(p);
  s.fixed.endMinute = get16(p);
  s.seasonCount = *p++;
  if (s.seasonCount > kMaxSeasons) return false;
  for (size_t i = 0; i < kMaxSeasons; ++i) {
    s.seasons[i].startDay = get16(p);
    s.seasons[i].day.startMinute = get16(p);
    s.seasons[i].day.endMinute = get16(p);
  }
  out = s;
  return true;
}

const char* momentName(Moment moment) { return moment == Moment::kNight ? "nuit" : "jour"; }

const char* dayNightModeName(DayNightMode mode) {
  return mode == DayNightMode::kSeasons ? "saisons" : "metronome";
}

bool parseDayNightMode(const char* text, DayNightMode& out) {
  if (text == nullptr) return false;
  if (std::strcmp(text, "metronome") == 0) {
    out = DayNightMode::kMetronome;
    return true;
  }
  if (std::strcmp(text, "saisons") == 0) {
    out = DayNightMode::kSeasons;
    return true;
  }
  return false;
}

}  // namespace terrarium::control
