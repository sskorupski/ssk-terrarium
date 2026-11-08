#include "posix_tz.h"

#include <cstring>

namespace terrarium::control {

namespace {

bool digit(char c) { return c >= '0' && c <= '9'; }
bool letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

// Reads an unsigned number of 1 to `maxDigits` digits, at most `max`.
bool number(const char*& p, int maxDigits, int max, int& value) {
  int digits = 0;
  value = 0;
  while (digit(*p) && digits < maxDigits) {
    value = value * 10 + (*p - '0');
    ++p;
    ++digits;
  }
  return digits > 0 && !digit(*p) && value <= max;
}

// std or dst: 3+ letters, or <3+ of [A-Za-z0-9+-]>.
bool name(const char*& p) {
  int length = 0;
  if (*p == '<') {
    ++p;
    while (letter(*p) || digit(*p) || *p == '+' || *p == '-') {
      ++p;
      ++length;
    }
    if (*p != '>' || length < 3) return false;
    ++p;
    return true;
  }
  while (letter(*p)) {
    ++p;
    ++length;
  }
  return length >= 3;
}

// [+-]hh[:mm[:ss]], hours at most `maxHours`.
bool clock(const char*& p, int maxHours) {
  if (*p == '+' || *p == '-') ++p;
  int value = 0;
  if (!number(p, 3, maxHours, value)) return false;
  for (int part = 0; part < 2 && *p == ':'; ++part) {
    ++p;
    if (!digit(p[0]) || !digit(p[1]) || digit(p[2])) return false;
    if ((p[0] - '0') * 10 + (p[1] - '0') > 59) return false;
    p += 2;
  }
  return true;
}

// ,Mm.w.d[/time]
bool rule(const char*& p) {
  if (*p != ',' || p[1] != 'M') return false;
  p += 2;
  int month = 0, week = 0, day = 0;
  if (!number(p, 2, 12, month) || month < 1 || *p++ != '.') return false;
  if (!number(p, 1, 5, week) || week < 1 || *p++ != '.') return false;
  if (!number(p, 1, 6, day)) return false;
  if (*p == '/') {
    ++p;
    if (!clock(p, 167)) return false;
  }
  return true;
}

}  // namespace

bool isValidPosixTimezone(const char* text) {
  if (text == nullptr) return false;
  if (std::strlen(text) > kTimezoneMaxLength) return false;
  const char* p = text;
  if (!name(p) || !clock(p, 24)) return false;
  if (*p == '\0') return true;
  if (!name(p)) return false;
  if (*p != ',' && *p != '\0' && !clock(p, 24)) return false;
  // A summer time needs both rules: newlib would otherwise apply US dates.
  if (!rule(p) || !rule(p)) return false;
  return *p == '\0';
}

bool parsePosixTimezone(const char* text, size_t length, char (&out)[kTimezoneSize]) {
  if (text == nullptr || length == 0 || length > kTimezoneMaxLength) return false;
  char candidate[kTimezoneSize];
  std::memcpy(candidate, text, length);
  candidate[length] = '\0';
  if (std::strlen(candidate) != length || !isValidPosixTimezone(candidate)) return false;
  std::memcpy(out, candidate, sizeof(out));
  return true;
}

}  // namespace terrarium::control
