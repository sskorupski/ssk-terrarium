#include "state_message.h"

#include <cstdarg>
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

// Appends a formatted string at `used`. False if it does not fit.
__attribute__((format(printf, 4, 5))) bool appendf(char* out, size_t size, size_t& used,
                                                   const char* format, ...) {
  if (used >= size) return false;
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(out + used, size - used, format, args);
  va_end(args);
  if (written < 0 || used + static_cast<size_t>(written) >= size) return false;
  used += static_cast<size_t>(written);
  return true;
}

// "HH:MM-HH:MM" quoted, or null if the window is not valid.
void quotedDay(const control::DayWindow& window, char (&out)[control::kDayWindowTextLength + 3]) {
  char text[control::kDayWindowTextLength + 1];
  if (control::formatDayWindow(window, text)) {
    std::snprintf(out, sizeof(out), "\"%s\"", text);
  } else {
    std::snprintf(out, sizeof(out), "null");
  }
}

// "jour_nuit", "jour_aujourdhui" and "moment" (RG-jour-et-nuit).
bool appendDayNight(char* out, size_t size, size_t& used, const StateSnapshot& s) {
  const control::DayNight& d = s.dayNight;
  char day[control::kDayWindowTextLength + 3];
  if (d.mode == control::DayNightMode::kSeasons) {
    if (!appendf(out, size, used, ",\"jour_nuit\":{\"mode\":\"saisons\",\"saisons\":[")) {
      return false;
    }
    const uint8_t count = d.seasonCount <= control::kMaxSeasons ? d.seasonCount : 0;
    for (uint8_t i = 0; i < count; ++i) {
      char start[control::kMonthDayTextLength + 1] = "";
      control::formatMonthDay(d.seasons[i].startDay, start);
      quotedDay(d.seasons[i].day, day);
      if (!appendf(out, size, used, "%s{\"debut\":\"%s\",\"jour\":%s}", i > 0 ? "," : "", start,
                   day)) {
        return false;
      }
    }
    if (!appendf(out, size, used, "]}")) return false;
  } else {
    quotedDay(d.fixed, day);
    if (!appendf(out, size, used, ",\"jour_nuit\":{\"mode\":\"metronome\",\"jour\":%s}", day)) {
      return false;
    }
  }
  char today[control::kDayWindowTextLength + 3] = "null";
  if (s.timeKnown) quotedDay(s.today, today);
  return appendf(out, size, used, ",\"jour_aujourdhui\":%s,\"moment\":\"%s\"", today,
                 control::momentName(s.moment));
}

bool appendRhythm(char* out, size_t size, size_t& used, const char* name,
                  const control::FanRhythm& r) {
  return appendf(out, size, used,
                 "\"%s\":{\"mode\":\"%s\",\"duree\":%u,\"periode\":%u,\"ecart\":%u}", name,
                 control::fanModeName(r.mode), unsigned{r.durationMin}, unsigned{r.periodMin},
                 unsigned{r.spreadPercent});
}

// "ventilation" (RG-ventilation-periodique). An invalid time is null.
bool appendFan(char* out, size_t size, size_t& used, const control::FanSettings& f) {
  char time[control::kClockTimeTextLength + 3] = "null";
  char clock[control::kClockTimeTextLength + 1];
  if (control::formatClockTime(f.hibernation.dailyMinute, clock)) {
    std::snprintf(time, sizeof(time), "\"%s\"", clock);
  }
  const control::SaturationOption& sat = f.saturation;
  return appendf(out, size, used, ",\"ventilation\":{") &&
         appendRhythm(out, size, used, "jour", f.day) && appendf(out, size, used, ",") &&
         appendRhythm(out, size, used, "nuit", f.night) &&
         appendf(out, size, used,
                 ",\"hibernation\":{\"heure\":%s,\"duree\":%u},\"saturation\":{\"active\":%s,"
                 "\"comportement\":\"%s\",\"humidite\":%u,\"attente\":%u,\"duree\":%u,"
                 "\"pause\":%u}}",
                 time, unsigned{f.hibernation.durationMin}, boolText(sat.active),
                 control::behaviourName(sat.behaviour), unsigned{sat.humidityPercent},
                 unsigned{sat.waitMin}, unsigned{sat.durationMin}, unsigned{sat.pauseMin});
}

// Tenths of a degree as "24.0".
struct Tenths {
  char text[10];
  explicit Tenths(int16_t tenths) {
    std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(tenths) / 10.0);
  }
};

// "securite_substrat", "seuils_substrat" and "part_substrat"
// (RG-coupure-par-le-substrat).
bool appendSubstrate(char* out, size_t size, size_t& used, const StateSnapshot& s) {
  const control::SubstrateSettings& c = s.substrateSettings;
  if (!appendf(out, size, used,
               ",\"securite_substrat\":{\"active\":%s,\"approche\":%s,"
               "\"jour\":{\"coupure\":%s,\"reprise\":%s},\"nuit\":{\"coupure\":%s,\"reprise\":%s},"
               "\"sonde_muette\":{\"comportement\":\"%s\",\"delai\":%u}}",
               boolText(c.active), Tenths(c.approachTenths).text, Tenths(c.day.cutTenths).text,
               Tenths(c.day.resumeTenths).text, Tenths(c.night.cutTenths).text,
               Tenths(c.night.resumeTenths).text,
               control::silentProbeBehaviourName(c.silentBehaviour), unsigned{c.silentDelayMin})) {
    return false;
  }
  if (c.active) {
    const control::SubstrateThresholds t = control::thresholdsFor(c, s.moment);
    if (!appendf(out, size, used, ",\"seuils_substrat\":{\"coupure\":%s,\"reprise\":%s}",
                 Tenths(t.cutTenths).text, Tenths(t.resumeTenths).text)) {
      return false;
    }
  } else if (!appendf(out, size, used, ",\"seuils_substrat\":null")) {
    return false;
  }
  if (!c.active || s.substratePartTenths < 0 || s.substratePartTenths > 10) {
    return appendf(out, size, used, ",\"part_substrat\":null");
  }
  return appendf(out, size, used, ",\"part_substrat\":%.2f",
                 static_cast<double>(s.substratePartTenths) / 10.0);
}

// The bounds of "securite_substrat", inside "bornes".
bool appendSubstrateBounds(char* out, size_t size, size_t& used,
                           const control::SubstrateSettingsBounds& b) {
  return appendf(out, size, used,
                 ",\"substrat\":{\"coupure\":[%s,%s],\"reprise_min\":%s,\"ecart_min\":%s,"
                 "\"delai\":[%u,%u],\"approche\":[%s,%s],\"comportements\":[\"%s\",\"%s\"]}",
                 Tenths(b.cutMinTenths).text, Tenths(b.cutMaxTenths).text,
                 Tenths(b.resumeMinTenths).text, Tenths(b.gapMinTenths).text,
                 unsigned{b.delayMinMin}, unsigned{b.delayMaxMin}, Tenths(b.approachMinTenths).text,
                 Tenths(b.approachMaxTenths).text,
                 control::silentProbeBehaviourName(control::SilentProbeBehaviour::kResumeOnAir),
                 control::silentProbeBehaviourName(control::SilentProbeBehaviour::kHold));
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
  if (!appendf(out, size, used,
               ",\"plages\":[%s],\"fuseau\":\"%s\",\"heure_connue\":%s,\"en_plage\":%s", windows,
               timezone, boolText(s.timeKnown), inWindow) ||
      !appendDayNight(out, size, used, s) || !appendFan(out, size, used, s.fanSettings) ||
      !appendf(out, size, used,
               ",\"aeration_saturation\":%s,\"chauffage_nuit\":{\"consigne\":%.1f,"
               "\"hysteresis\":%.1f}",
               boolText(s.saturationRunning), s.nightHeat.setpointC, s.nightHeat.hysteresisC) ||
      !appendSubstrate(out, size, used, s)) {
    return 0;
  }
  const control::FanSettingsBounds& f = s.fanBounds;
  if (!appendf(out, size, used,
               ",\"bornes\":{\"consigne\":[%.1f,%.1f],\"hysteresis\":[%.1f,%.1f],"
               "\"somme_max\":%.1f,\"plages_max\":%u,\"fuseau_max\":%u,\"combinaisons\":[%s],"
               "\"saisons_max\":%u,\"ventilation\":{\"duree\":[%u,%u],\"periode\":[%u,%u],"
               "\"ecart\":[%u,%u],\"humidite\":[%u,%u],\"attente\":[%u,%u],\"pause\":[%u,%u]}",
               s.bounds.setpointMinC, s.bounds.setpointMaxC, s.bounds.hysteresisMinC,
               s.bounds.hysteresisMaxC, s.bounds.stopBelowC,
               static_cast<unsigned>(control::kMaxTimeWindows),
               static_cast<unsigned>(control::kTimezoneMaxLength), combinations,
               static_cast<unsigned>(control::kMaxSeasons), unsigned{f.durationMinMin},
               unsigned{f.durationMaxMin}, unsigned{f.periodMinMin}, unsigned{f.periodMaxMin},
               unsigned{f.spreadMinPercent}, unsigned{f.spreadMaxPercent},
               unsigned{f.humidityMinPercent}, unsigned{f.humidityMaxPercent},
               unsigned{f.waitMinMin}, unsigned{f.waitMaxMin}, unsigned{f.pauseMinMin},
               unsigned{f.pauseMaxMin}) ||
      !appendSubstrateBounds(out, size, used, s.substrateBounds) ||
      !appendf(out, size, used, "}}")) {
    return 0;
  }
  return used;
}

}  // namespace terrarium::supervision
