// ssk-terrarium firmware entry point.
//
// The control path (sensors -> regulator -> outputs) runs entirely on the ESP32
// and never waits for the network. MQTT, web and alerts are supervision only.

#include <Arduino.h>
#include <Wire.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>

#include "air_sensor.h"
#include "alerts.h"
#include "commands.h"
#include "config.h"
#include "heat_guard.h"
#include "heat_settings.h"
#include "hysteresis_regulator.h"
#include "mode_machine.h"
#include "outputs.h"
#include "sensor_watch.h"
#include "serial_command.h"
#include "settings_store.h"
#include "state_message.h"
#include "substrate_sensor.h"
#include "supervisor.h"

// Set by outils/version_firmware.py from `git describe`.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "inconnue"
#endif

namespace cfg = terrarium::config;
using terrarium::control::HeatGuard;
using terrarium::control::HeatingPolicy;
using terrarium::control::HeatSettings;
using terrarium::control::HysteresisRegulator;
using terrarium::control::Mode;
using terrarium::control::ModeMachine;
using terrarium::control::SensorHealth;
using terrarium::control::SensorWatch;

namespace {

// Mode at power-up when NVS holds no valid one (RG-mode-au-demarrage).
constexpr Mode kFallbackBootMode = Mode::kMaintenance;

// Longest serial command line, terminator included.
constexpr size_t kSerialLineSize = 32;

// All objects are static: nothing is allocated once loop() runs.
terrarium::outputs::Outputs outputs({cfg::kPinHeaterRelay, cfg::kPinLightRelay, cfg::kPinMistPump,
                                     cfg::kPinFanRelay, cfg::kOutputOffLevel});
terrarium::sensors::AirSensor airSensor;
terrarium::sensors::SubstrateSensor substrateSensor(cfg::kPinOneWire);
terrarium::storage::SettingsStore settingsStore;
terrarium::network::Supervisor supervisor(settingsStore);
terrarium::alerts::Notifier notifier;

ModeMachine modeMachine(kFallbackBootMode);
SensorWatch airWatch(cfg::kSensorSilentMaxMs);
HeatGuard heatGuard(cfg::kHeatContinuousMaxMs, cfg::kHeatPauseMs);
// Bounds of the settings received from the network (RG-reglages-bornes).
constexpr terrarium::control::HeatSettingsBounds kHeatBounds{
    cfg::kSetpointMinC, cfg::kSetpointMaxC, cfg::kHeatHysteresisMinC, cfg::kHeatHysteresisMaxC,
    cfg::kAlertTempHighC};
HeatSettings heatSettings{cfg::kSetpointC, cfg::kHeatHysteresisC};
HysteresisRegulator activeHeating =
    HysteresisRegulator::aroundSetpoint(cfg::kSetpointC, cfg::kHeatHysteresisC);
HysteresisRegulator frostHeating(cfg::kFrostHeatOnBelowC, cfg::kFrostHeatOffAtC);

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

// One regulation step. Misting, light and fan stay off in phase 1
// (RG-sorties-par-mode); an air sensor in fault switches heating off, and so
// does heating for too long (RG-chauffe-continue-bornee).
void regulate(const terrarium::sensors::AirReading& air, uint32_t nowMs) {
  const SensorHealth previous = airWatch.health();
  const SensorHealth health =
      airWatch.update(nowMs, air.valid && terrarium::control::isPlausibleAirReading(
                                              air.temperatureC, air.humidityRhPercent));
  if (health != previous && (health == SensorHealth::kFault || previous == SensorHealth::kFault)) {
    Serial.println(health == SensorHealth::kFault ? "DEFAUT capteur air" : "REPRISE capteur air");
  }

  const terrarium::control::ModePolicy policy = modeMachine.policy();
  terrarium::outputs::OutputState next{false, false, false, false};

  switch (policy.heating) {
    case HeatingPolicy::kSetpoint:
      frostHeating.reset();
      next.heater = heaterFrom(activeHeating, health, air.temperatureC);
      break;
    case HeatingPolicy::kFrostProtection:
      activeHeating.reset();
      next.heater = heaterFrom(frostHeating, health, air.temperatureC);
      break;
    case HeatingPolicy::kOff:
      activeHeating.reset();
      frostHeating.reset();
      break;
  }

  const bool wasPaused = heatGuard.paused();
  next.heater = heatGuard.filter(nowMs, next.heater);
  if (heatGuard.paused() != wasPaused) {
    Serial.println(heatGuard.paused() ? "PAUSE chauffage apres 2 h continues"
                                      : "REPRISE chauffage");
  }
  const bool fault = health == SensorHealth::kFault || heatGuard.paused();
  digitalWrite(cfg::kPinStatusLed, fault ? HIGH : LOW);

  // TODO(phase 2): light from the NTP time when policy.lightAllowed.
  // TODO(phase 2): periodic fan cycles in ACTIF (kFanCyclePeriodMs).
  // TODO(phase 4): misting pulses when policy.mistAllowed and air.valid.
  outputs.apply(next);
}

// One line per measurement cycle, for the commissioning checks of the notice.
// Invalid readings print as "--".
void reportMeasures(const terrarium::sensors::AirReading& air,
                    const terrarium::sensors::SubstrateReading& substrate) {
  Serial.print("MESURE air=");
  if (air.valid) {
    Serial.printf("%.1fC hr=%.0f%%", air.temperatureC, air.humidityRhPercent);
  } else {
    Serial.print("-- hr=--");
  }
  Serial.print(" substrat=");
  if (substrate.valid) {
    Serial.printf("%.1fC", substrate.temperatureC);
  } else {
    Serial.print("--");
  }
  Serial.printf(" chauffage=%d mode=%s\r\n", outputs.state().heater ? 1 : 0,
                terrarium::control::toString(modeMachine.mode()));
}

// Hands the cycle's state to the supervision task, which publishes it when
// the broker is reachable (RG-topics-mqtt). Never waits for the network.
void publishState(const terrarium::sensors::AirReading& air,
                  const terrarium::sensors::SubstrateReading& substrate, uint32_t nowMs) {
  const terrarium::outputs::OutputState& out = outputs.state();
  terrarium::supervision::StateSnapshot snapshot{};
  snapshot.airValid = air.valid;
  snapshot.airTemperatureC = air.temperatureC;
  snapshot.airHumidityRhPercent = air.humidityRhPercent;
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
  snapshot.uptimeS = nowMs / 1000;
  snapshot.firmwareVersion = FIRMWARE_VERSION;
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
  if (!modeMachine.request(target)) {
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

// Commands received over MQTT, applied here, in the regulation loop.
void pollRemoteCommands() {
  terrarium::network::RemoteCommand command{};
  while (supervisor.takeCommand(command)) {
    if (command.kind == terrarium::network::RemoteCommand::Kind::kMode) {
      applyMode(command.mode);
    } else {
      applyHeatSettings(command.settings);
    }
  }
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

void handleSerialLine(const char* line) {
  if (terrarium::control::isWifiPortalCommand(line)) {
    supervisor.requestPortal();
    Serial.println("WIFI ouverture du reseau de configuration demandee");
    return;
  }
  Mode target = modeMachine.mode();
  if (!terrarium::control::parseModeCommand(line, target)) {
    Serial.println(
        "ERREUR commande inconnue (MODE ACTIF|HIBERNATION|MAINTENANCE, WIFI CONFIGURER)");
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

  restoreMode();
  restoreHeatSettings();

  // Supervision: started last, in its own task, and none of it is awaited.
  supervisor.begin();
  notifier.begin();

  Serial.println("=== pret : regulation en cours ===");
}

void loop() {
  esp_task_wdt_reset();

  const uint32_t nowMs = millis();
  if (!firstReadDone || nowMs - lastSensorReadMs >= cfg::kSensorPeriodMs) {
    firstReadDone = true;
    lastSensorReadMs = nowMs;
    const terrarium::sensors::AirReading air = airSensor.read();
    regulate(air, nowMs);
    const terrarium::sensors::SubstrateReading substrate = substrateSensor.read();
    reportMeasures(air, substrate);
    publishState(air, substrate, nowMs);
  }

  pollSerial();
  pollRemoteCommands();
  notifier.loop();
  delay(10);
}
