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

}  // namespace

size_t formatState(const StateSnapshot& s, char* out, size_t size) {
  char airT[8] = "null";
  char airRh[8] = "null";
  char substrateT[8] = "null";
  if (s.airValid) {
    std::snprintf(airT, sizeof(airT), "%.1f", s.airTemperatureC);
    std::snprintf(airRh, sizeof(airRh), "%.0f", s.airHumidityRhPercent);
  }
  if (s.substrateValid)
    std::snprintf(substrateT, sizeof(substrateT), "%.1f", s.substrateTemperatureC);

  const int written = std::snprintf(
      out, size,
      "{\"air\":{\"temperature\":%s,\"humidite\":%s},\"substrat\":{\"temperature\":%s},"
      "\"sorties\":{\"chauffage\":%s,\"eclairage\":%s,\"pompe\":%s,\"ventilateur\":%s},"
      "\"mode\":\"%s\",\"consigne\":%.1f,\"hysteresis\":%.1f,\"defaut_capteur\":%s,"
      "\"pause_chauffage\":%s,\"uptime_s\":%lu,\"version\":\"%s\"}",
      airT, airRh, substrateT, boolText(s.heater), boolText(s.light), boolText(s.mistPump),
      boolText(s.fan), control::toString(s.mode), s.setpointC, s.hysteresisC,
      boolText(s.sensorFault), boolText(s.heatPaused), static_cast<unsigned long>(s.uptimeS),
      safeVersion(s.firmwareVersion));
  if (written <= 0 || static_cast<size_t>(written) >= size) return 0;

  // Drops the closing brace, then appends the PLAGES part and the bounds of
  // the settings, so a client never copies them (RG-reglages-bornes).
  size_t used = static_cast<size_t>(written) - 1;
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
  const char* timezone = control::isValidPosixTimezone(s.timezone) ? s.timezone : "";
  const char* inWindow = s.timeKnown ? boolText(s.inWindow) : "null";
  const int tail =
      std::snprintf(out + used, size - used,
                    ",\"plages\":[%s],\"fuseau\":\"%s\",\"heure_connue\":%s,"
                    "\"en_plage\":%s,\"bornes\":{\"consigne\":[%.1f,%.1f],"
                    "\"hysteresis\":[%.1f,%.1f],\"somme_max\":%.1f,"
                    "\"plages_max\":%u,\"fuseau_max\":%u}}",
                    windows, timezone, boolText(s.timeKnown), inWindow, s.bounds.setpointMinC,
                    s.bounds.setpointMaxC, s.bounds.hysteresisMinC, s.bounds.hysteresisMaxC,
                    s.bounds.stopBelowC, static_cast<unsigned>(control::kMaxTimeWindows),
                    static_cast<unsigned>(control::kTimezoneMaxLength));
  if (tail <= 0 || used + static_cast<size_t>(tail) >= size) return 0;
  return used + static_cast<size_t>(tail);
}

}  // namespace terrarium::supervision
