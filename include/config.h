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

// Periodic ventilation in ACTIF (cadrage F10, values to validate).
constexpr uint32_t kFanCyclePeriodMs = 60UL * 60UL * 1000UL;
constexpr uint32_t kFanOnDurationMs = 5UL * 60UL * 1000UL;

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

// --- Bounds of the remote settings (RG-reglages-bornes), never overridable ----
// A setpoint or hysteresis received from the network, or read back from NVS,
// is refused outside these bounds; heating must also stop below the high alert.

constexpr float kSetpointMinC = 15.0f;
constexpr float kSetpointMaxC = 22.0f;
constexpr float kHeatHysteresisMinC = 0.2f;
constexpr float kHeatHysteresisMaxC = 2.0f;

// --- Alert thresholds (cadrage F9) --------------------------------------------

constexpr float kAlertTempLowC = 15.0f;
constexpr float kAlertTempHighC = 24.0f;
constexpr float kAlertRhLowPercent = 60.0f;
constexpr float kAlertRhHighPercent = 95.0f;

// --- Timing ------------------------------------------------------------------

constexpr uint32_t kSensorPeriodMs = 30UL * 1000UL;  // Cadrage F1.
constexpr uint32_t kWatchdogTimeoutMs = 10UL * 1000UL;

static_assert(kHeatHysteresisC > 0.0f, "hysteresis must be positive");
static_assert(kFrostHeatOnBelowC < kFrostHeatOffAtC, "frost band must not be empty");
static_assert(kMistPulseMs <= kMistPulseMaxMs, "default pulse exceeds the safety limit");
static_assert(kFanOnDurationMs < kFanCyclePeriodMs, "fan would never stop");
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
static_assert(kDefaultTimeWindowCount <= 4, "at most four windows");
static_assert(kSetpointMinC - kHeatHysteresisMaxC > kFrostHeatOffAtC,
              "lowest active band overlaps frost protection");

}  // namespace terrarium::config
