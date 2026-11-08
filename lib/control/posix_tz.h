// Time zone of the PLAGES windows, written as a POSIX TZ string
// (ADR-heure-par-ntp, decision 05-fuseau). Pure logic: no hardware.
//
// The C library silently falls back to UTC on a string it cannot read, which
// would shift every window without a word: anything this parser does not
// accept is refused instead.
#pragma once

#include <cstddef>

namespace terrarium::control {

// Longest accepted string, terminator excluded.
constexpr size_t kTimezoneMaxLength = 47;
constexpr size_t kTimezoneSize = kTimezoneMaxLength + 1;

// Accepts "std offset [dst [offset] ,Mm.w.d[/time],Mm.w.d[/time]]":
// - a name of 3 letters or more, or of 3 characters or more within < >;
// - an offset [+-]hh[:mm[:ss]] of at most 24 hours;
// - a summer time is optional, but needs its two Mm.w.d rules (month 1-12,
//   week 1-5, day 0-6), each with an optional [+-]time of at most 167 hours.
// At most kTimezoneMaxLength characters.
bool isValidPosixTimezone(const char* text);

// Same, on the first `length` bytes of `text`, copied into `out` when valid.
// Returns false and leaves `out` untouched otherwise.
bool parsePosixTimezone(const char* text, size_t length, char (&out)[kTimezoneSize]);

}  // namespace terrarium::control
