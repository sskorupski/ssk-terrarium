#include "time_windows.h"

#include <cstring>

namespace terrarium::control {

namespace {

constexpr char kNoWindow[] = "AUCUNE";
constexpr char kNoWindowText[] = "aucune";

bool digit(char c) { return c >= '0' && c <= '9'; }

// Reads "HH:MM" at `text`, which holds at least 5 bytes.
bool parseClock(const char* text, uint16_t& minuteOfDay) {
  if (!digit(text[0]) || !digit(text[1]) || text[2] != ':' || !digit(text[3]) || !digit(text[4])) {
    return false;
  }
  const int hour = (text[0] - '0') * 10 + (text[1] - '0');
  const int minute = (text[3] - '0') * 10 + (text[4] - '0');
  if (hour > 23 || minute > 59) return false;
  minuteOfDay = static_cast<uint16_t>(hour * 60 + minute);
  return true;
}

}  // namespace

bool isValidTimeWindow(const TimeWindow& window) {
  return window.startMinute < kMinutesPerDay && window.endMinute < kMinutesPerDay &&
         window.startMinute != window.endMinute;
}

bool isValidTimeWindows(const TimeWindows& windows) {
  if (windows.count > kMaxTimeWindows) return false;
  for (uint8_t i = 0; i < windows.count; ++i) {
    if (!isValidTimeWindow(windows.windows[i])) return false;
  }
  return true;
}

bool parseTimeWindow(const char* text, size_t length, TimeWindow& out) {
  if (text == nullptr || length != kTimeWindowTextLength || text[5] != '-') return false;
  TimeWindow window{};
  if (!parseClock(text, window.startMinute) || !parseClock(text + 6, window.endMinute)) {
    return false;
  }
  if (!isValidTimeWindow(window)) return false;
  out = window;
  return true;
}

bool parseTimeWindowList(const char* text, TimeWindows& out) {
  if (text == nullptr) return false;
  if (std::strcmp(text, kNoWindow) == 0) {
    out = TimeWindows{0, {}};
    return true;
  }
  TimeWindows parsed{0, {}};
  const char* cursor = text;
  for (;;) {
    const char* space = std::strchr(cursor, ' ');
    const size_t length =
        space != nullptr ? static_cast<size_t>(space - cursor) : std::strlen(cursor);
    if (parsed.count == kMaxTimeWindows) return false;
    if (!parseTimeWindow(cursor, length, parsed.windows[parsed.count])) return false;
    ++parsed.count;
    if (space == nullptr) break;
    cursor = space + 1;
  }
  out = parsed;
  return true;
}

bool formatTimeWindow(const TimeWindow& window, char (&out)[kTimeWindowTextLength + 1]) {
  if (!isValidTimeWindow(window)) return false;
  // Written digit by digit: both ends are below 24:00, so it always fits.
  const uint16_t clocks[2] = {window.startMinute, window.endMinute};
  char* p = out;
  for (int i = 0; i < 2; ++i) {
    const unsigned hour = clocks[i] / 60U;
    const unsigned minute = clocks[i] % 60U;
    if (i == 1) *p++ = '-';
    *p++ = static_cast<char>('0' + hour / 10U);
    *p++ = static_cast<char>('0' + hour % 10U);
    *p++ = ':';
    *p++ = static_cast<char>('0' + minute / 10U);
    *p++ = static_cast<char>('0' + minute % 10U);
  }
  *p = '\0';
  return true;
}

bool formatTimeWindows(const TimeWindows& windows, char (&out)[kTimeWindowsTextSize]) {
  if (!isValidTimeWindows(windows)) return false;
  if (windows.count == 0) {
    std::memcpy(out, kNoWindowText, sizeof(kNoWindowText));
    return true;
  }
  size_t used = 0;
  for (uint8_t i = 0; i < windows.count; ++i) {
    char one[kTimeWindowTextLength + 1];
    formatTimeWindow(windows.windows[i], one);
    if (i > 0) out[used++] = ' ';
    std::memcpy(out + used, one, kTimeWindowTextLength);
    used += kTimeWindowTextLength;
  }
  out[used] = '\0';
  return true;
}

bool inAnyWindow(const TimeWindows& windows, uint16_t minuteOfDay) {
  const uint8_t count = windows.count <= kMaxTimeWindows ? windows.count : 0;
  for (uint8_t i = 0; i < count; ++i) {
    const TimeWindow& w = windows.windows[i];
    const bool inside = w.startMinute < w.endMinute
                            ? minuteOfDay >= w.startMinute && minuteOfDay < w.endMinute
                            : minuteOfDay >= w.startMinute || minuteOfDay < w.endMinute;
    if (inside) return true;
  }
  return false;
}

ScheduledHeating scheduledHeating(bool timeKnown, const TimeWindows& windows,
                                  uint16_t minuteOfDay) {
  if (!timeKnown || inAnyWindow(windows, minuteOfDay)) return ScheduledHeating::kSetpoint;
  return ScheduledHeating::kFloor;
}

}  // namespace terrarium::control
