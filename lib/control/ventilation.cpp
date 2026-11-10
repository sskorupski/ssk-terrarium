#include "ventilation.h"

namespace terrarium::control {

namespace {

constexpr uint32_t kMinuteMs = 60UL * 1000UL;
constexpr uint16_t kMinutesPerDay = 24 * 60;

bool digit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

bool isAcceptedFanSettings(const FanSettings& settings, const FanSettingsBounds& bounds) {
  const bool durationOk = settings.durationMin >= bounds.durationMinMin &&
                          settings.durationMin <= bounds.durationMaxMin;
  const bool periodOk =
      settings.periodMin >= bounds.periodMinMin && settings.periodMin <= bounds.periodMaxMin;
  return durationOk && periodOk && settings.durationMin < settings.periodMin &&
         settings.dailyMinute < kMinutesPerDay;
}

FanSettings merged(const FanSettings& current, const FanSettingsPatch& patch) {
  FanSettings next = current;
  if (patch.hasDuration) next.durationMin = patch.durationMin;
  if (patch.hasPeriod) next.periodMin = patch.periodMin;
  if (patch.hasDailyMinute) next.dailyMinute = patch.dailyMinute;
  return next;
}

bool parseClockTime(const char* text, size_t length, uint16_t& out) {
  if (text == nullptr || length != kClockTimeTextLength) return false;
  if (!digit(text[0]) || !digit(text[1]) || text[2] != ':' || !digit(text[3]) || !digit(text[4])) {
    return false;
  }
  const int hour = (text[0] - '0') * 10 + (text[1] - '0');
  const int minute = (text[3] - '0') * 10 + (text[4] - '0');
  if (hour > 23 || minute > 59) return false;
  out = static_cast<uint16_t>(hour * 60 + minute);
  return true;
}

bool formatClockTime(uint16_t minuteOfDay, char (&out)[kClockTimeTextLength + 1]) {
  if (minuteOfDay >= kMinutesPerDay) return false;
  const unsigned hour = minuteOfDay / 60U;
  const unsigned minute = minuteOfDay % 60U;
  out[0] = static_cast<char>('0' + hour / 10U);
  out[1] = static_cast<char>('0' + hour % 10U);
  out[2] = ':';
  out[3] = static_cast<char>('0' + minute / 10U);
  out[4] = static_cast<char>('0' + minute % 10U);
  out[5] = '\0';
  return true;
}

void FanScheduler::restart(uint32_t nowMs) {
  restartMs_ = nowMs;
  periodStartMs_ = nowMs;
  aired_ = false;
}

bool FanScheduler::update(uint32_t nowMs, FanPolicy policy, const FanSettings& settings,
                          bool timeKnown, uint16_t minuteOfDay) {
  switch (policy) {
    case FanPolicy::kOff:
      return false;
    case FanPolicy::kPeriodic: {
      if (settings.periodMin == 0) return false;
      const uint32_t periodMs = settings.periodMin * kMinuteMs;
      // Moves the period start forward, so that the elapsed time stays below
      // one period and never meets the millis() wrap.
      uint32_t elapsed = nowMs - periodStartMs_;
      if (elapsed >= periodMs) {
        periodStartMs_ += elapsed - elapsed % periodMs;
        elapsed %= periodMs;
      }
      return elapsed < settings.durationMin * kMinuteMs;
    }
    case FanPolicy::kDaily:
      return daily(nowMs, settings, timeKnown, minuteOfDay);
  }
  return false;
}

bool FanScheduler::daily(uint32_t nowMs, const FanSettings& settings, bool timeKnown,
                         uint16_t minuteOfDay) {
  if (aired_) {
    const uint32_t since = nowMs - airingStartMs_;
    if (since < settings.durationMin * kMinuteMs) return true;
    if (since < dailyMinGapMs_) return false;
  }
  bool start = false;
  if (timeKnown) {
    // Any sample within the airing's span starts it: the regulation step
    // runs every 30 s and may miss the set minute itself.
    const uint16_t sinceSet = static_cast<uint16_t>(
        (minuteOfDay + kMinutesPerDay - settings.dailyMinute) % kMinutesPerDay);
    start = sinceSet < settings.durationMin;
  } else {
    start = nowMs - (aired_ ? airingStartMs_ : restartMs_) >= dailyFallbackMs_;
  }
  if (start) {
    aired_ = true;
    airingStartMs_ = nowMs;
  }
  return start;
}

}  // namespace terrarium::control
