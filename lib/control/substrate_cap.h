// Cuts heating on a hot substrate, and slows it in the approach zone below
// the cut (RG-coupure-par-le-substrat, cadrage 2026-018): its settings, their
// bounds, and the regulation. Pure logic: no hardware, no allocation.
// Temperatures of the settings are whole tenths of a degree, so that the
// 2 °C gap and the tenth-of-a-degree rule are exact.
#pragma once

#include <cstddef>
#include <cstdint>

#include "day_night.h"

namespace terrarium::control {

// What one reading changed, for the serial announcements.
enum class SubstrateCapChange : uint8_t {
  kNone,
  kCut,              // The substrate reached the cut threshold of the moment.
  kResumedCold,      // The substrate fell below the resume threshold of the moment.
  kResumedSilent,    // "reprise-air": the probe stayed silent too long during a cut.
  kResumedFrost,     // "maintien": the probe is silent and the air fell below the floor.
  kResumedDisabled,  // The cut was disabled during a cut.
};

// What a silent probe does to a cut under way (decision 03 of 2026-018).
enum class SilentProbeBehaviour : uint8_t {
  kResumeOnAir,  // "reprise-air": the cut lifts after the delay.
  kHold,         // "maintien": the cut holds, unless the air falls below the floor.
};

// Cut and resume thresholds of the day, or of the night, in tenths of °C.
struct SubstrateThresholds {
  int16_t cutTenths;     // "coupure": cuts at or above it.
  int16_t resumeTenths;  // "reprise": resumes strictly below it.
};

struct SubstrateSettings {
  bool active;             // "active".
  int16_t approachTenths;  // "approche": width of the zone below the cut, 0 for none.
  SubstrateThresholds day;
  SubstrateThresholds night;
  SilentProbeBehaviour silentBehaviour;  // "sonde_muette.comportement".
  uint16_t silentDelayMin;               // "sonde_muette.delai", for "reprise-air".
};

// Compile-time bounds, from config.h, both ends included (RG-reglages-bornes).
struct SubstrateSettingsBounds {
  int16_t cutMinTenths;
  int16_t cutMaxTenths;
  int16_t resumeMinTenths;  // "reprise_min".
  int16_t gapMinTenths;     // "ecart_min": the resume stays at most cut - gap.
  int16_t approachMinTenths;
  int16_t approachMaxTenths;
  uint16_t delayMinMin;
  uint16_t delayMaxMin;
};

// Every field within bounds, and each resume at least the gap below its cut.
bool isAcceptedSubstrateSettings(const SubstrateSettings& settings,
                                 const SubstrateSettingsBounds& bounds);

// The thresholds that apply at `moment`: the day's while the time is unknown,
// which momentAt already says.
SubstrateThresholds thresholdsFor(const SubstrateSettings& settings, Moment moment);

// The fields a cmd/config "securite_substrat" names; the others keep their value.
struct SubstrateThresholdsPatch {
  bool hasCut;
  int16_t cutTenths;
  bool hasResume;
  int16_t resumeTenths;
};

struct SubstrateSettingsPatch {
  bool hasActive;
  bool active;
  bool hasApproach;
  int16_t approachTenths;
  SubstrateThresholdsPatch day;
  SubstrateThresholdsPatch night;
  bool hasBehaviour;
  SilentProbeBehaviour behaviour;
  bool hasDelay;
  uint16_t delayMin;
};

SubstrateSettings merged(const SubstrateSettings& current, const SubstrateSettingsPatch& patch);

// A temperature given at most to the tenth of a degree, as whole tenths.
// Returns false, leaving `out` untouched, for anything finer, NaN, or beyond
// +/- 1000 °C.
bool tenthsFromCelsius(float celsius, int16_t& out);

// Wire names: "reprise-air", "maintien". Parsing is exact.
const char* silentProbeBehaviourName(SilentProbeBehaviour behaviour);
bool parseSilentProbeBehaviour(const char* text, SilentProbeBehaviour& out);

// NVS form (RG-mode-au-demarrage): a layout version, then every field in
// little-endian. decodeSubstrateSettings returns false on another version, an
// "active" other than 0 or 1, or an unknown behaviour; the caller then checks
// the bounds.
constexpr size_t kSubstrateStoredSize = 15;
void encodeSubstrateSettings(const SubstrateSettings& settings,
                             uint8_t (&out)[kSubstrateStoredSize]);
bool decodeSubstrateSettings(const uint8_t (&in)[kSubstrateStoredSize], SubstrateSettings& out);

// Whether a substrate reading can be true: -20..60 °C. NaN is rejected.
bool isPlausibleSubstrateReading(float temperatureC);

// partTenths() when nothing says what the substrate allows: "part_substrat" null.
constexpr int8_t kNoSubstratePart = -1;

class SubstrateCap {
 public:
  // `holdAirFloorC`: under "maintien", a silent cut lifts once the air is
  // strictly below it. `sliceMs`: the approach slice. Neither is a setting.
  constexpr SubstrateCap(float holdAirFloorC, uint32_t sliceMs)
      : holdAirFloorC_(holdAirFloorC), sliceMs_(sliceMs) {}

  // Feeds the reading taken at `nowMs` (millis(), wrap-safe), with the
  // settings and the moment of the day in force. Active, a cut starts on a
  // valid reading at or above the cut of the moment and ends on a valid
  // reading strictly below its resume; a silent probe then follows the set
  // behaviour. Without a probe, or disabled, it never cuts. `airValid` and
  // `airC`: the air measure that regulates, for the floor of "maintien".
  SubstrateCapChange update(uint32_t nowMs, const SubstrateSettings& settings, Moment moment,
                            bool valid, float temperatureC, bool airValid, float airC);

  // Whether the heater may run, given what the mode asks for (`demand`), at
  // the same `nowMs` as the last update. Off during a cut. In the approach
  // zone, from cut - approach included to cut excluded, it runs only the
  // first part of each slice, fixed at its start from the last reading:
  // (cut - reading) / approach, rounded to the minute. A slice starts when the
  // mode asks for heat in the zone, and the next one when it ends.
  bool filter(uint32_t nowMs, bool demand);

  bool cut() const { return cut_; }

  // What the substrate allows of each slice, in tenths ("part_substrat"): 0
  // during a cut, 10 outside the zone, the part of the slice under way or
  // that a slice would take now, or kNoSubstratePart when disabled or
  // without a valid reading. Read after filter().
  int8_t partTenths() const;

 private:
  int8_t minutesFor(float temperatureC) const;
  bool inZone() const;

  float holdAirFloorC_;
  uint32_t sliceMs_;
  bool cut_ = false;
  bool silent_ = false;
  uint32_t silentSinceMs_ = 0;
  // From the last update.
  bool active_ = true;
  bool valid_ = false;
  float temperatureC_ = 0.0f;
  SubstrateThresholds thresholds_{0, 0};
  int16_t approachTenths_ = 0;
  // The approach slice under way.
  bool slice_ = false;
  uint32_t sliceStartMs_ = 0;
  int8_t sliceMinutes_ = 0;
};

}  // namespace terrarium::control
