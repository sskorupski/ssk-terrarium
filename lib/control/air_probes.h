// The two air probes, their roles, and the one air measure the regulation runs
// on (RG-sondes-d-air, cadrage 2026-007). Pure logic: no hardware, no
// allocation.
#pragma once

#include <cstdint>

#include "sensor_watch.h"
#include "serial_command.h"

namespace terrarium::control {

// Named after their component, as published on MQTT.
enum class Probe : uint8_t { kSht45 = 0, kDht11 = 1 };
constexpr uint8_t kProbeCount = 2;

enum class ProbeRole : uint8_t {
  kInterior = 0,  // Measures the air of the tank: takes part in the regulation.
  kExterior = 1,  // Measures the garage: published only.
};

// How interior probes make one air measure when more than one is accepted.
enum class ProbeCombination : uint8_t {
  kPrioritySht45 = 0,  // The SHT45 if accepted, else the DHT11.
  kPriorityDht11 = 1,  // The DHT11 if accepted, else the SHT45.
  kAverage = 2,        // Mean temperature and mean humidity of the accepted ones.
  kColdest = 3,        // The coldest accepted probe, temperature and humidity together.
};
constexpr uint8_t kCombinationCount = 4;

struct ProbeSetup {
  ProbeRole roles[kProbeCount];
  ProbeCombination combination;
};

// The SHT45 alone is interior: the regulation of before cadrage 2026-007.
constexpr ProbeSetup kDefaultProbeSetup{{ProbeRole::kInterior, ProbeRole::kExterior},
                                        ProbeCombination::kPrioritySht45};

bool operator==(const ProbeSetup& a, const ProbeSetup& b);

// Roles and combination in range, and at least one interior probe
// (RG-reglages-bornes).
bool isValidProbeSetup(const ProbeSetup& setup);

// Wire names (RG-topics-mqtt).
const char* probeName(Probe probe);                         // "sht45", "dht11"
const char* roleName(ProbeRole role);                       // "interieure", "exterieure"
const char* combinationName(ProbeCombination combination);  // "priorite-sht45", ...
// Case-insensitive. Return false and leave `out` untouched on anything else.
bool parseProbe(const char* text, Probe& out);
bool parseRole(const char* text, ProbeRole& out);
bool parseCombination(const char* text, ProbeCombination& out);

// One byte in NVS (RG-mode-au-demarrage). fromStored returns false, leaving
// `out` untouched, for a byte that does not decode to a valid setup.
uint8_t probeSetupToStored(const ProbeSetup& setup);
bool probeSetupFromStored(uint8_t stored, ProbeSetup& out);

// The interior probe that decides first: the one a priority combination names,
// the SHT45 for the others, or the other probe if that one is exterior
// (RG-capteur-en-defaut). `setup` must be valid.
Probe priorityInteriorProbe(const ProbeSetup& setup);

// One probe's reading this cycle, already classified (classifyAirReading).
struct ProbeSample {
  AirRejection rejection;
  float temperatureC;
  float humidityRhPercent;
};

// Where the air measure comes from, as published in "sonde_retenue".
enum class AirSource : uint8_t { kNone, kSht45, kDht11, kAverage };
const char* sourceName(AirSource source);  // nullptr for kNone.

struct AirMeasure {
  // kNone when no interior probe is accepted.
  AirSource source;
  // The measure when `source` is set. Otherwise the rejected reading of the
  // first interior probe, in priority order, that answered: what a disabled
  // safety regulates on (RG-capteur-en-defaut).
  float temperatureC;
  float humidityRhPercent;
  // What the interior air watch is fed: kNone when `source` is set, else the
  // rejection behind temperatureC, or kSilent when every interior probe is.
  AirRejection watchRejection;
  // Published "cause_mesure_air": kNone when `source` is set, else the cause
  // of the priority interior probe.
  AirRejection cause;
};

// Combines the samples of the interior probes (RG-sondes-d-air). `setup` must
// be valid; exterior samples are ignored.
AirMeasure combineAir(const ProbeSetup& setup, const ProbeSample (&samples)[kProbeCount]);

// Serial link: "SONDES" shows, "SONDE <sonde> <INTERIEURE|EXTERIEURE>" and
// "COMBINAISON <combinaison>" set, case-insensitive, optional trailing CR.
// kSet writes `current` changed into `out`; kInvalid, for a bad value or a
// setup left without an interior probe, leaves `out` untouched.
SettingCommand parseProbeCommand(const char* line, const ProbeSetup& current, ProbeSetup& out);

}  // namespace terrarium::control
