// Compile-time configuration of the ssk-terrarium firmware.
//
// Source of truth: the cadrage (ssk-terrarium-cadrage, 2026-001), section 4 for
// the pinout and sections 2 and 5 for defaults and safety limits. Defaults are
// the values used until the settings store (phase 2) provides persisted ones;
// safety limits are never overridable at runtime.
#pragma once

#include <cstdint>

namespace terrarium::config {

// --- Pinout (ESP32-WROOM-32, cadrage section 4) -----------------------------

constexpr uint8_t kPinI2cSda = 21;  // SHT45 (Adafruit #6174, address 0x44).
constexpr uint8_t kPinI2cScl = 22;
constexpr uint8_t kPinOneWire = 4;  // DS18B20, 4.7 kOhm pull-up to 3.3 V.
// DHT11 (KY-015 module, powered from 3.3 V, pull-up on the module), cadrage 2026-007.
constexpr uint8_t kPinDht11 = 13;

// Power outputs: channels of the 4-relay optocoupled module (Kiwi KW-1613).
constexpr uint8_t kPinHeaterRelay = 26;
constexpr uint8_t kPinLightRelay = 27;
constexpr uint8_t kPinMistPump = 25;
constexpr uint8_t kPinFanRelay = 33;
// External LED with a 330 Ohm resistor: the KS5016 has no LED on this pin.
constexpr uint8_t kPinStatusLed = 2;

// Level that releases a relay (RG-relais-coupes-sans-commande). The Kiwi
// KW-1613 module is expected to trigger on LOW, so "off" is HIGH.
// TODO(phase 1): confirm with a multimeter on the delivered module and record
// the measured trigger level in the cadrage. Connect no 230 V load before then.
constexpr uint8_t kOutputOffLevel = 1;

// --- Defaults, ACTIF mode (cadrage F3, F4, F5) -------------------------------

constexpr float kSetpointC = 19.0f;
// Half-width of the band: heating starts below setpoint - hysteresis and stops
// at setpoint + hysteresis.
constexpr float kHeatHysteresisC = 0.5f;

constexpr float kMistThresholdRhPercent = 75.0f;
constexpr uint32_t kMistPulseMs = 8UL * 1000UL;
constexpr uint32_t kMistMinIntervalMs = 10UL * 60UL * 1000UL;

constexpr uint8_t kLightOnHour = 8;
constexpr uint8_t kLightOffHour = 21;

// Ventilation defaults (cadrage F10, RG-ventilation-periodique, 2026-013):
// 5 minutes every 60 in ACTIF and PLAGES, the daily HIBERNATION airing at
// 13:00. Settable within the bounds below.
constexpr uint16_t kFanDurationMin = 5;
constexpr uint16_t kFanPeriodMin = 60;
constexpr uint16_t kFanDailyMinute = 13 * 60;
// Rubato spread, and the saturation option, inactive by default
// (RG-ventilation-periodique, cadrage 2026-015).
constexpr uint16_t kFanSpreadPercent = 30;
constexpr uint16_t kSaturationHumidityPercent = 95;
constexpr uint16_t kSaturationWaitMin = 10;
constexpr uint16_t kSaturationDurationMin = 5;
constexpr uint16_t kSaturationPauseMin = 60;
// Day by default (RG-jour-et-nuit): 08:00-21:00, as the light schedule (F5).
constexpr uint16_t kDayStartMinute = 8 * 60;
constexpr uint16_t kDayEndMinute = 21 * 60;

// --- HIBERNATION mode: frost protection only (cadrage, modes table) ----------

constexpr float kFrostHeatOnBelowC = 4.0f;
constexpr float kFrostHeatOffAtC = 6.0f;

// --- PLAGES mode (cadrage 2026-003, RG-chauffage-par-plages) ------------------

// Floor outside the windows: not a setting (RG-reglages-bornes).
constexpr float kFloorHeatOnBelowC = 12.0f;
constexpr float kFloorHeatOffAtC = 14.0f;
// Default windows, in minutes since midnight: 01:35-06:30 and 14:05-17:05.
constexpr uint8_t kDefaultTimeWindowCount = 2;
constexpr uint16_t kDefaultTimeWindows[kDefaultTimeWindowCount][2] = {{1 * 60 + 35, 6 * 60 + 30},
                                                                      {14 * 60 + 5, 17 * 60 + 5}};
// Paris, summer time included (ADR-heure-par-ntp).
constexpr char kDefaultTimezone[] = "CET-1CEST,M3.5.0,M10.5.0/3";

// --- Safety limits (cadrage section 5), never overridable ---------------------

constexpr uint32_t kMistPulseMaxMs = 30UL * 1000UL;
constexpr uint32_t kHeatContinuousMaxMs = 2UL * 60UL * 60UL * 1000UL;
// Off time after the continuous-heat cap, then heating resumes on its own.
constexpr uint32_t kHeatPauseMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kSensorSilentMaxMs = 2UL * 60UL * 1000UL;
// Substrate cut (cadrage 2026-005, RG-coupure-par-le-substrat): heating off
// once the substrate reaches kSubstrateCutAtC, until it falls strictly below
// kSubstrateResumeBelowC. Fixed, whatever the setpoint; a probe silent for
// kSensorSilentMaxMs lifts the cut.
constexpr float kSubstrateCutAtC = 24.0f;
constexpr float kSubstrateResumeBelowC = 18.5f;

// --- Bounds of the remote settings (RG-reglages-bornes), never overridable ----
// A setpoint or hysteresis received from the network, or read back from NVS,
// is refused outside these bounds; heating must also stop below the high alert.

constexpr float kSetpointMinC = 15.0f;
constexpr float kSetpointMaxC = 22.0f;
constexpr float kHeatHysteresisMinC = 0.2f;
constexpr float kHeatHysteresisMaxC = 2.0f;

// Ventilation (RG-reglages-bornes, decision 02 of cadrage 2026-013), in
// minutes; the duration must also stay strictly below the period.
constexpr uint16_t kFanDurationMinMin = 1;
constexpr uint16_t kFanDurationMaxMin = 30;
constexpr uint16_t kFanPeriodMinMin = 15;
constexpr uint16_t kFanPeriodMaxMin = 240;
// Rubato spread and the saturation option (cadrage 2026-015), in percent and
// minutes; the saturation duration takes the bounds of the fan duration.
constexpr uint16_t kFanSpreadMinPercent = 10;
constexpr uint16_t kFanSpreadMaxPercent = 50;
constexpr uint16_t kSaturationHumidityMinPercent = 80;
constexpr uint16_t kSaturationHumidityMaxPercent = 100;
constexpr uint16_t kSaturationWaitMinMin = 1;
constexpr uint16_t kSaturationWaitMaxMin = 60;
constexpr uint16_t kSaturationPauseMinMin = 15;
constexpr uint16_t kSaturationPauseMaxMin = 240;
// The daily HIBERNATION airing: never two within 12 hours, and every 24 hours
// while the time is unknown (RG-ventilation-periodique). Not settings.
constexpr uint32_t kFanDailyMinGapMs = 12UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kFanDailyFallbackMs = 24UL * 60UL * 60UL * 1000UL;

// --- Alert thresholds (cadrage F9) --------------------------------------------

constexpr float kAlertTempLowC = 15.0f;
constexpr float kAlertTempHighC = 24.0f;
constexpr float kAlertRhLowPercent = 60.0f;
constexpr float kAlertRhHighPercent = 95.0f;

// --- Timing ------------------------------------------------------------------

constexpr uint32_t kSensorPeriodMs = 30UL * 1000UL;  // Cadrage F1.
constexpr uint32_t kWatchdogTimeoutMs = 10UL * 1000UL;

// --- SHT45 heater (RG-chauffe-de-la-sht45) -------------------------------------
// Right after a reading at or above this humidity, or rejected for its
// humidity, the SHT45 heats 1 s at 200 mW, at most once per interval. Not a
// setting (INV-5).

constexpr float kProbeHeatMinRhPercent = 95.0f;
constexpr uint32_t kProbeHeatMinIntervalMs = 5UL * 60UL * 1000UL;
// Fixed by the sensor's command; it blocks the loop, plus ~10 ms of measure.
constexpr uint32_t kProbeHeatPulseMs = 1000UL;

static_assert(kProbeHeatPulseMs * 10UL <= kProbeHeatMinIntervalMs,
              "Sensirion caps the heater at a 10 % duty cycle");
static_assert(kProbeHeatPulseMs * 2UL < kWatchdogTimeoutMs, "heating would trip the watchdog");
static_assert(kProbeHeatPulseMs < kSensorPeriodMs, "the chip must cool before the next reading");

static_assert(kHeatHysteresisC > 0.0f, "hysteresis must be positive");
static_assert(kFrostHeatOnBelowC < kFrostHeatOffAtC, "frost band must not be empty");
static_assert(kMistPulseMs <= kMistPulseMaxMs, "default pulse exceeds the safety limit");
static_assert(kFanDurationMin < kFanPeriodMin, "fan would never stop");
static_assert(kFanDurationMinMin <= kFanDurationMin && kFanDurationMin <= kFanDurationMaxMin,
              "default fan duration outside its bounds");
static_assert(kFanPeriodMinMin <= kFanPeriodMin && kFanPeriodMin <= kFanPeriodMaxMin,
              "default fan period outside its bounds");
static_assert(kFanDailyMinute < 24 * 60, "daily airing must fall within the day");
static_assert(kFanDailyMinGapMs < kFanDailyFallbackMs, "fallback must exceed the minimum gap");
static_assert(kLightOnHour < kLightOffHour && kLightOffHour <= 24, "invalid light schedule");
static_assert(kSetpointC + kHeatHysteresisC < kAlertTempHighC,
              "setpoint band reaches the high alert threshold");
static_assert(kSetpointMinC <= kSetpointC && kSetpointC <= kSetpointMaxC,
              "default setpoint outside its bounds");
static_assert(kHeatHysteresisMinC <= kHeatHysteresisC && kHeatHysteresisC <= kHeatHysteresisMaxC,
              "default hysteresis outside its bounds");
static_assert(kFloorHeatOnBelowC < kFloorHeatOffAtC, "floor band must not be empty");
static_assert(kFrostHeatOffAtC < kFloorHeatOnBelowC, "floor band overlaps frost protection");
static_assert(kFloorHeatOffAtC < kAlertTempHighC, "floor band reaches the high alert threshold");
static_assert(kSubstrateResumeBelowC < kSubstrateCutAtC, "substrate cut band must not be empty");
static_assert(kDefaultTimeWindowCount <= 4, "at most four windows");
static_assert(kSetpointMinC - kHeatHysteresisMaxC > kFrostHeatOffAtC,
              "lowest active band overlaps frost protection");

}  // namespace terrarium::config
