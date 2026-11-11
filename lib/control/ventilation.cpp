#include "ventilation.h"

#include <cstring>

namespace terrarium::control {

namespace {

constexpr uint32_t kMinuteMs = 60UL * 1000UL;
constexpr uint16_t kMinutesPerDay = 24 * 60;
// A draw in [-kDrawScale, kDrawScale].
constexpr int32_t kDrawScale = 1000;

bool digit(char c) { return c >= '0' && c <= '9'; }

bool within(uint16_t value, uint16_t min, uint16_t max) { return value >= min && value <= max; }

bool isAcceptedRhythm(const FanRhythm& rhythm, const FanSettingsBounds& b) {
  return (rhythm.mode == FanMode::kMetronome || rhythm.mode == FanMode::kRubato) &&
         within(rhythm.durationMin, b.durationMinMin, b.durationMaxMin) &&
         within(rhythm.periodMin, b.periodMinMin, b.periodMaxMin) &&
         within(rhythm.spreadPercent, b.spreadMinPercent, b.spreadMaxPercent) &&
         rhythm.durationMin < rhythm.periodMin;
}

FanRhythm mergedRhythm(FanRhythm current, const FanRhythmPatch& patch) {
  if (patch.hasMode) current.mode = patch.mode;
  if (patch.hasDuration) current.durationMin = patch.durationMin;
  if (patch.hasPeriod) current.periodMin = patch.periodMin;
  if (patch.hasSpread) current.spreadPercent = patch.spreadPercent;
  return current;
}

}  // namespace

bool isAcceptedFanSettings(const FanSettings& s, const FanSettingsBounds& b) {
  const SaturationOption& sat = s.saturation;
  return isAcceptedRhythm(s.day, b) && isAcceptedRhythm(s.night, b) &&
         s.hibernation.dailyMinute < kMinutesPerDay &&
         within(s.hibernation.durationMin, b.durationMinMin, b.durationMaxMin) &&
         (sat.behaviour == SaturationBehaviour::kAiring ||
          sat.behaviour == SaturationBehaviour::kContinuous) &&
         within(sat.humidityPercent, b.humidityMinPercent, b.humidityMaxPercent) &&
         within(sat.waitMin, b.waitMinMin, b.waitMaxMin) &&
         within(sat.durationMin, b.durationMinMin, b.durationMaxMin) &&
         within(sat.pauseMin, b.pauseMinMin, b.pauseMaxMin);
}

FanSettings merged(const FanSettings& current, const FanSettingsPatch& patch) {
  FanSettings next = current;
  next.day = mergedRhythm(current.day, patch.day);
  next.night = mergedRhythm(current.night, patch.night);
  if (patch.hibernation.hasTime) next.hibernation.dailyMinute = patch.hibernation.dailyMinute;
  if (patch.hibernation.hasDuration) next.hibernation.durationMin = patch.hibernation.durationMin;
  const SaturationPatch& sat = patch.saturation;
  if (sat.hasActive) next.saturation.active = sat.active;
  if (sat.hasBehaviour) next.saturation.behaviour = sat.behaviour;
  if (sat.hasHumidity) next.saturation.humidityPercent = sat.humidityPercent;
  if (sat.hasWait) next.saturation.waitMin = sat.waitMin;
  if (sat.hasDuration) next.saturation.durationMin = sat.durationMin;
  if (sat.hasPause) next.saturation.pauseMin = sat.pauseMin;
  return next;
}

const char* fanModeName(FanMode mode) { return mode == FanMode::kRubato ? "rubato" : "metronome"; }

bool parseFanMode(const char* text, FanMode& out) {
  if (text == nullptr) return false;
  if (std::strcmp(text, "metronome") == 0) {
    out = FanMode::kMetronome;
    return true;
  }
  if (std::strcmp(text, "rubato") == 0) {
    out = FanMode::kRubato;
    return true;
  }
  return false;
}

const char* behaviourName(SaturationBehaviour behaviour) {
  return behaviour == SaturationBehaviour::kContinuous ? "continu" : "aeration";
}

bool parseBehaviour(const char* text, SaturationBehaviour& out) {
  if (text == nullptr) return false;
  if (std::strcmp(text, "aeration") == 0) {
    out = SaturationBehaviour::kAiring;
    return true;
  }
  if (std::strcmp(text, "continu") == 0) {
    out = SaturationBehaviour::kContinuous;
    return true;
  }
  return false;
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

namespace {

constexpr uint8_t kFanLayout = 2;  // 2026-013's six bytes were layout 1.

struct Writer {
  uint8_t* p;
  void u8(uint8_t v) { *p++ = v; }
  void u16(uint16_t v) {
    *p++ = static_cast<uint8_t>(v & 0xFF);
    *p++ = static_cast<uint8_t>(v >> 8);
  }
};

struct Reader {
  const uint8_t* p;
  uint8_t u8() { return *p++; }
  uint16_t u16() {
    const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
    p += 2;
    return v;
  }
};

void writeRhythm(Writer& w, const FanRhythm& r) {
  w.u8(static_cast<uint8_t>(r.mode));
  w.u16(r.durationMin);
  w.u16(r.periodMin);
  w.u16(r.spreadPercent);
}

bool readRhythm(Reader& r, FanRhythm& out) {
  const uint8_t mode = r.u8();
  out.durationMin = r.u16();
  out.periodMin = r.u16();
  out.spreadPercent = r.u16();
  if (mode > static_cast<uint8_t>(FanMode::kRubato)) return false;
  out.mode = static_cast<FanMode>(mode);
  return true;
}

}  // namespace

void encodeFanSettings(const FanSettings& s, uint8_t (&out)[kFanStoredSize]) {
  Writer w{out};
  w.u8(kFanLayout);
  writeRhythm(w, s.day);
  writeRhythm(w, s.night);
  w.u16(s.hibernation.dailyMinute);
  w.u16(s.hibernation.durationMin);
  w.u8(s.saturation.active ? 1 : 0);
  w.u8(static_cast<uint8_t>(s.saturation.behaviour));
  w.u16(s.saturation.humidityPercent);
  w.u16(s.saturation.waitMin);
  w.u16(s.saturation.durationMin);
  w.u16(s.saturation.pauseMin);
}

bool decodeFanSettings(const uint8_t (&in)[kFanStoredSize], FanSettings& out) {
  Reader r{in};
  if (r.u8() != kFanLayout) return false;
  FanSettings s{};
  if (!readRhythm(r, s.day) || !readRhythm(r, s.night)) return false;
  s.hibernation.dailyMinute = r.u16();
  s.hibernation.durationMin = r.u16();
  const uint8_t active = r.u8();
  const uint8_t behaviour = r.u8();
  if (active > 1 || behaviour > static_cast<uint8_t>(SaturationBehaviour::kContinuous)) {
    return false;
  }
  s.saturation.active = active == 1;
  s.saturation.behaviour = static_cast<SaturationBehaviour>(behaviour);
  s.saturation.humidityPercent = r.u16();
  s.saturation.waitMin = r.u16();
  s.saturation.durationMin = r.u16();
  s.saturation.pauseMin = r.u16();
  out = s;
  return true;
}

void FanScheduler::restart(uint32_t nowMs) {
  restartMs_ = nowMs;
  pending_ = true;
  aired_ = false;
}

bool FanScheduler::update(uint32_t nowMs, FanPolicy policy, const FanSettings& settings,
                          Moment moment, bool timeKnown, uint16_t minuteOfDay) {
  switch (policy) {
    case FanPolicy::kOff:
      return false;
    case FanPolicy::kPeriodic:
      return periodic(nowMs, moment == Moment::kNight ? settings.night : settings.day);
    case FanPolicy::kDaily:
      return daily(nowMs, settings.hibernation, timeKnown, minuteOfDay);
  }
  return false;
}

uint16_t FanScheduler::draw(uint16_t value, uint16_t spreadPercent, uint16_t min, uint16_t max) {
  const int32_t u = static_cast<int32_t>(random_() % (2 * kDrawScale + 1)) - kDrawScale;
  // value * (1 + u/1000 * spread/100), rounded half away from zero.
  const int32_t scale = 100 * kDrawScale;
  const int32_t num =
      static_cast<int32_t>(value) * (scale + static_cast<int32_t>(spreadPercent) * u);
  int32_t drawn = (2 * num + scale) / (2 * scale);
  if (drawn < min) drawn = min;
  if (drawn > max) drawn = max;
  return static_cast<uint16_t>(drawn);
}

bool FanScheduler::periodic(uint32_t nowMs, const FanRhythm& rhythm) {
  if (pending_ || nowMs - airingStartMs_ >= intervalMs_) {
    uint16_t duration = rhythm.durationMin;
    uint16_t interval = rhythm.periodMin;
    if (rhythm.mode == FanMode::kRubato) {
      duration =
          draw(duration, rhythm.spreadPercent, bounds_.durationMinMin, bounds_.durationMaxMin);
      interval = draw(interval, rhythm.spreadPercent, bounds_.periodMinMin, bounds_.periodMaxMin);
    }
    if (duration >= interval) duration = interval - 1;
    // The first airing starts at the restart; the next ones an interval after
    // the previous start, so that sampling every 30 s does not drift. A whole
    // interval missed starts afresh.
    uint32_t start = pending_ ? restartMs_ : airingStartMs_ + intervalMs_;
    if (nowMs - start >= interval * kMinuteMs) start = nowMs;
    airingStartMs_ = start;
    durationMs_ = duration * kMinuteMs;
    intervalMs_ = interval * kMinuteMs;
    pending_ = false;
  }
  return nowMs - airingStartMs_ < durationMs_;
}

bool FanScheduler::daily(uint32_t nowMs, const HibernationAiring& airing, bool timeKnown,
                         uint16_t minuteOfDay) {
  if (aired_) {
    const uint32_t since = nowMs - dailyStartMs_;
    if (since < airing.durationMin * kMinuteMs) return true;
    if (since < dailyMinGapMs_) return false;
  }
  bool start = false;
  if (timeKnown) {
    // Any sample within the airing's span starts it: the regulation step runs
    // every 30 s and may miss the set minute itself.
    const uint16_t sinceSet =
        static_cast<uint16_t>((minuteOfDay + kMinutesPerDay - airing.dailyMinute) % kMinutesPerDay);
    start = sinceSet < airing.durationMin;
  } else {
    start = nowMs - (aired_ ? dailyStartMs_ : restartMs_) >= dailyFallbackMs_;
  }
  if (start) {
    aired_ = true;
    dailyStartMs_ = nowMs;
  }
  return start;
}

void SaturationVent::reset() {
  state_ = State::kWaiting;
  saturated_ = false;
}

bool SaturationVent::update(uint32_t nowMs, const SaturationOption& option, bool allowed,
                            bool airValid, float humidityPercent) {
  if (!option.active || !allowed) {
    reset();
    return false;
  }
  const bool saturated = airValid && humidityPercent >= static_cast<float>(option.humidityPercent);
  if (state_ == State::kRunning) {
    const bool timeUp = nowMs - sinceMs_ >= option.durationMin * kMinuteMs;
    const bool dried = option.behaviour == SaturationBehaviour::kContinuous && !saturated;
    if (!timeUp && !dried) return true;
    state_ = State::kPausing;
    sinceMs_ = nowMs;
    return false;
  }
  if (state_ == State::kPausing) {
    if (nowMs - sinceMs_ < option.pauseMin * kMinuteMs) return false;
    state_ = State::kWaiting;
    saturated_ = false;
  }
  if (!saturated) {
    saturated_ = false;
    return false;
  }
  if (!saturated_) {
    saturated_ = true;
    saturatedSinceMs_ = nowMs;
  }
  if (nowMs - saturatedSinceMs_ < option.waitMin * kMinuteMs) return false;
  state_ = State::kRunning;
  sinceMs_ = nowMs;
  saturated_ = false;
  return true;
}

}  // namespace terrarium::control
