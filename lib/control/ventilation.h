// Ventilation of the tank (RG-ventilation-periodique): its settings and their
// bounds, when the fan runs by mode and by moment of the day, and the
// saturation option. Pure logic: no hardware, no allocation; instants, the
// time of day and random numbers are passed in.
#pragma once

#include <cstddef>
#include <cstdint>

#include "day_night.h"

namespace terrarium::control {

// What the fan may do in a mode (RG-sorties-par-mode).
enum class FanPolicy : uint8_t {
  kOff,       // MAINTENANCE: off, even in the middle of an airing.
  kPeriodic,  // ACTIF, PLAGES: the rhythm of the moment of the day.
  kDaily,     // HIBERNATION: one airing a day.
};

enum class FanMode : uint8_t {
  kMetronome,  // "metronome": the same duration, the same interval.
  kRubato,     // "rubato": both drawn within +/- spread (decision 02 of 2026-015).
};

// The rhythm of the day, or of the night.
struct FanRhythm {
  FanMode mode;
  uint16_t durationMin;    // "duree".
  uint16_t periodMin;      // "periode": from the start of one airing to the next.
  uint16_t spreadPercent;  // "ecart": rubato only.
};

struct HibernationAiring {
  uint16_t dailyMinute;  // "heure", minutes since midnight.
  uint16_t durationMin;  // "duree".
};

enum class SaturationBehaviour : uint8_t {
  kAiring,      // "aeration": run `duree` minutes.
  kContinuous,  // "continu": run while saturated, at most `duree` minutes.
};

struct SaturationOption {
  bool active;
  SaturationBehaviour behaviour;
  uint16_t humidityPercent;  // "humidite": trigger at or above it.
  uint16_t waitMin;          // "attente": saturated that long before running.
  uint16_t durationMin;      // "duree".
  uint16_t pauseMin;         // "pause": no new trigger that long after a run.
};

struct FanSettings {
  FanRhythm day;
  FanRhythm night;
  HibernationAiring hibernation;
  SaturationOption saturation;
};

// Compile-time bounds, from config.h, both ends included. Durations apply to
// the rhythms, the hibernation airing and the saturation option alike.
struct FanSettingsBounds {
  uint16_t durationMinMin;
  uint16_t durationMaxMin;
  uint16_t periodMinMin;
  uint16_t periodMaxMin;
  uint16_t spreadMinPercent;
  uint16_t spreadMaxPercent;
  uint16_t humidityMinPercent;
  uint16_t humidityMaxPercent;
  uint16_t waitMinMin;
  uint16_t waitMaxMin;
  uint16_t pauseMinMin;
  uint16_t pauseMaxMin;
};

// Every field within bounds, each rhythm's duration strictly below its period,
// and the hibernation time before 24:00 (RG-reglages-bornes).
bool isAcceptedFanSettings(const FanSettings& settings, const FanSettingsBounds& bounds);

// The fields a cmd/config "ventilation" names; the others keep their value.
struct FanRhythmPatch {
  bool hasMode;
  FanMode mode;
  bool hasDuration;
  uint16_t durationMin;
  bool hasPeriod;
  uint16_t periodMin;
  bool hasSpread;
  uint16_t spreadPercent;
};

struct HibernationPatch {
  bool hasTime;
  uint16_t dailyMinute;
  bool hasDuration;
  uint16_t durationMin;
};

struct SaturationPatch {
  bool hasActive;
  bool active;
  bool hasBehaviour;
  SaturationBehaviour behaviour;
  bool hasHumidity;
  uint16_t humidityPercent;
  bool hasWait;
  uint16_t waitMin;
  bool hasDuration;
  uint16_t durationMin;
  bool hasPause;
  uint16_t pauseMin;
};

struct FanSettingsPatch {
  FanRhythmPatch day;
  FanRhythmPatch night;
  HibernationPatch hibernation;
  SaturationPatch saturation;
};

FanSettings merged(const FanSettings& current, const FanSettingsPatch& patch);

// Wire names: "metronome", "rubato"; "aeration", "continu". Parsing is exact.
const char* fanModeName(FanMode mode);
bool parseFanMode(const char* text, FanMode& out);
const char* behaviourName(SaturationBehaviour behaviour);
bool parseBehaviour(const char* text, SaturationBehaviour& out);

// "HH:MM", without terminator.
constexpr size_t kClockTimeTextLength = 5;

// Parses exactly "HH:MM" from the first `length` bytes of `text`. Returns
// false and leaves `out` untouched on anything else.
bool parseClockTime(const char* text, size_t length, uint16_t& out);

// Writes "HH:MM". Returns false if `minuteOfDay` is not below 24:00.
bool formatClockTime(uint16_t minuteOfDay, char (&out)[kClockTimeTextLength + 1]);

// NVS form (RG-mode-au-demarrage): a layout version, then every field in
// little-endian. decodeFanSettings returns false on another version or an
// unknown mode or behaviour; the caller then checks the bounds.
constexpr size_t kFanStoredSize = 29;
void encodeFanSettings(const FanSettings& settings, uint8_t (&out)[kFanStoredSize]);
bool decodeFanSettings(const uint8_t (&in)[kFanStoredSize], FanSettings& out);

// Any 32-bit random source: esp_random on the board, a script in the tests.
using RandomSource = uint32_t (*)();

// Decides, at each regulation step, whether the rhythm of the mode runs the
// fan. The saturation option is SaturationVent's.
class FanScheduler {
 public:
  // `dailyMinGapMs`: no daily airing starts sooner after the previous one.
  // `dailyFallbackMs`: time unknown, the daily airing starts this long after
  // the previous one, or after the restart. Neither is a setting (INV-5).
  FanScheduler(uint32_t dailyMinGapMs, uint32_t dailyFallbackMs, const FanSettingsBounds& bounds,
               RandomSource random)
      : dailyMinGapMs_(dailyMinGapMs),
        dailyFallbackMs_(dailyFallbackMs),
        bounds_(bounds),
        random_(random) {}

  // At boot and at every mode change: an airing starts at `nowMs`, and the
  // previous daily airing is forgotten.
  void restart(uint32_t nowMs);

  // Whether the fan runs at `nowMs` (millis(), wrap-safe). Called at least
  // once per airing. The duration of an airing and the interval to the next
  // are fixed at its start, from the rhythm of `moment` (decision 03 of
  // 2026-015). `minuteOfDay` is read only when `timeKnown`.
  bool update(uint32_t nowMs, FanPolicy policy, const FanSettings& settings, Moment moment,
              bool timeKnown, uint16_t minuteOfDay);

 private:
  bool periodic(uint32_t nowMs, const FanRhythm& rhythm);
  bool daily(uint32_t nowMs, const HibernationAiring& airing, bool timeKnown, uint16_t minuteOfDay);
  uint16_t draw(uint16_t value, uint16_t spreadPercent, uint16_t min, uint16_t max);

  uint32_t dailyMinGapMs_;
  uint32_t dailyFallbackMs_;
  FanSettingsBounds bounds_;
  RandomSource random_;
  uint32_t restartMs_ = 0;      // Boot or last mode change.
  bool pending_ = true;         // The first airing since the restart is due.
  uint32_t airingStartMs_ = 0;  // Start of the current or last periodic airing.
  uint32_t durationMs_ = 0;     // Its duration.
  uint32_t intervalMs_ = 0;     // From its start to the next one.
  bool aired_ = false;          // A daily airing started since the restart.
  uint32_t dailyStartMs_ = 0;   // Start of the last daily airing.
};

// The saturation option: runs the fan once the air has stayed saturated long
// enough, within a duration and a pause.
class SaturationVent {
 public:
  // At each measurement. `allowed` is false in MAINTENANCE; `airValid` false
  // when no interior probe gives the air measure (RG-sondes-d-air).
  bool update(uint32_t nowMs, const SaturationOption& option, bool allowed, bool airValid,
              float humidityPercent);
  bool running() const { return state_ == State::kRunning; }
  void reset();

 private:
  enum class State : uint8_t { kWaiting, kRunning, kPausing };
  State state_ = State::kWaiting;
  bool saturated_ = false;  // Saturated since saturatedSinceMs_.
  uint32_t saturatedSinceMs_ = 0;
  uint32_t sinceMs_ = 0;  // Start of the run, or of the pause.
};

}  // namespace terrarium::control
