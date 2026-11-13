#include "substrate_cap.h"

#include <cmath>
#include <cstring>

namespace terrarium::control {

namespace {

constexpr float kSubstrateMinC = -20.0f;
constexpr float kSubstrateMaxC = 60.0f;
constexpr uint8_t kSubstrateLayout = 1;
constexpr uint32_t kMinuteMs = 60UL * 1000UL;

constexpr char kResumeOnAirName[] = "reprise-air";
constexpr char kHoldName[] = "maintien";

float celsius(int16_t tenths) { return static_cast<float>(tenths) / 10.0f; }

bool within(int32_t value, int32_t min, int32_t max) { return value >= min && value <= max; }

bool isAcceptedThresholds(const SubstrateThresholds& t, const SubstrateSettingsBounds& b) {
  return within(t.cutTenths, b.cutMinTenths, b.cutMaxTenths) &&
         t.resumeTenths >= b.resumeMinTenths &&
         int32_t{t.resumeTenths} <= int32_t{t.cutTenths} - b.gapMinTenths;
}

SubstrateThresholds mergedThresholds(SubstrateThresholds current,
                                     const SubstrateThresholdsPatch& patch) {
  if (patch.hasCut) current.cutTenths = patch.cutTenths;
  if (patch.hasResume) current.resumeTenths = patch.resumeTenths;
  return current;
}

void putU16(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>(value >> 8);
}

uint16_t getU16(const uint8_t* in) {
  return static_cast<uint16_t>(in[0] | static_cast<uint16_t>(in[1]) << 8);
}

}  // namespace

bool isPlausibleSubstrateReading(float temperatureC) {
  // Written so that NaN, which fails every comparison, is rejected.
  return temperatureC >= kSubstrateMinC && temperatureC <= kSubstrateMaxC;
}

bool isAcceptedSubstrateSettings(const SubstrateSettings& s, const SubstrateSettingsBounds& b) {
  return (s.silentBehaviour == SilentProbeBehaviour::kResumeOnAir ||
          s.silentBehaviour == SilentProbeBehaviour::kHold) &&
         within(s.approachTenths, b.approachMinTenths, b.approachMaxTenths) &&
         within(s.silentDelayMin, b.delayMinMin, b.delayMaxMin) && isAcceptedThresholds(s.day, b) &&
         isAcceptedThresholds(s.night, b);
}

SubstrateThresholds thresholdsFor(const SubstrateSettings& settings, Moment moment) {
  return moment == Moment::kNight ? settings.night : settings.day;
}

SubstrateSettings merged(const SubstrateSettings& current, const SubstrateSettingsPatch& patch) {
  SubstrateSettings next = current;
  if (patch.hasActive) next.active = patch.active;
  if (patch.hasApproach) next.approachTenths = patch.approachTenths;
  next.day = mergedThresholds(current.day, patch.day);
  next.night = mergedThresholds(current.night, patch.night);
  if (patch.hasBehaviour) next.silentBehaviour = patch.behaviour;
  if (patch.hasDelay) next.silentDelayMin = patch.delayMin;
  return next;
}

bool tenthsFromCelsius(float c, int16_t& out) {
  if (!(c > -1000.0f && c < 1000.0f)) return false;  // NaN fails both.
  const float scaled = c * 10.0f;
  const float rounded = std::round(scaled);
  // A tenth given as a float, 22.1, is within a few millionths of 221.
  if (std::fabs(scaled - rounded) > 0.001f) return false;
  out = static_cast<int16_t>(rounded);
  return true;
}

const char* silentProbeBehaviourName(SilentProbeBehaviour behaviour) {
  return behaviour == SilentProbeBehaviour::kHold ? kHoldName : kResumeOnAirName;
}

bool parseSilentProbeBehaviour(const char* text, SilentProbeBehaviour& out) {
  if (text == nullptr) return false;
  if (std::strcmp(text, kResumeOnAirName) == 0) {
    out = SilentProbeBehaviour::kResumeOnAir;
    return true;
  }
  if (std::strcmp(text, kHoldName) == 0) {
    out = SilentProbeBehaviour::kHold;
    return true;
  }
  return false;
}

void encodeSubstrateSettings(const SubstrateSettings& s, uint8_t (&out)[kSubstrateStoredSize]) {
  out[0] = kSubstrateLayout;
  out[1] = s.active ? 1 : 0;
  putU16(out + 2, static_cast<uint16_t>(s.approachTenths));
  putU16(out + 4, static_cast<uint16_t>(s.day.cutTenths));
  putU16(out + 6, static_cast<uint16_t>(s.day.resumeTenths));
  putU16(out + 8, static_cast<uint16_t>(s.night.cutTenths));
  putU16(out + 10, static_cast<uint16_t>(s.night.resumeTenths));
  out[12] = static_cast<uint8_t>(s.silentBehaviour);
  putU16(out + 13, s.silentDelayMin);
}

bool decodeSubstrateSettings(const uint8_t (&in)[kSubstrateStoredSize], SubstrateSettings& out) {
  if (in[0] != kSubstrateLayout || in[1] > 1 ||
      in[12] > static_cast<uint8_t>(SilentProbeBehaviour::kHold)) {
    return false;
  }
  SubstrateSettings s{};
  s.active = in[1] == 1;
  s.approachTenths = static_cast<int16_t>(getU16(in + 2));
  s.day = {static_cast<int16_t>(getU16(in + 4)), static_cast<int16_t>(getU16(in + 6))};
  s.night = {static_cast<int16_t>(getU16(in + 8)), static_cast<int16_t>(getU16(in + 10))};
  s.silentBehaviour = static_cast<SilentProbeBehaviour>(in[12]);
  s.silentDelayMin = getU16(in + 13);
  out = s;
  return true;
}

SubstrateCapChange SubstrateCap::update(uint32_t nowMs, const SubstrateSettings& settings,
                                        Moment moment, bool valid, float temperatureC,
                                        bool airValid, float airC) {
  active_ = settings.active;
  thresholds_ = thresholdsFor(settings, moment);
  approachTenths_ = settings.approachTenths;
  valid_ = valid && isPlausibleSubstrateReading(temperatureC);
  if (valid_) temperatureC_ = temperatureC;

  if (!active_) {
    silent_ = false;
    slice_ = false;
    if (!cut_) return SubstrateCapChange::kNone;
    cut_ = false;
    return SubstrateCapChange::kResumedDisabled;
  }

  if (!valid_) {
    slice_ = false;
    if (!cut_) return SubstrateCapChange::kNone;
    if (!silent_) {
      silent_ = true;
      silentSinceMs_ = nowMs;
    }
    if (settings.silentBehaviour == SilentProbeBehaviour::kHold) {
      // Written so that a NaN air reading never lifts the cut.
      if (!airValid || !(airC < holdAirFloorC_)) return SubstrateCapChange::kNone;
      cut_ = false;
      silent_ = false;
      return SubstrateCapChange::kResumedFrost;
    }
    if (nowMs - silentSinceMs_ > uint32_t{settings.silentDelayMin} * kMinuteMs) {
      cut_ = false;
      silent_ = false;
      return SubstrateCapChange::kResumedSilent;
    }
    return SubstrateCapChange::kNone;
  }

  silent_ = false;
  if (!cut_ && temperatureC >= celsius(thresholds_.cutTenths)) {
    cut_ = true;
    slice_ = false;
    return SubstrateCapChange::kCut;
  }
  if (cut_ && temperatureC < celsius(thresholds_.resumeTenths)) {
    cut_ = false;
    return SubstrateCapChange::kResumedCold;
  }
  return SubstrateCapChange::kNone;
}

bool SubstrateCap::inZone() const {
  if (!active_ || cut_ || !valid_ || approachTenths_ <= 0) return false;
  const float lowC = celsius(static_cast<int16_t>(thresholds_.cutTenths - approachTenths_));
  return temperatureC_ >= lowC && temperatureC_ < celsius(thresholds_.cutTenths);
}

int8_t SubstrateCap::minutesFor(float temperatureC) const {
  const int32_t sliceMinutes = static_cast<int32_t>(sliceMs_ / kMinuteMs);
  const float ratio = (celsius(thresholds_.cutTenths) - temperatureC) / celsius(approachTenths_);
  const long minutes = std::lround(ratio * static_cast<float>(sliceMinutes));
  if (minutes < 0) return 0;
  if (minutes > sliceMinutes) return static_cast<int8_t>(sliceMinutes);
  return static_cast<int8_t>(minutes);
}

bool SubstrateCap::filter(uint32_t nowMs, bool demand) {
  if (cut_) return false;
  if (!inZone()) {
    slice_ = false;
    return demand;
  }
  if (slice_ && nowMs - sliceStartMs_ >= sliceMs_) slice_ = false;
  if (!slice_ && demand) {
    slice_ = true;
    sliceStartMs_ = nowMs;
    sliceMinutes_ = minutesFor(temperatureC_);
  }
  return demand && slice_ && nowMs - sliceStartMs_ < uint32_t(sliceMinutes_) * kMinuteMs;
}

int8_t SubstrateCap::partTenths() const {
  if (!active_) return kNoSubstratePart;
  if (cut_) return 0;
  if (!valid_) return kNoSubstratePart;
  if (!inZone()) return 10;
  const int32_t sliceMinutes = static_cast<int32_t>(sliceMs_ / kMinuteMs);
  if (sliceMinutes <= 0) return 10;
  const int32_t minutes = slice_ ? sliceMinutes_ : minutesFor(temperatureC_);
  return static_cast<int8_t>((minutes * 10 + sliceMinutes / 2) / sliceMinutes);
}

}  // namespace terrarium::control
