// Periodic ventilation of the tank (RG-ventilation-periodique): its settings,
// their bounds, and when the fan runs. Pure logic: no hardware, no
// allocation; instants and the time of day are passed in.
#pragma once

#include <cstddef>
#include <cstdint>

namespace terrarium::control {

// What the fan may do in a mode (RG-sorties-par-mode).
enum class FanPolicy : uint8_t {
  kOff,       // MAINTENANCE: off, even in the middle of an airing.
  kPeriodic,  // ACTIF, PLAGES: one airing at the start of each period.
  kDaily,     // HIBERNATION: one airing a day.
};

// Settable from the network and the serial link, within bounds
// (RG-reglages-bornes).
struct FanSettings {
  uint16_t durationMin;  // "duree": how long an airing lasts.
  uint16_t periodMin;    // "periode": ACTIF and PLAGES, one airing every period.
  uint16_t dailyMinute;  // "heure": HIBERNATION, minutes since midnight.
};

// Compile-time bounds, from config.h, both ends included.
struct FanSettingsBounds {
  uint16_t durationMinMin;
  uint16_t durationMaxMin;
  uint16_t periodMinMin;
  uint16_t periodMaxMin;
};

// Within bounds, the duration strictly below the period so that the fan stops
// in each period, and the time of day before 24:00.
bool isAcceptedFanSettings(const FanSettings& settings, const FanSettingsBounds& bounds);

// The fields a cmd/config "ventilation" names; the others keep their value.
struct FanSettingsPatch {
  bool hasDuration;
  uint16_t durationMin;
  bool hasPeriod;
  uint16_t periodMin;
  bool hasDailyMinute;
  uint16_t dailyMinute;
};

FanSettings merged(const FanSettings& current, const FanSettingsPatch& patch);

// "HH:MM", without terminator.
constexpr size_t kClockTimeTextLength = 5;

// Parses exactly "HH:MM" from the first `length` bytes of `text`. Returns
// false and leaves `out` untouched on anything else.
bool parseClockTime(const char* text, size_t length, uint16_t& out);

// Writes "HH:MM". Returns false if `minuteOfDay` is not below 24:00.
bool formatClockTime(uint16_t minuteOfDay, char (&out)[kClockTimeTextLength + 1]);

// Decides, at each regulation step, whether the fan runs.
class FanScheduler {
 public:
  // `dailyMinGapMs`: no daily airing starts sooner after the previous one.
  // `dailyFallbackMs`: time unknown, the daily airing starts this long after
  // the previous one, or after the restart. Neither is a setting (INV-5).
  constexpr FanScheduler(uint32_t dailyMinGapMs, uint32_t dailyFallbackMs)
      : dailyMinGapMs_(dailyMinGapMs), dailyFallbackMs_(dailyFallbackMs) {}

  // At boot and at every mode change: periods count from `nowMs`, which
  // begins with an airing, and the previous daily airing is forgotten.
  void restart(uint32_t nowMs);

  // Whether the fan runs at `nowMs` (millis(), wrap-safe). Called at least
  // once per period. `minuteOfDay` is read only when `timeKnown`. Settings
  // apply at once, without restarting the count.
  bool update(uint32_t nowMs, FanPolicy policy, const FanSettings& settings, bool timeKnown,
              uint16_t minuteOfDay);

 private:
  bool daily(uint32_t nowMs, const FanSettings& settings, bool timeKnown, uint16_t minuteOfDay);

  uint32_t dailyMinGapMs_;
  uint32_t dailyFallbackMs_;
  uint32_t restartMs_ = 0;      // Boot or last mode change.
  uint32_t periodStartMs_ = 0;  // Start of the current period.
  bool aired_ = false;          // A daily airing started since the restart.
  uint32_t airingStartMs_ = 0;  // Start of the last daily airing.
};

}  // namespace terrarium::control
