#include "state_message.h"

#include <cstdio>
#include <cstring>

namespace terrarium::supervision {

namespace {

// A version is quoted verbatim, so it may only hold characters that need no
// escaping in JSON.
const char* safeVersion(const char* version) {
  if (version == nullptr || *version == '\0') return "inconnue";
  for (const char* c = version; *c != '\0'; ++c) {
    if (*c < 0x20 || *c > 0x7e || *c == '"' || *c == '\\') return "inconnue";
  }
  return version;
}

const char* boolText(bool value) { return value ? "true" : "false"; }

// Writes "<name>":{...} for one probe at `used`. False if it does not fit.
bool appendProbe(char* out, size_t size, size_t& used, control::Probe probe,
                 const ProbeState& state) {
  char temperature[8] = "null";
  char humidity[8] = "null";
  char cause[32] = "null";
  if (state.rejection == control::AirRejection::kNone) {
    std::snprintf(temperature, sizeof(temperature), "%.1f", state.temperatureC);
    std::snprintf(humidity, sizeof(humidity), "%.0f", state.humidityRhPercent);
  } else if (const char* name = control::rejectionName(state.rejection)) {
    std::snprintf(cause, sizeof(cause), "\"%s\"", name);
  }
  const int written = std::snprintf(
      out + used, size - used,
      "%s\"%s\":{\"role\":\"%s\",\"temperature\":%s,\"humidite\":%s,\"cause\":%s,\"defaut\":%s}",
      probe == control::Probe::kSht45 ? "" : ",", control::probeName(probe),
      control::roleName(state.role), temperature, humidity, cause, boolText(state.fault));
  if (written <= 0 || used + static_cast<size_t>(written) >= size) return false;
  used += static_cast<size_t>(written);
  return true;
}

}  // namespace

size_t formatState(const StateSnapshot& s, char* out, size_t size) {
  char airT[8] = "null";
  char airRh[8] = "null";
  char substrateT[8] = "null";
  if (s.airValid) {
    std::snprintf(airT, sizeof(airT), "%.1f", s.airTemperatureC);
    std::snprintf(airRh, sizeof(airRh), "%.0f", s.airHumidityRhPercent);
  }
  char rejection[32] = "null";
  if (const char* name = control::rejectionName(s.airRejection)) {
    std::snprintf(rejection, sizeof(rejection), "\"%s\"", name);
  }
  if (s.substrateValid)
    std::snprintf(substrateT, sizeof(substrateT), "%.1f", s.substrateTemperatureC);

  char source[16] = "null";
  if (const char* name = control::sourceName(s.airSource)) {
    std::snprintf(source, sizeof(source), "\"%s\"", name);
  }
  const int opening =
      std::snprintf(out, size,
                    "{\"air\":{\"temperature\":%s,\"humidite\":%s},\"sonde_retenue\":%s,"
                    "\"sondes\":{",
                    airT, airRh, source);
  if (opening <= 0 || static_cast<size_t>(opening) >= size) return 0;
  size_t head = static_cast<size_t>(opening);
  for (uint8_t i = 0; i < control::kProbeCount; ++i) {
    if (!appendProbe(out, size, head, static_cast<control::Probe>(i), s.probes[i])) return 0;
  }

  const int written = std::snprintf(
      out + head, size - head,
      "},\"substrat\":{\"temperature\":%s},"
      "\"sorties\":{\"chauffage\":%s,\"eclairage\":%s,\"pompe\":%s,\"ventilateur\":%s},"
      "\"mode\":\"%s\",\"consigne\":%.1f,\"hysteresis\":%.1f,\"defaut_capteur\":%s,"
      "\"cause_mesure_air\":%s,\"mise_en_securite\":{\"air\":%s},\"combinaison\":\"%s\","
      "\"pause_chauffage\":%s,"
      "\"coupure_substrat\":%s,\"uptime_s\":%lu,\"version\":\"%s\"}",
      substrateT, boolText(s.heater), boolText(s.light), boolText(s.mistPump), boolText(s.fan),
      control::toString(s.mode), s.setpointC, s.hysteresisC, boolText(s.sensorFault), rejection,
      boolText(s.airSafety), control::combinationName(s.combination), boolText(s.heatPaused),
      boolText(s.substrateCut), static_cast<unsigned long>(s.uptimeS),
      safeVersion(s.firmwareVersion));
  if (written <= 0 || head + static_cast<size_t>(written) >= size) return 0;

  // Drops the closing brace, then appends the PLAGES part and the bounds of
  // the settings, so a client never copies them (RG-reglages-bornes).
  size_t used = head + static_cast<size_t>(written) - 1;
  char windows[control::kMaxTimeWindows * (control::kTimeWindowTextLength + 3) + 1] = "";
  size_t windowsUsed = 0;
  if (control::isValidTimeWindows(s.windows)) {
    for (uint8_t i = 0; i < s.windows.count; ++i) {
      char one[control::kTimeWindowTextLength + 1];
      control::formatTimeWindow(s.windows.windows[i], one);
      windowsUsed += static_cast<size_t>(std::snprintf(
          windows + windowsUsed, sizeof(windows) - windowsUsed, "%s\"%s\"", i > 0 ? "," : "", one));
    }
  }
  char combinations[80] = "";
  size_t combinationsUsed = 0;
  for (uint8_t i = 0; i < control::kCombinationCount; ++i) {
    combinationsUsed += static_cast<size_t>(std::snprintf(
        combinations + combinationsUsed, sizeof(combinations) - combinationsUsed, "%s\"%s\"",
        i > 0 ? "," : "", control::combinationName(static_cast<control::ProbeCombination>(i))));
  }
  const char* timezone = control::isValidPosixTimezone(s.timezone) ? s.timezone : "";
  const char* inWindow = s.timeKnown ? boolText(s.inWindow) : "null";
  const int tail =
      std::snprintf(out + used, size - used,
                    ",\"plages\":[%s],\"fuseau\":\"%s\",\"heure_connue\":%s,"
                    "\"en_plage\":%s,\"bornes\":{\"consigne\":[%.1f,%.1f],"
                    "\"hysteresis\":[%.1f,%.1f],\"somme_max\":%.1f,"
                    "\"plages_max\":%u,\"fuseau_max\":%u,\"combinaisons\":[%s]}}",
                    windows, timezone, boolText(s.timeKnown), inWindow, s.bounds.setpointMinC,
                    s.bounds.setpointMaxC, s.bounds.hysteresisMinC, s.bounds.hysteresisMaxC,
                    s.bounds.stopBelowC, static_cast<unsigned>(control::kMaxTimeWindows),
                    static_cast<unsigned>(control::kTimezoneMaxLength), combinations);
  if (tail <= 0 || used + static_cast<size_t>(tail) >= size) return 0;
  return used + static_cast<size_t>(tail);
}

}  // namespace terrarium::supervision
