#include "air_probes.h"

#include <cctype>
#include <cstddef>
#include <cstring>

namespace terrarium::control {

namespace {

constexpr const char* kProbeNames[kProbeCount] = {"sht45", "dht11"};
constexpr const char* kRoleNames[] = {"interieure", "exterieure"};
constexpr uint8_t kRoleCount = 2;
constexpr const char* kCombinationNames[kCombinationCount] = {"priorite-sht45", "priorite-dht11",
                                                              "moyenne", "plus-froide"};

constexpr char kShowKeyword[] = "SONDES";
constexpr char kRoleKeyword[] = "SONDE";
constexpr char kCombinationKeyword[] = "COMBINAISON";
// Longest word of a serial probe command: "priorite-sht45" is 14.
constexpr size_t kMaxWordLength = 15;

// NVS byte: bit 0 role of the SHT45, bit 1 role of the DHT11, bits 2-3 the
// combination, bits 4-7 zero.
constexpr uint8_t kStoredUnusedMask = 0xF0;

bool equalsIgnoreCase(const char* a, const char* b) {
  for (; *a != '\0' && *b != '\0'; ++a, ++b) {
    if (std::tolower(static_cast<unsigned char>(*a)) !=
        std::tolower(static_cast<unsigned char>(*b))) {
      return false;
    }
  }
  return *a == '\0' && *b == '\0';
}

// Index of `text` among `names`, case-insensitive, or -1.
int indexOf(const char* text, const char* const* names, uint8_t count) {
  if (text == nullptr) return -1;
  for (uint8_t i = 0; i < count; ++i) {
    if (equalsIgnoreCase(text, names[i])) return i;
  }
  return -1;
}

bool isInterior(const ProbeSetup& setup, Probe probe) {
  return setup.roles[static_cast<uint8_t>(probe)] == ProbeRole::kInterior;
}

Probe other(Probe probe) { return probe == Probe::kSht45 ? Probe::kDht11 : Probe::kSht45; }

AirSource sourceOf(Probe probe) {
  return probe == Probe::kSht45 ? AirSource::kSht45 : AirSource::kDht11;
}

// Splits `line` into at most three space-separated words, without the
// trailing CR. Returns the number of words, or -1 if a word is too long or
// there are more than three.
int splitWords(const char* line, char (&words)[3][kMaxWordLength + 1]) {
  int count = 0;
  size_t length = 0;
  for (const char* c = line;; ++c) {
    const bool end = *c == '\0' || (*c == '\r' && c[1] == '\0');
    if (end || *c == ' ') {
      if (length > 0) {
        words[count][length] = '\0';
        ++count;
        length = 0;
      }
      if (end) return count;
      continue;
    }
    if (count >= 3 || length >= kMaxWordLength) return -1;
    words[count][length++] = *c;
  }
}

}  // namespace

bool operator==(const ProbeSetup& a, const ProbeSetup& b) {
  return a.roles[0] == b.roles[0] && a.roles[1] == b.roles[1] && a.combination == b.combination;
}

bool isValidProbeSetup(const ProbeSetup& setup) {
  bool anyInterior = false;
  for (const ProbeRole role : setup.roles) {
    if (static_cast<uint8_t>(role) >= kRoleCount) return false;
    anyInterior = anyInterior || role == ProbeRole::kInterior;
  }
  return anyInterior && static_cast<uint8_t>(setup.combination) < kCombinationCount;
}

const char* probeName(Probe probe) { return kProbeNames[static_cast<uint8_t>(probe)]; }

const char* roleName(ProbeRole role) {
  return role == ProbeRole::kInterior ? kRoleNames[0] : kRoleNames[1];
}

const char* combinationName(ProbeCombination combination) {
  const auto index = static_cast<uint8_t>(combination);
  return index < kCombinationCount ? kCombinationNames[index] : kCombinationNames[0];
}

bool parseProbe(const char* text, Probe& out) {
  const int index = indexOf(text, kProbeNames, kProbeCount);
  if (index < 0) return false;
  out = static_cast<Probe>(index);
  return true;
}

bool parseRole(const char* text, ProbeRole& out) {
  const int index = indexOf(text, kRoleNames, kRoleCount);
  if (index < 0) return false;
  out = static_cast<ProbeRole>(index);
  return true;
}

bool parseCombination(const char* text, ProbeCombination& out) {
  const int index = indexOf(text, kCombinationNames, kCombinationCount);
  if (index < 0) return false;
  out = static_cast<ProbeCombination>(index);
  return true;
}

uint8_t probeSetupToStored(const ProbeSetup& setup) {
  return static_cast<uint8_t>((static_cast<uint8_t>(setup.roles[0]) & 1U) |
                              ((static_cast<uint8_t>(setup.roles[1]) & 1U) << 1) |
                              ((static_cast<uint8_t>(setup.combination) & 3U) << 2));
}

bool probeSetupFromStored(uint8_t stored, ProbeSetup& out) {
  if ((stored & kStoredUnusedMask) != 0) return false;
  const ProbeSetup decoded{
      {static_cast<ProbeRole>(stored & 1U), static_cast<ProbeRole>((stored >> 1) & 1U)},
      static_cast<ProbeCombination>((stored >> 2) & 3U)};
  if (!isValidProbeSetup(decoded)) return false;
  out = decoded;
  return true;
}

Probe priorityInteriorProbe(const ProbeSetup& setup) {
  const Probe preferred =
      setup.combination == ProbeCombination::kPriorityDht11 ? Probe::kDht11 : Probe::kSht45;
  return isInterior(setup, preferred) ? preferred : other(preferred);
}

const char* sourceName(AirSource source) {
  switch (source) {
    case AirSource::kNone:
      return nullptr;
    case AirSource::kSht45:
      return "sht45";
    case AirSource::kDht11:
      return "dht11";
    case AirSource::kAverage:
      return "moyenne";
  }
  return nullptr;
}

AirMeasure combineAir(const ProbeSetup& setup, const ProbeSample (&samples)[kProbeCount]) {
  const Probe first = priorityInteriorProbe(setup);
  const Probe second = other(first);
  const Probe order[kProbeCount] = {first, second};
  const auto sample = [&samples](Probe probe) -> const ProbeSample& {
    return samples[static_cast<uint8_t>(probe)];
  };
  const auto usable = [&](Probe probe) {
    return isInterior(setup, probe) && sample(probe).rejection == AirRejection::kNone;
  };

  AirMeasure measure{AirSource::kNone, 0.0f, 0.0f, AirRejection::kNone, AirRejection::kNone};
  const auto take = [&measure, &sample](Probe probe) {
    measure.source = sourceOf(probe);
    measure.temperatureC = sample(probe).temperatureC;
    measure.humidityRhPercent = sample(probe).humidityRhPercent;
  };

  if (usable(first) && usable(second)) {
    const ProbeSample& a = sample(Probe::kSht45);
    const ProbeSample& b = sample(Probe::kDht11);
    switch (setup.combination) {
      case ProbeCombination::kPrioritySht45:
      case ProbeCombination::kPriorityDht11:
        take(first);
        break;
      case ProbeCombination::kAverage:
        measure.source = AirSource::kAverage;
        measure.temperatureC = (a.temperatureC + b.temperatureC) / 2.0f;
        measure.humidityRhPercent = (a.humidityRhPercent + b.humidityRhPercent) / 2.0f;
        break;
      case ProbeCombination::kColdest:
        take(b.temperatureC < a.temperatureC ? Probe::kDht11 : Probe::kSht45);
        break;
    }
    return measure;
  }
  for (const Probe probe : order) {
    if (usable(probe)) {
      take(probe);
      return measure;
    }
  }

  // No interior probe accepted: the priority probe's cause is published, and
  // the first interior probe that answered gives what a disabled safety
  // regulates on (RG-capteur-en-defaut).
  measure.cause = sample(first).rejection;
  measure.watchRejection = AirRejection::kSilent;
  for (const Probe probe : order) {
    if (isInterior(setup, probe) && sample(probe).rejection != AirRejection::kSilent) {
      measure.watchRejection = sample(probe).rejection;
      measure.temperatureC = sample(probe).temperatureC;
      measure.humidityRhPercent = sample(probe).humidityRhPercent;
      break;
    }
  }
  return measure;
}

SettingCommand parseProbeCommand(const char* line, const ProbeSetup& current, ProbeSetup& out) {
  if (line == nullptr) return SettingCommand::kNone;
  char words[3][kMaxWordLength + 1] = {};
  const int count = splitWords(line, words);
  if (count <= 0) {
    // Too long or too many words: still ours if it starts with a keyword.
    const bool ours = std::strncmp(line, kRoleKeyword, sizeof(kRoleKeyword) - 1) == 0 ||
                      std::strncmp(line, kCombinationKeyword, sizeof(kCombinationKeyword) - 1) == 0;
    return ours && count < 0 ? SettingCommand::kInvalid : SettingCommand::kNone;
  }
  if (std::strcmp(words[0], kShowKeyword) == 0) {
    return count == 1 ? SettingCommand::kShow : SettingCommand::kInvalid;
  }
  ProbeSetup next = current;
  if (std::strcmp(words[0], kRoleKeyword) == 0) {
    Probe probe = Probe::kSht45;
    ProbeRole role = ProbeRole::kInterior;
    if (count != 3 || !parseProbe(words[1], probe) || !parseRole(words[2], role)) {
      return SettingCommand::kInvalid;
    }
    next.roles[static_cast<uint8_t>(probe)] = role;
  } else if (std::strcmp(words[0], kCombinationKeyword) == 0) {
    if (count != 2 || !parseCombination(words[1], next.combination)) {
      return SettingCommand::kInvalid;
    }
  } else {
    return SettingCommand::kNone;
  }
  if (!isValidProbeSetup(next)) return SettingCommand::kInvalid;
  out = next;
  return SettingCommand::kSet;
}

}  // namespace terrarium::control
