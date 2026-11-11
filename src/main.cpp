// ssk-terrarium firmware entry point.
//
// The control path (sensors -> regulator -> outputs) runs entirely on the ESP32
// and never waits for the network. MQTT, web and alerts are supervision only.

#include <Arduino.h>
#include <Wire.h>
#include <esp_idf_version.h>
#include <esp_random.h>
#include <esp_task_wdt.h>

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "air_probes.h"
#include "air_sensor.h"
#include "alerts.h"
#include "commands.h"
#include "config.h"
#include "day_night.h"
#include "dht_sensor.h"
#include "heat_guard.h"
#include "heat_settings.h"
#include "hysteresis_regulator.h"
#include "mode_machine.h"
#include "outputs.h"
#include "posix_tz.h"
#include "probe_heater.h"
#include "sensor_watch.h"
#include "serial_command.h"
#include "settings_store.h"
#include "state_message.h"
#include "substrate_cap.h"
#include "substrate_sensor.h"
#include "supervisor.h"
#include "time_windows.h"
#include "update_command.h"
#include "ventilation.h"

// Set by outils/version_firmware.py from `git describe`.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "inconnue"
#endif

// The owner's minisign public key, set by outils/cle_mise_a_jour.py from
// outils/mise-a-jour.pub (RG-image-signee). No firmware without it.
#ifndef UPDATE_PUBLIC_KEY
#error "UPDATE_PUBLIC_KEY manquante : outils/mise-a-jour.pub, voir outils/cle-mise-a-jour.sh"
#endif

namespace cfg = terrarium::config;
using terrarium::control::AirMeasure;
using terrarium::control::AirRejection;
using terrarium::control::AirSource;
using terrarium::control::DayNight;
using terrarium::control::FanPolicy;
using terrarium::control::FanScheduler;
using terrarium::control::FanSettings;
using terrarium::control::HeatGuard;
using terrarium::control::HeatingPolicy;
using terrarium::control::HeatReason;
using terrarium::control::HeatSettings;
using terrarium::control::HysteresisRegulator;
using terrarium::control::kProbeCount;
using terrarium::control::Mode;
using terrarium::control::ModeMachine;
using terrarium::control::Moment;
using terrarium::control::Probe;
using terrarium::control::ProbeRole;
using terrarium::control::ProbeSample;
using terrarium::control::ProbeSetup;
using terrarium::control::SaturationVent;
using terrarium::control::ScheduledHeating;
using terrarium::control::SensorHealth;
using terrarium::control::SensorWatch;
using terrarium::control::SettingCommand;
using terrarium::control::SubstrateCapChange;
using terrarium::control::TimeWindows;

namespace {

// Mode at power-up when NVS holds no valid one (RG-mode-au-demarrage).
constexpr Mode kFallbackBootMode = Mode::kMaintenance;

// Longest serial command line, terminator included: "licence " and a token.
// "LICENCE <token>", or "CONFIG <json>" with six seasons (2026-015).
constexpr size_t kSerialLineSize =
    8 + terrarium::update::kLicenceSize > 512 ? 8 + terrarium::update::kLicenceSize : 512;

// All objects are static: nothing is allocated once loop() runs.
terrarium::outputs::Outputs outputs({cfg::kPinHeaterRelay, cfg::kPinLightRelay, cfg::kPinMistPump,
                                     cfg::kPinFanRelay, cfg::kOutputOffLevel});
terrarium::sensors::AirSensor airSensor;
terrarium::sensors::DhtSensor dhtSensor(cfg::kPinDht11);
terrarium::sensors::SubstrateSensor substrateSensor(cfg::kPinOneWire);
terrarium::storage::SettingsStore settingsStore;
terrarium::network::Supervisor supervisor(settingsStore);
terrarium::alerts::Notifier notifier;

ModeMachine modeMachine(kFallbackBootMode);
// The interior air, all interior probes together (RG-capteur-en-defaut).
SensorWatch airWatch(cfg::kSensorSilentMaxMs);
// Each probe on its own, safety always on: its own fault is only signalled.
SensorWatch probeWatches[kProbeCount] = {SensorWatch(cfg::kSensorSilentMaxMs),
                                         SensorWatch(cfg::kSensorSilentMaxMs)};
// Roles of the probes and their combination (RG-sondes-d-air), from NVS.
ProbeSetup probeSetup = terrarium::control::kDefaultProbeSetup;
// This cycle's samples and the air measure made of them.
ProbeSample probeSamples[kProbeCount] = {{AirRejection::kNone, 0.0f, 0.0f},
                                         {AirRejection::kNone, 0.0f, 0.0f}};
AirMeasure airMeasure{AirSource::kNone, 0.0f, 0.0f, AirRejection::kNone, AirRejection::kNone};
// The owner keeps the interior air's safety on, unless told otherwise over
// MQTT and saved in NVS (RG-capteur-en-defaut, INV-2).
bool airSafety = true;
// Heats the SHT45 out of condensation (RG-chauffe-de-la-sht45).
terrarium::control::ProbeHeater sht45Heater(cfg::kProbeHeatMinRhPercent,
                                            cfg::kProbeHeatMinIntervalMs);
HeatGuard heatGuard(cfg::kHeatContinuousMaxMs, cfg::kHeatPauseMs);
terrarium::control::SubstrateCap substrateCap(cfg::kSubstrateCutAtC, cfg::kSubstrateResumeBelowC,
                                              cfg::kSensorSilentMaxMs);
// Bounds of the settings received from the network (RG-reglages-bornes).
constexpr terrarium::control::HeatSettingsBounds kHeatBounds{
    cfg::kSetpointMinC, cfg::kSetpointMaxC, cfg::kHeatHysteresisMinC, cfg::kHeatHysteresisMaxC,
    cfg::kAlertTempHighC};
HeatSettings heatSettings{cfg::kSetpointC, cfg::kHeatHysteresisC};
// Ventilation (RG-ventilation-periodique): settings within compile-time
// bounds (RG-reglages-bornes), and the schedule restarted at each mode change.
constexpr terrarium::control::FanSettingsBounds kFanBounds{cfg::kFanDurationMinMin,
                                                           cfg::kFanDurationMaxMin,
                                                           cfg::kFanPeriodMinMin,
                                                           cfg::kFanPeriodMaxMin,
                                                           cfg::kFanSpreadMinPercent,
                                                           cfg::kFanSpreadMaxPercent,
                                                           cfg::kSaturationHumidityMinPercent,
                                                           cfg::kSaturationHumidityMaxPercent,
                                                           cfg::kSaturationWaitMinMin,
                                                           cfg::kSaturationWaitMaxMin,
                                                           cfg::kSaturationPauseMinMin,
                                                           cfg::kSaturationPauseMaxMin};
constexpr terrarium::control::FanRhythm kDefaultRhythm{terrarium::control::FanMode::kMetronome,
                                                       cfg::kFanDurationMin, cfg::kFanPeriodMin,
                                                       cfg::kFanSpreadPercent};
constexpr FanSettings kDefaultFanSettings{
    kDefaultRhythm,
    kDefaultRhythm,
    {cfg::kFanDailyMinute, cfg::kFanDurationMin},
    {false, terrarium::control::SaturationBehaviour::kAiring, cfg::kSaturationHumidityPercent,
     cfg::kSaturationWaitMin, cfg::kSaturationDurationMin, cfg::kSaturationPauseMin}};
FanSettings fanSettings = kDefaultFanSettings;
FanScheduler fanScheduler(cfg::kFanDailyMinGapMs, cfg::kFanDailyFallbackMs, kFanBounds, esp_random);
SaturationVent saturationVent;
bool fanBySaturation = false;  // The fan runs for the saturation option.
// Day and night (RG-jour-et-nuit), and the night setpoint of ACTIF (2026-016).
DayNight dayNight{terrarium::control::DayNightMode::kMetronome,
                  {cfg::kDayStartMinute, cfg::kDayEndMinute},
                  0,
                  {}};
HeatSettings nightHeat{cfg::kSetpointC, cfg::kHeatHysteresisC};
Moment moment = Moment::kDay;
HysteresisRegulator activeHeating =
    HysteresisRegulator::aroundSetpoint(cfg::kSetpointC, cfg::kHeatHysteresisC);
HysteresisRegulator frostHeating(cfg::kFrostHeatOnBelowC, cfg::kFrostHeatOffAtC);
HysteresisRegulator floorHeating(cfg::kFloorHeatOnBelowC, cfg::kFloorHeatOffAtC);

// PLAGES (RG-chauffage-par-plages): windows, time zone, and whether NTP has
// answered since boot. Only this task reads the clock or sets TZ.
TimeWindows timeWindows{0, {}};
char timeZone[terrarium::control::kTimezoneSize] = {};
uint32_t seenTimeSyncs = 0;
bool timeKnown = false;

// What PLAGES last announced on the serial link, to report changes only.
enum class ScheduleReport : uint8_t { kNone, kTimeUnknown, kInWindow, kOutside };
ScheduleReport lastScheduleReport = ScheduleReport::kNone;

uint32_t lastSensorReadMs = 0;
bool firstReadDone = false;

char serialLine[kSerialLineSize];
size_t serialLineLength = 0;
bool serialLineOverflow = false;

void startWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  // Arduino core 3.x already starts the task watchdog: reconfigure it.
  const esp_task_wdt_config_t config = {
      .timeout_ms = cfg::kWatchdogTimeoutMs,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&config);
#else
  esp_task_wdt_init(cfg::kWatchdogTimeoutMs / 1000, true);
#endif
  esp_task_wdt_add(nullptr);  // Watch the loop task.
}

// Heater state for one regulator, given the air sensor health
// (RG-capteur-en-defaut): regulate on a valid reading, keep the current state
// while the sensor is briefly silent, switch off once it is in fault.
bool heaterFrom(HysteresisRegulator& regulator, SensorHealth health, float temperatureC) {
  switch (health) {
    case SensorHealth::kOk:
      return regulator.update(temperatureC, true);
    case SensorHealth::kHolding:
      return regulator.isOn();
    case SensorHealth::kFault:
      break;
  }
  regulator.reset();
  return false;
}

TimeWindows defaultTimeWindows() {
  TimeWindows windows{cfg::kDefaultTimeWindowCount, {}};
  for (uint8_t i = 0; i < cfg::kDefaultTimeWindowCount; ++i) {
    windows.windows[i] = {cfg::kDefaultTimeWindows[i][0], cfg::kDefaultTimeWindows[i][1]};
  }
  return windows;
}

void applyTimezoneToClock() {
  setenv("TZ", timeZone, 1);
  tzset();
}

// Local time, minutes since midnight; meaningful only when timeKnown.
uint16_t minuteOfDay() {
  const time_t now = time(nullptr);
  struct tm local{};
  localtime_r(&now, &local);
  return static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min);
}

// Local day of a 365-day year; meaningful only when timeKnown.
uint16_t dayOfYearNow() {
  const time_t now = time(nullptr);
  struct tm local{};
  localtime_r(&now, &local);
  return terrarium::control::dayOfYear(local.tm_mon + 1, local.tm_mday);
}

// Marks the time known at the first NTP answer, and reports each one.
void pollTimeSync() {
  const uint32_t syncs = supervisor.timeSyncCount();
  if (syncs == seenTimeSyncs) return;
  seenTimeSyncs = syncs;
  const bool first = !timeKnown;
  timeKnown = true;
  const time_t now = time(nullptr);
  struct tm local{};
  localtime_r(&now, &local);
  char text[24];
  strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
  Serial.printf("HEURE %s par NTP : %s, fuseau %s\r\n", first ? "connue" : "resynchronisee", text,
                timeZone);
}

// Heater state in PLAGES: the setpoint in a window or while the time is
// unknown, the 12-14 C floor outside (RG-chauffage-par-plages).
bool scheduledHeater(SensorHealth health, float temperatureC) {
  const uint16_t minute = timeKnown ? minuteOfDay() : 0;
  const ScheduledHeating target =
      terrarium::control::scheduledHeating(timeKnown, timeWindows, minute);
  ScheduleReport report = ScheduleReport::kTimeUnknown;
  if (timeKnown) {
    report = target == ScheduledHeating::kSetpoint ? ScheduleReport::kInWindow
                                                   : ScheduleReport::kOutside;
  }
  if (report != lastScheduleReport) {
    lastScheduleReport = report;
    switch (report) {
      case ScheduleReport::kTimeUnknown:
        Serial.printf(
            "PLAGES heure inconnue : chauffage sur la consigne %.1fC jusqu'a l'heure NTP\r\n",
            heatSettings.setpointC);
        break;
      case ScheduleReport::kInWindow:
        Serial.printf("PLAGES en plage a %02u:%02u : chauffage sur la consigne %.1fC\r\n",
                      minute / 60U, minute % 60U, heatSettings.setpointC);
        break;
      case ScheduleReport::kOutside:
        Serial.printf(
            "PLAGES hors plage a %02u:%02u : chauffage seulement sous %.0fC, arret a %.0fC\r\n",
            minute / 60U, minute % 60U, cfg::kFloorHeatOnBelowC, cfg::kFloorHeatOffAtC);
        break;
      case ScheduleReport::kNone:
        break;
    }
  }
  frostHeating.reset();
  if (target == ScheduledHeating::kSetpoint) {
    floorHeating.reset();
    return heaterFrom(activeHeating, health, temperatureC);
  }
  activeHeating.reset();
  return heaterFrom(floorHeating, health, temperatureC);
}

bool isInterior(Probe probe) {
  return probeSetup.roles[static_cast<uint8_t>(probe)] == ProbeRole::kInterior;
}

bool probeInFault(Probe probe) {
  return probeWatches[static_cast<uint8_t>(probe)].health() == SensorHealth::kFault;
}

// Classifies each probe's reading, watches it on its own, and reports its
// rejections and faults (RG-capteur-en-defaut).
void watchProbes(const terrarium::sensors::AirReading (&readings)[kProbeCount], uint32_t nowMs) {
  for (uint8_t i = 0; i < kProbeCount; ++i) {
    const auto probe = static_cast<Probe>(i);
    const char* name = terrarium::control::probeName(probe);
    const terrarium::sensors::AirReading& reading = readings[i];
    const AirRejection previousRejection = probeWatches[i].rejection();
    const bool wasInFault = probeInFault(probe);
    const AirRejection rejection = terrarium::control::classifyAirReading(
        reading.valid, reading.temperatureC, reading.humidityRhPercent);
    probeSamples[i] = {rejection, reading.temperatureC, reading.humidityRhPercent};
    probeWatches[i].update(nowMs, rejection, /*safetyEnabled=*/true);
    if (rejection != previousRejection) {
      if (rejection == AirRejection::kNone) {
        Serial.printf("MESURE %s acceptee\r\n", name);
      } else {
        Serial.printf("REJET mesure %s : %s\r\n", name,
                      terrarium::control::rejectionName(rejection));
      }
    }
    if (probeInFault(probe) != wasInFault) {
      Serial.printf("%s sonde %s (%s)\r\n", wasInFault ? "REPRISE" : "DEFAUT", name,
                    terrarium::control::roleName(probeSetup.roles[i]));
    }
  }
}

// The one air measure the regulation runs on (RG-sondes-d-air), and the health
// of the interior air as a whole (RG-capteur-en-defaut).
SensorHealth measureAir(uint32_t nowMs) {
  const AirSource previousSource = airMeasure.source;
  airMeasure = terrarium::control::combineAir(probeSetup, probeSamples);
  if (airMeasure.source != previousSource) {
    if (const char* source = terrarium::control::sourceName(airMeasure.source)) {
      Serial.printf("REGULATION sur %s\r\n", source);
    } else {
      Serial.println("REGULATION sans sonde interieure acceptee");
    }
  }
  const SensorHealth previous = airWatch.health();
  const SensorHealth health = airWatch.update(nowMs, airMeasure.watchRejection, airSafety);
  if (health != previous && (health == SensorHealth::kFault || previous == SensorHealth::kFault)) {
    Serial.println(health == SensorHealth::kFault ? "DEFAUT capteur air" : "REPRISE capteur air");
  }
  return health;
}

// Heats the SHT45 out of condensation right after its reading, so between two
// readings (RG-chauffe-de-la-sht45): nothing is read during the pulse, the
// reading it returns is thrown away, and the next one comes at the usual time,
// once the chip has cooled. The outputs keep what the last reading decided.
void heatAirSensorIfDue(const terrarium::sensors::AirReading& reading, uint32_t nowMs) {
  const ProbeSample& sample = probeSamples[static_cast<uint8_t>(Probe::kSht45)];
  const HeatReason reason = sht45Heater.due(nowMs, sample.rejection, sample.humidityRhPercent);
  if (reason == HeatReason::kNone) return;
  esp_task_wdt_reset();
  if (!airSensor.heat()) {
    Serial.println("ERREUR chauffe sonde sht45 refusee");
    return;
  }
  Serial.printf("CHAUFFE sonde sht45 : humidite %.1f %%\r\n", reading.humidityRhPercent);
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatProbeHeatingEvent(reason, event)) supervisor.sendEvent(event);
}

// One regulation step. Misting and light stay off (RG-sorties-par-mode); the
// fan airs the tank by mode and moment of the day, and on saturated air if the
// owner wants it (RG-ventilation-periodique); ACTIF heats to the setpoint of
// the moment (RG-chauffage-par-hysteresis); the interior air in fault switches
// heating off, and so
// do a hot substrate (RG-coupure-par-le-substrat) and heating for too long
// (RG-chauffe-continue-bornee).
void regulate(const terrarium::sensors::AirReading (&readings)[kProbeCount],
              const terrarium::sensors::SubstrateReading& substrate, uint32_t nowMs) {
  watchProbes(readings, nowMs);
  const SensorHealth health = measureAir(nowMs);
  const float airTemperatureC = airMeasure.temperatureC;

  const terrarium::control::ModePolicy policy = modeMachine.policy();
  terrarium::outputs::OutputState next{false, false, false, false};
  const uint16_t minute = timeKnown ? minuteOfDay() : 0;
  moment =
      terrarium::control::momentAt(dayNight, timeKnown, timeKnown ? dayOfYearNow() : 0, minute);
  // Night setpoint in ACTIF only; PLAGES heats to the day one in its windows.
  const HeatSettings& tuning =
      policy.heating == HeatingPolicy::kSetpoint && moment == Moment::kNight ? nightHeat
                                                                             : heatSettings;
  activeHeating.retune(tuning.setpointC, tuning.hysteresisC);

  if (policy.heating != HeatingPolicy::kSchedule) lastScheduleReport = ScheduleReport::kNone;
  switch (policy.heating) {
    case HeatingPolicy::kSetpoint:
      frostHeating.reset();
      floorHeating.reset();
      next.heater = heaterFrom(activeHeating, health, airTemperatureC);
      break;
    case HeatingPolicy::kSchedule:
      next.heater = scheduledHeater(health, airTemperatureC);
      break;
    case HeatingPolicy::kFrostProtection:
      activeHeating.reset();
      floorHeating.reset();
      next.heater = heaterFrom(frostHeating, health, airTemperatureC);
      break;
    case HeatingPolicy::kOff:
      activeHeating.reset();
      frostHeating.reset();
      floorHeating.reset();
      break;
  }

  // Before the guard: a cut heater does not count as continuous heating.
  switch (substrateCap.update(nowMs, substrate.valid, substrate.temperatureC)) {
    case SubstrateCapChange::kCut:
      Serial.printf("COUPURE chauffage : substrat a %.1f C\r\n", substrate.temperatureC);
      break;
    case SubstrateCapChange::kResumedCold:
      Serial.printf("REPRISE chauffage : substrat a %.1f C\r\n", substrate.temperatureC);
      break;
    case SubstrateCapChange::kResumedSilent:
      Serial.println("REPRISE chauffage : sonde substrat muette");
      break;
    case SubstrateCapChange::kNone:
      break;
  }
  if (substrateCap.cut()) next.heater = false;

  const bool wasPaused = heatGuard.paused();
  next.heater = heatGuard.filter(nowMs, next.heater);
  if (heatGuard.paused() != wasPaused) {
    Serial.println(heatGuard.paused() ? "PAUSE chauffage apres 2 h continues"
                                      : "REPRISE chauffage");
  }
  // With the safety off, a missing air measure still lights the LED, and so
  // does an interior probe in fault while another regulates.
  bool interiorProbeFault = false;
  for (uint8_t i = 0; i < kProbeCount; ++i) {
    const auto probe = static_cast<Probe>(i);
    interiorProbeFault = interiorProbeFault || (isInterior(probe) && probeInFault(probe));
  }
  const bool fault = health == SensorHealth::kFault || heatGuard.paused() ||
                     (!airSafety && airMeasure.source == AirSource::kNone) || interiorProbeFault;
  digitalWrite(cfg::kPinStatusLed, fault ? HIGH : LOW);

  // The faults under way: a remote update goes on, and names them
  // (RG-mise-a-jour-pendant-un-defaut).
  namespace update = terrarium::update;
  constexpr update::UpdateFaults kProbeFaults[kProbeCount] = {update::kSht45Fault,
                                                              update::kDht11Fault};
  update::UpdateFaults faults = 0;
  if (health == SensorHealth::kFault) faults |= update::kAirFault;
  for (uint8_t i = 0; i < kProbeCount; ++i) {
    if (probeInFault(static_cast<Probe>(i))) faults |= kProbeFaults[i];
  }
  if (!substrate.valid) faults |= update::kSubstrateRejected;
  if (substrateCap.cut()) faults |= update::kSubstrateCut;
  if (heatGuard.paused()) faults |= update::kHeatPaused;
  supervisor.setUpdateFaults(faults);

  // TODO(phase 2): light from the NTP time when policy.lightAllowed.
  const bool byRhythm =
      fanScheduler.update(nowMs, policy.fan, fanSettings, moment, timeKnown, minute);
  const bool bySaturation =
      saturationVent.update(nowMs, fanSettings.saturation, policy.fan != FanPolicy::kOff,
                            airMeasure.source != AirSource::kNone, airMeasure.humidityRhPercent);
  next.fan = byRhythm || bySaturation;
  if (next.fan != outputs.state().fan) {
    const bool saturationCause = next.fan ? bySaturation && !byRhythm : fanBySaturation;
    Serial.printf("VENTILATION %s%s\r\n", next.fan ? "marche" : "arret",
                  saturationCause ? " (saturation)" : "");
  }
  fanBySaturation = bySaturation && !byRhythm;
  // TODO(phase 4): misting pulses on airMeasure's humidity when
  // policy.mistAllowed and health is kOk (2026-007, decision 03).
  outputs.apply(next);
}

// One line per measurement cycle, for the commissioning checks of the notice:
// the air measure, then each probe. Missing or rejected readings print as "--".
void reportMeasures(const terrarium::sensors::SubstrateReading& substrate) {
  Serial.print("MESURE air=");
  if (const char* source = terrarium::control::sourceName(airMeasure.source)) {
    Serial.printf("%.1fC hr=%.0f%% (%s)", airMeasure.temperatureC, airMeasure.humidityRhPercent,
                  source);
  } else {
    Serial.print("-- hr=--");
  }
  for (uint8_t i = 0; i < kProbeCount; ++i) {
    Serial.printf(" %s=", terrarium::control::probeName(static_cast<Probe>(i)));
    if (probeSamples[i].rejection == AirRejection::kNone) {
      Serial.printf("%.1fC/%.0f%%", probeSamples[i].temperatureC,
                    probeSamples[i].humidityRhPercent);
    } else {
      Serial.print("--");
    }
  }
  Serial.print(" substrat=");
  if (substrate.valid) {
    Serial.printf("%.1fC", substrate.temperatureC);
  } else {
    Serial.print("--");
  }
  Serial.printf(" chauffage=%d mode=%s", outputs.state().heater ? 1 : 0,
                terrarium::control::toString(modeMachine.mode()));
  if (timeKnown) {
    const uint16_t minute = minuteOfDay();
    Serial.printf(" heure=%02u:%02u\r\n", minute / 60U, minute % 60U);
  } else {
    Serial.print(" heure=--\r\n");
  }
}

// Hands the cycle's state to the supervision task, which publishes it when
// the broker is reachable (RG-topics-mqtt). Never waits for the network.
void publishState(const terrarium::sensors::SubstrateReading& substrate, uint32_t nowMs) {
  const terrarium::outputs::OutputState& out = outputs.state();
  terrarium::supervision::StateSnapshot snapshot{};
  // A rejected reading is never published (RG-topics-mqtt).
  snapshot.airValid = airMeasure.source != AirSource::kNone;
  snapshot.airSource = airMeasure.source;
  snapshot.airRejection = airMeasure.cause;
  snapshot.airSafety = airSafety;
  snapshot.airTemperatureC = airMeasure.temperatureC;
  snapshot.airHumidityRhPercent = airMeasure.humidityRhPercent;
  snapshot.combination = probeSetup.combination;
  for (uint8_t i = 0; i < kProbeCount; ++i) {
    snapshot.probes[i] = {probeSetup.roles[i], probeSamples[i].rejection,
                          probeSamples[i].temperatureC, probeSamples[i].humidityRhPercent,
                          probeInFault(static_cast<Probe>(i))};
  }
  snapshot.substrateValid = substrate.valid;
  snapshot.substrateTemperatureC = substrate.temperatureC;
  snapshot.heater = out.heater;
  snapshot.light = out.light;
  snapshot.mistPump = out.mistPump;
  snapshot.fan = out.fan;
  snapshot.mode = modeMachine.mode();
  snapshot.setpointC = heatSettings.setpointC;
  snapshot.hysteresisC = heatSettings.hysteresisC;
  snapshot.sensorFault = airWatch.health() == SensorHealth::kFault;
  snapshot.heatPaused = heatGuard.paused();
  snapshot.substrateCut = substrateCap.cut();
  snapshot.uptimeS = nowMs / 1000;
  snapshot.firmwareVersion = FIRMWARE_VERSION;
  snapshot.windows = timeWindows;
  std::memcpy(snapshot.timezone, timeZone, sizeof(snapshot.timezone));
  snapshot.timeKnown = timeKnown;
  snapshot.inWindow = timeKnown && terrarium::control::inAnyWindow(timeWindows, minuteOfDay());
  snapshot.bounds = kHeatBounds;
  snapshot.nightHeat = nightHeat;
  snapshot.dayNight = dayNight;
  snapshot.today = terrarium::control::dayFor(dayNight, timeKnown ? dayOfYearNow() : 0);
  snapshot.moment = moment;
  snapshot.fanSettings = fanSettings;
  snapshot.saturationRunning = saturationVent.running();
  snapshot.fanBounds = kFanBounds;
  supervisor.publish(snapshot);
}

// Restores the last saved mode, or stays in MAINTENANCE (RG-mode-au-demarrage).
void restoreMode() {
  uint8_t stored = 0;
  Mode mode = kFallbackBootMode;
  if (settingsStore.loadMode(stored) && terrarium::control::modeFromStored(stored, mode)) {
    modeMachine.request(mode);
  } else {
    Serial.println("INFO aucun mode enregistre, demarrage en MAINTENANCE");
  }
  Serial.printf("MODE %s au demarrage\r\n", terrarium::control::toString(modeMachine.mode()));
}

// Switches mode on an explicit command, from the serial link or MQTT
// (RG-changement-de-mode-manuel), saves it, and reports it on both.
void applyMode(Mode target) {
  using terrarium::supervision::ModeOutcome;
  // Each reply differs from the command typed, so that a terminal echoing the
  // command still shows that the board answered.
  const char* name = terrarium::control::toString(target);
  ModeOutcome outcome = ModeOutcome::kApplied;
  const bool changed = modeMachine.request(target);
  // A mode change restarts the ventilation count (RG-ventilation-periodique).
  if (changed) fanScheduler.restart(millis());
  if (!changed) {
    outcome = ModeOutcome::kAlreadyActive;
    Serial.printf("MODE %s deja actif\r\n", name);
  } else if (settingsStore.saveMode(terrarium::control::modeToStored(target))) {
    Serial.printf("MODE %s enregistre, conserve apres coupure\r\n", name);
  } else {
    outcome = ModeOutcome::kNotSaved;
    Serial.printf("ERREUR mode %s applique mais non enregistre, perdu a la prochaine coupure\r\n",
                  name);
  }
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatModeEvent(target, outcome, event)) supervisor.sendEvent(event);
}

// Applies a setpoint and hysteresis received over MQTT, only within the
// bounds of config.h (RG-reglages-bornes); anything else is refused whole.
void applyHeatSettings(const HeatSettings& candidate) {
  const bool accepted = terrarium::control::isAcceptedHeatSettings(candidate, kHeatBounds);
  if (accepted) {
    heatSettings = candidate;
    activeHeating.retune(heatSettings.setpointC, heatSettings.hysteresisC);
    if (settingsStore.saveHeatSettings(heatSettings)) {
      Serial.printf("CONSIGNE %.1fC hysteresis %.1fC enregistree\r\n", heatSettings.setpointC,
                    heatSettings.hysteresisC);
    } else {
      Serial.printf("ERREUR consigne %.1fC appliquee mais non enregistree\r\n",
                    heatSettings.setpointC);
    }
  } else {
    Serial.printf(
        "REFUS consigne %.1fC hysteresis %.1fC hors bornes (consigne 15-22C, hysteresis 0.2-2C, "
        "somme sous 24C)\r\n",
        candidate.setpointC, candidate.hysteresisC);
  }
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatSettingsEvent(candidate, accepted, event)) {
    supervisor.sendEvent(event);
  }
}

// Replaces the PLAGES windows (already validated), saves and reports them.
void applyTimeWindows(const TimeWindows& windows) {
  timeWindows = windows;
  char text[terrarium::control::kTimeWindowsTextSize];
  terrarium::control::formatTimeWindows(timeWindows, text);
  if (settingsStore.saveTimeWindows(timeWindows)) {
    Serial.printf("PLAGES %s enregistrees, conservees apres coupure\r\n", text);
  } else {
    Serial.printf("ERREUR plages %s appliquees mais non enregistrees\r\n", text);
  }
  // Re-announce the window state at the next regulation step.
  lastScheduleReport = ScheduleReport::kNone;
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatWindowsEvent(timeWindows, event)) supervisor.sendEvent(event);
}

// Replaces the time zone (already validated), saves and reports it.
void applyTimezone(const char (&candidate)[terrarium::control::kTimezoneSize]) {
  std::memcpy(timeZone, candidate, sizeof(timeZone));
  applyTimezoneToClock();
  if (settingsStore.saveTimezone(timeZone)) {
    Serial.printf("FUSEAU %s enregistre, conserve apres coupure\r\n", timeZone);
  } else {
    Serial.printf("ERREUR fuseau %s applique mais non enregistre\r\n", timeZone);
  }
  lastScheduleReport = ScheduleReport::kNone;
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatTimezoneEvent(timeZone, event)) supervisor.sendEvent(event);
}

// Switches the air sensor's safety on or off, on the owner's command
// (RG-capteur-en-defaut, decision 03 of cadrage 2026-006), saves and reports it.
void applyAirSafety(bool enabled) {
  airSafety = enabled;
  const bool saved = settingsStore.saveAirSafety(enabled);
  if (saved) {
    Serial.printf("SECURITE capteur air %s, conservee apres coupure\r\n",
                  enabled ? "ACTIVE" : "DESACTIVEE");
  } else {
    Serial.printf("ERREUR securite capteur air %s mais non enregistree\r\n",
                  enabled ? "activee" : "desactivee");
  }
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatSafetyEvent(enabled, event, saved)) supervisor.sendEvent(event);
}

void printProbeSetup(const char* suffix) {
  Serial.printf("SONDES sht45=%s dht11=%s combinaison=%s%s\r\n",
                terrarium::control::roleName(probeSetup.roles[0]),
                terrarium::control::roleName(probeSetup.roles[1]),
                terrarium::control::combinationName(probeSetup.combination), suffix);
}

// Replaces the roles and combination (already validated), saves and reports
// them (RG-sondes-d-air). The regulators keep their state: the next
// measurement simply runs on the new air measure.
void applyProbeSetup(const ProbeSetup& next) {
  using terrarium::supervision::SettingOutcome;
  probeSetup = next;
  const bool saved =
      settingsStore.saveProbeSetup(terrarium::control::probeSetupToStored(probeSetup));
  printProbeSetup(saved ? " enregistrees, conservees apres coupure"
                        : " appliquees mais non enregistrees");
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatProbeSetupEvent(
          probeSetup, saved ? SettingOutcome::kApplied : SettingOutcome::kNotSaved, event)) {
    supervisor.sendEvent(event);
  }
}

void printRhythm(const char* name, const terrarium::control::FanRhythm& r) {
  Serial.printf("VENTILATION %s %s duree=%umin periode=%umin ecart=%u%%\r\n", name,
                terrarium::control::fanModeName(r.mode), unsigned{r.durationMin},
                unsigned{r.periodMin}, unsigned{r.spreadPercent});
}

// Every ventilation setting, one line each (RG-ventilation-periodique).
void printFanSettings(const char* suffix) {
  printRhythm("jour", fanSettings.day);
  printRhythm("nuit", fanSettings.night);
  char time[terrarium::control::kClockTimeTextLength + 1] = "--:--";
  terrarium::control::formatClockTime(fanSettings.hibernation.dailyMinute, time);
  Serial.printf("VENTILATION hibernation heure=%s duree=%umin\r\n", time,
                unsigned{fanSettings.hibernation.durationMin});
  const terrarium::control::SaturationOption& sat = fanSettings.saturation;
  Serial.printf(
      "VENTILATION saturation %s %s humidite=%u%% attente=%umin duree=%umin pause=%umin%s\r\n",
      sat.active ? "active" : "inactive", terrarium::control::behaviourName(sat.behaviour),
      unsigned{sat.humidityPercent}, unsigned{sat.waitMin}, unsigned{sat.durationMin},
      unsigned{sat.pauseMin}, suffix);
}

// Today's day, the moment and the setting (RG-jour-et-nuit).
void printDayNight(const char* suffix) {
  char day[terrarium::control::kDayWindowTextLength + 1] = "--:-----:--";
  if (dayNight.mode == terrarium::control::DayNightMode::kMetronome) {
    terrarium::control::formatDayWindow(dayNight.fixed, day);
    Serial.printf("JOUR metronome %s%s\r\n", day, suffix);
  } else {
    Serial.printf("JOUR saisons (%u)%s\r\n", unsigned{dayNight.seasonCount}, suffix);
    for (uint8_t i = 0; i < dayNight.seasonCount; ++i) {
      char start[terrarium::control::kMonthDayTextLength + 1] = "--";
      terrarium::control::formatMonthDay(dayNight.seasons[i].startDay, start);
      terrarium::control::formatDayWindow(dayNight.seasons[i].day, day);
      Serial.printf("JOUR saison %s %s\r\n", start, day);
    }
  }
  if (timeKnown) {
    terrarium::control::formatDayWindow(terrarium::control::dayFor(dayNight, dayOfYearNow()), day);
    Serial.printf("JOUR aujourd'hui %s, %s\r\n", day, terrarium::control::momentName(moment));
  } else {
    Serial.println("JOUR heure inconnue : jour");
  }
}

void sendFieldEvent(const char* field, terrarium::supervision::SettingOutcome outcome) {
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatFieldEvent(field, outcome, event)) supervisor.sendEvent(event);
}

// Replaces the ventilation settings (already within bounds), saves and
// reports them. The next airing takes them (decision 03 of 2026-015).
void applyFanSettings(const FanSettings& next) {
  using terrarium::supervision::SettingOutcome;
  fanSettings = next;
  uint8_t stored[terrarium::control::kFanStoredSize];
  terrarium::control::encodeFanSettings(fanSettings, stored);
  const bool saved = settingsStore.saveFanSettings(stored);
  printFanSettings(saved ? " : enregistree" : " : appliquee mais non enregistree");
  sendFieldEvent("ventilation", saved ? SettingOutcome::kApplied : SettingOutcome::kNotSaved);
}

// Replaces the day and night (already valid), saves and reports it.
void applyDayNight(const DayNight& next) {
  using terrarium::supervision::SettingOutcome;
  dayNight = next;
  uint8_t stored[terrarium::control::kDayNightStoredSize];
  terrarium::control::encodeDayNight(dayNight, stored);
  const bool saved = settingsStore.saveDayNight(stored);
  printDayNight(saved ? " : enregistre" : " : applique mais non enregistre");
  sendFieldEvent("jour_nuit", saved ? SettingOutcome::kApplied : SettingOutcome::kNotSaved);
}

// Replaces the night setpoint (already within bounds), saves and reports it.
void applyNightHeat(const HeatSettings& next) {
  nightHeat = next;
  if (settingsStore.saveNightHeat(nightHeat)) {
    Serial.printf("CONSIGNE nuit %.1fC hysteresis %.1fC enregistree\r\n", nightHeat.setpointC,
                  nightHeat.hysteresisC);
  } else {
    Serial.printf("ERREUR consigne nuit %.1fC appliquee mais non enregistree\r\n",
                  nightHeat.setpointC);
  }
  char event[terrarium::supervision::kEventSize];
  if (terrarium::supervision::formatNightHeatEvent(nightHeat, true, event)) {
    supervisor.sendEvent(event);
  }
}

// A cmd/config, or CONFIG on the serial link: refused whole if it would leave
// no interior probe, or if its ventilation, night or day heat part is out of
// bounds (RG-reglages-bornes); windows, time zone and day and night were
// validated by the parser.
void applyConfig(const terrarium::supervision::ConfigCommand& config) {
  const bool hasProbes = config.hasRole[0] || config.hasRole[1] || config.hasCombination;
  ProbeSetup nextProbes = probeSetup;
  for (uint8_t i = 0; i < kProbeCount; ++i) {
    if (config.hasRole[i]) nextProbes.roles[i] = config.roles[i];
  }
  if (config.hasCombination) nextProbes.combination = config.combination;
  if (hasProbes && !terrarium::control::isValidProbeSetup(nextProbes)) {
    Serial.println("REFUS sondes : au moins une sonde doit rester interieure");
    char event[terrarium::supervision::kEventSize];
    if (terrarium::supervision::formatProbeSetupEvent(
            nextProbes, terrarium::supervision::SettingOutcome::kNoInteriorProbe, event)) {
      supervisor.sendEvent(event);
    }
    return;
  }
  const FanSettings nextFan = terrarium::control::merged(fanSettings, config.fan);
  if (config.hasFan && !terrarium::control::isAcceptedFanSettings(nextFan, kFanBounds)) {
    Serial.println("REFUS ventilation : hors bornes, ou une duree pas sous sa periode");
    sendFieldEvent("ventilation", terrarium::supervision::SettingOutcome::kInvalidFan);
    return;
  }
  HeatSettings nextNight = nightHeat;
  if (config.nightHeat.hasSetpoint) nextNight.setpointC = config.nightHeat.setpointC;
  if (config.nightHeat.hasHysteresis) nextNight.hysteresisC = config.nightHeat.hysteresisC;
  if (config.hasNightHeat && !terrarium::control::isAcceptedHeatSettings(nextNight, kHeatBounds)) {
    Serial.printf("REFUS consigne nuit %.1fC hysteresis %.1fC hors bornes\r\n", nextNight.setpointC,
                  nextNight.hysteresisC);
    char event[terrarium::supervision::kEventSize];
    if (terrarium::supervision::formatNightHeatEvent(nextNight, false, event)) {
      supervisor.sendEvent(event);
    }
    return;
  }
  if (config.hasHeat) {
    const bool accepted = terrarium::control::isAcceptedHeatSettings(config.heat, kHeatBounds);
    applyHeatSettings(config.heat);  // Applies it, or reports the refusal.
    if (!accepted) return;
  }
  if (config.hasWindows) applyTimeWindows(config.windows);
  if (config.hasTimezone) applyTimezone(config.timezone);
  if (config.hasAirSafety) applyAirSafety(config.airSafety);
  if (hasProbes) applyProbeSetup(nextProbes);
  if (config.hasFan) applyFanSettings(nextFan);
  if (config.hasDayNight) applyDayNight(config.dayNight);
  if (config.hasNightHeat) applyNightHeat(nextNight);
}

// Commands received over MQTT, applied here, in the regulation loop.
void pollRemoteCommands() {
  terrarium::network::RemoteCommand command{};
  while (supervisor.takeCommand(command)) {
    if (command.kind == terrarium::network::RemoteCommand::Kind::kMode) {
      applyMode(command.mode);
    } else {
      applyConfig(command.config);
    }
  }
}

// Restores the saved windows and time zone if valid, else the firmware's
// (RG-mode-au-demarrage), and sets the clock's time zone.
void restoreSchedule() {
  TimeWindows stored{};
  timeWindows = defaultTimeWindows();
  if (settingsStore.loadTimeWindows(stored)) {
    if (terrarium::control::isValidTimeWindows(stored)) {
      timeWindows = stored;
    } else {
      Serial.println("INFO plages enregistrees invalides, plages du firmware");
    }
  }
  std::memcpy(timeZone, cfg::kDefaultTimezone, sizeof(cfg::kDefaultTimezone));
  char storedZone[terrarium::control::kTimezoneSize] = {};
  if (settingsStore.loadTimezone(storedZone)) {
    if (terrarium::control::isValidPosixTimezone(storedZone)) {
      std::memcpy(timeZone, storedZone, sizeof(timeZone));
    } else {
      Serial.println("INFO fuseau enregistre invalide, fuseau du firmware");
    }
  }
  applyTimezoneToClock();
  char text[terrarium::control::kTimeWindowsTextSize];
  terrarium::control::formatTimeWindows(timeWindows, text);
  Serial.printf("PLAGES %s au demarrage, fuseau %s\r\n", text, timeZone);
}

void printSchedule() {
  char text[terrarium::control::kTimeWindowsTextSize];
  terrarium::control::formatTimeWindows(timeWindows, text);
  if (timeKnown) {
    const uint16_t minute = minuteOfDay();
    Serial.printf("PLAGES actuelles %s, fuseau %s, heure %02u:%02u\r\n", text, timeZone,
                  minute / 60U, minute % 60U);
  } else {
    Serial.printf("PLAGES actuelles %s, fuseau %s, heure inconnue\r\n", text, timeZone);
  }
}

// PLAGES and FUSEAU on the serial link. Returns false if `line` is neither.
bool handleScheduleLine(const char* line) {
  TimeWindows windows{};
  switch (terrarium::control::parseWindowsCommand(line, windows)) {
    case SettingCommand::kShow:
      printSchedule();
      return true;
    case SettingCommand::kSet:
      applyTimeWindows(windows);
      return true;
    case SettingCommand::kInvalid:
      Serial.println(
          "REFUS plages : 4 au plus, au format HH:MM-HH:MM separees par une espace, debut "
          "different de la fin, ou AUCUNE");
      return true;
    case SettingCommand::kNone:
      break;
  }
  char zone[terrarium::control::kTimezoneSize] = {};
  switch (terrarium::control::parseTimezoneCommand(line, zone)) {
    case SettingCommand::kShow:
      printSchedule();
      return true;
    case SettingCommand::kSet:
      applyTimezone(zone);
      return true;
    case SettingCommand::kInvalid:
      Serial.println(
          "REFUS fuseau : chaine POSIX TZ attendue, par exemple CET-1CEST,M3.5.0,M10.5.0/3");
      return true;
    case SettingCommand::kNone:
      break;
  }
  return false;
}

// SONDES, SONDE and COMBINAISON on the serial link. Returns false if `line` is
// none of them.
bool handleProbeLine(const char* line) {
  ProbeSetup next = probeSetup;
  switch (terrarium::control::parseProbeCommand(line, probeSetup, next)) {
    case SettingCommand::kShow:
      printProbeSetup(" actuelles");
      return true;
    case SettingCommand::kSet:
      applyProbeSetup(next);
      return true;
    case SettingCommand::kInvalid:
      Serial.println(
          "REFUS sondes : SONDE <sht45|dht11> <INTERIEURE|EXTERIEURE> ou COMBINAISON "
          "<priorite-sht45|priorite-dht11|moyenne|plus-froide>, au moins une sonde interieure");
      return true;
    case SettingCommand::kNone:
      break;
  }
  return false;
}

// CONFIG <json>, JOUR and VENTILATION on the serial link (decision 05 of
// 2026-015). Returns false if `line` is none of them.
bool handleSettingsLine(const char* line) {
  if (terrarium::control::isShowCommand(line, "VENTILATION")) {
    printFanSettings(" actuelle");
    return true;
  }
  if (terrarium::control::isShowCommand(line, "JOUR")) {
    printDayNight(" actuel");
    return true;
  }
  size_t length = 0;
  const char* json = terrarium::control::configPayload(line, length);
  if (json == nullptr) return false;
  terrarium::supervision::ConfigCommand config{};
  const terrarium::supervision::ConfigError error = terrarium::supervision::parseConfigPayload(
      reinterpret_cast<const uint8_t*>(json), length, heatSettings, config);
  if (error != terrarium::supervision::ConfigError::kNone) {
    Serial.printf("REFUS CONFIG : %s\r\n", terrarium::supervision::describe(error));
    return true;
  }
  applyConfig(config);
  return true;
}

// Restores the saved ventilation, day and night, and night setpoint if within
// the bounds of this firmware, else keeps the firmware's (RG-mode-au-demarrage).
void restoreFanSettings() {
  uint8_t stored[terrarium::control::kFanStoredSize];
  FanSettings decoded{};
  if (settingsStore.loadFanSettings(stored)) {
    if (terrarium::control::decodeFanSettings(stored, decoded) &&
        terrarium::control::isAcceptedFanSettings(decoded, kFanBounds)) {
      fanSettings = decoded;
    } else {
      Serial.println("INFO ventilation enregistree illisible ou hors bornes, valeurs du firmware");
    }
  }
  printFanSettings(" au demarrage");
  uint8_t storedDay[terrarium::control::kDayNightStoredSize];
  DayNight decodedDay{};
  if (settingsStore.loadDayNight(storedDay)) {
    if (terrarium::control::decodeDayNight(storedDay, decodedDay) &&
        terrarium::control::isValidDayNight(decodedDay)) {
      dayNight = decodedDay;
    } else {
      Serial.println("INFO jour et nuit enregistres illisibles, ceux du firmware");
    }
  }
  printDayNight(" au demarrage");
  HeatSettings storedNight{};
  if (settingsStore.loadNightHeat(storedNight)) {
    if (terrarium::control::isAcceptedHeatSettings(storedNight, kHeatBounds)) {
      nightHeat = storedNight;
    } else {
      Serial.println("INFO consigne de nuit enregistree hors bornes, valeurs du firmware");
    }
  }
  Serial.printf("CONSIGNE nuit %.1fC hysteresis %.1fC au demarrage\r\n", nightHeat.setpointC,
                nightHeat.hysteresisC);
}

// Restores the saved roles and combination if valid, else the firmware's
// (RG-mode-au-demarrage): the SHT45 alone interior.
void restoreProbeSetup() {
  uint8_t stored = 0;
  if (settingsStore.loadProbeSetup(stored) &&
      !terrarium::control::probeSetupFromStored(stored, probeSetup)) {
    Serial.println("INFO sondes enregistrees invalides, sondes du firmware");
  }
  printProbeSetup(" au demarrage");
}

// Restores the saved setpoint and hysteresis if they are within the bounds of
// this firmware, else keeps its defaults (RG-mode-au-demarrage).
void restoreHeatSettings() {
  HeatSettings stored{};
  if (!settingsStore.loadHeatSettings(stored)) return;
  if (!terrarium::control::isAcceptedHeatSettings(stored, kHeatBounds)) {
    Serial.println("INFO consigne enregistree hors bornes, valeurs du firmware");
    return;
  }
  heatSettings = stored;
  activeHeating.retune(heatSettings.setpointC, heatSettings.hysteresisC);
  Serial.printf("CONSIGNE %.1fC hysteresis %.1fC au demarrage\r\n", heatSettings.setpointC,
                heatSettings.hysteresisC);
}

// Restores the owner's choice for the air sensor's safety, on by default, and
// says so when it is off.
void restoreAirSafety() {
  bool stored = true;
  if (settingsStore.loadAirSafety(stored)) airSafety = stored;
  if (!airSafety) Serial.println("ATTENTION securite capteur air DESACTIVEE");
}

// "licence <jeton>": the Skaron licence, never printed back
// (RG-mise-a-jour-a-distance). Returns false if `line` is not this command.
bool handleLicenceLine(const char* line) {
  static char licence[terrarium::update::kLicenceSize];
  switch (terrarium::update::parseLicenceLine(line, licence)) {
    case terrarium::update::LicenceLine::kNone:
      return false;
    case terrarium::update::LicenceLine::kInvalid:
      Serial.println("REFUS licence : jeton vide, trop long ou invalide");
      return true;
    case terrarium::update::LicenceLine::kValid:
      supervisor.setUpdateLicence(licence);  // Prints and publishes the outcome.
      std::memset(licence, 0, sizeof(licence));
      return true;
  }
  return false;
}

void handleSerialLine(const char* line) {
  if (handleLicenceLine(line)) return;
  if (terrarium::control::isWifiPortalCommand(line)) {
    supervisor.requestPortal();
    Serial.println("WIFI ouverture du reseau de configuration demandee");
    return;
  }
  if (handleScheduleLine(line)) return;
  if (handleProbeLine(line)) return;
  if (handleSettingsLine(line)) return;
  Mode target = modeMachine.mode();
  if (!terrarium::control::parseModeCommand(line, target)) {
    Serial.println(
        "ERREUR commande inconnue (MODE ACTIF|PLAGES|HIBERNATION|MAINTENANCE, PLAGES, FUSEAU, "
        "SONDES, SONDE, COMBINAISON, VENTILATION, JOUR, CONFIG <json>, WIFI CONFIGURER, LICENCE)");
    return;
  }
  applyMode(target);
}

// Reads the serial link without blocking, one line at a time. A line ends
// on LF or CR: browser consoles send CR alone on Enter. Over-long lines are
// dropped whole.
void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c != '\n' && c != '\r') {
      if (serialLineLength + 1 < kSerialLineSize) {
        serialLine[serialLineLength++] = c;
      } else {
        serialLineOverflow = true;
      }
      continue;
    }
    serialLine[serialLineLength] = '\0';
    if (serialLineOverflow) {
      Serial.println("ERREUR ligne trop longue");
    } else if (serialLineLength > 0) {
      handleSerialLine(serialLine);
    }
    std::memset(serialLine, 0, serialLineLength);  // A licence does not linger.
    serialLineLength = 0;
    serialLineOverflow = false;
  }
}

}  // namespace

void setup() {
  // SAFETY FIRST: drive every power output (pump GPIO 25, heater GPIO 26,
  // light GPIO 27, fan GPIO 33) to its off level before anything else runs.
  // If any later initialisation hangs, crashes or reboots the board, nothing
  // is powered. Between reset and this line the pins float: the relay module
  // must be wired so that a floating input leaves its relay off (cadrage,
  // relay safeguards).
  pinMode(cfg::kPinFanRelay, OUTPUT);
  digitalWrite(cfg::kPinFanRelay, cfg::kOutputOffLevel);
  pinMode(cfg::kPinMistPump, OUTPUT);
  digitalWrite(cfg::kPinMistPump, cfg::kOutputOffLevel);
  pinMode(cfg::kPinHeaterRelay, OUTPUT);
  digitalWrite(cfg::kPinHeaterRelay, cfg::kOutputOffLevel);
  pinMode(cfg::kPinLightRelay, OUTPUT);
  digitalWrite(cfg::kPinLightRelay, cfg::kOutputOffLevel);

  outputs.begin();
  pinMode(cfg::kPinStatusLed, OUTPUT);
  digitalWrite(cfg::kPinStatusLed, LOW);
  Serial.begin(115200);
  // Separates the firmware's own lines from the chip's boot messages above.
  Serial.println();
  Serial.println("=== ssk-terrarium " FIRMWARE_VERSION " : demarrage ===");

  startWatchdog();

  Wire.begin(cfg::kPinI2cSda, cfg::kPinI2cScl);
  if (!airSensor.begin()) Serial.println("ABSENT capteur air");
  if (!substrateSensor.begin()) Serial.println("ABSENTE sonde substrat (optionnelle)");
  dhtSensor.begin();  // Optional: an absent DHT11 reads as a silent probe.

  restoreMode();
  restoreHeatSettings();
  restoreAirSafety();
  restoreProbeSetup();
  restoreSchedule();
  restoreFanSettings();
  // Periods count from the boot: the first one begins with an airing.
  fanScheduler.restart(millis());

  // Supervision: started last, in its own task, and none of it is awaited.
  supervisor.begin(FIRMWARE_VERSION, UPDATE_PUBLIC_KEY);
  notifier.begin();

  Serial.println("=== pret : regulation en cours ===");
}

void loop() {
  esp_task_wdt_reset();

  const uint32_t nowMs = millis();
  if (!firstReadDone || nowMs - lastSensorReadMs >= cfg::kSensorPeriodMs) {
    firstReadDone = true;
    lastSensorReadMs = nowMs;
    const terrarium::sensors::AirReading air[kProbeCount] = {airSensor.read(), dhtSensor.read()};
    const terrarium::sensors::SubstrateReading substrate = substrateSensor.read();
    regulate(air, substrate, nowMs);
    reportMeasures(substrate);
    publishState(substrate, nowMs);
    heatAirSensorIfDue(air[static_cast<uint8_t>(Probe::kSht45)], nowMs);
  }

  pollTimeSync();
  pollSerial();
  pollRemoteCommands();
  // A new firmware validates itself, or returns to the old one
  // (RG-retour-arriere-automatique).
  supervisor.watchUpdate(nowMs, airMeasure.source != AirSource::kNone);
  notifier.loop();
  delay(10);
}
