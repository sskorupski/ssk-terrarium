#include "state_message.h"

#include <cstdio>

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
  return static_cast<size_t>(written);
}

}  // namespace terrarium::supervision
