#include "settings_store.h"

#include <Preferences.h>
#include <esp_random.h>

#include <cstring>

namespace terrarium::storage {

namespace {

constexpr char kNamespace[] = "terrarium";
constexpr char kModeKey[] = "mode";
// The struct is stored as raw bytes: a layout change shows up as a size
// mismatch, and the board then opens its portal instead of using garbage.
constexpr char kNetworkKey[] = "reseau";
constexpr char kApPasswordKey[] = "ap_mdp";
constexpr char kSetpointKey[] = "consigne";
constexpr char kHysteresisKey[] = "hysteresis";
constexpr char kTimeWindowsKey[] = "plages";
constexpr char kTimezoneKey[] = "fuseau";
constexpr char kAirSafetyKey[] = "securite_air";
constexpr char kProbeSetupKey[] = "sondes";
// Duration, period and time of day, two little-endian bytes each.
constexpr char kFanKey[] = "ventilation";
constexpr size_t kFanStoredSize = 6;

constexpr char kUpdateLicenceKey[] = "licence_maj";
constexpr char kUpdateAttemptKey[] = "maj_tentee";

// A string key of at most size - 1 characters; false if absent or empty.
template <size_t N>
bool loadString(const char* key, char (&out)[N]) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  char stored[N] = {};
  const bool found = preferences.isKey(key) && preferences.getString(key, stored, N) > 0;
  preferences.end();
  if (found) std::memcpy(out, stored, N);
  return found;
}

bool saveString(const char* key, const char* value) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written = preferences.putString(key, value) == std::strlen(value);
  preferences.end();
  return written;
}

}  // namespace

// TODO(phase 2): Preferences in namespace "terrarium", with a schema version and
// range checks against the safety limits before accepting stored values.
bool SettingsStore::load(Settings& /*out*/) { return false; }

// TODO(phase 2): write only changed keys to spare the flash.
bool SettingsStore::save(const Settings& /*settings*/) { return false; }

bool SettingsStore::loadMode(uint8_t& out) {
  Preferences preferences;
  // Read-write so that a first boot creates the namespace instead of logging
  // "nvs_open failed: NOT_FOUND"; the caller reports the missing mode itself.
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool found = preferences.isKey(kModeKey);
  if (found) out = preferences.getUChar(kModeKey);
  preferences.end();
  return found;
}

bool SettingsStore::saveMode(uint8_t mode) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written = preferences.putUChar(kModeKey, mode) == sizeof(mode);
  preferences.end();
  return written;
}

bool SettingsStore::loadAirSafety(bool& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool found = preferences.isKey(kAirSafetyKey);
  // Fails safe: a corrupted byte keeps the safety on.
  if (found) out = preferences.getUChar(kAirSafetyKey) != 0;
  preferences.end();
  return found;
}

bool SettingsStore::saveAirSafety(bool enabled) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const uint8_t value = enabled ? 1 : 0;
  const bool written = preferences.putUChar(kAirSafetyKey, value) == sizeof(value);
  preferences.end();
  return written;
}

bool SettingsStore::loadProbeSetup(uint8_t& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool found = preferences.isKey(kProbeSetupKey);
  if (found) out = preferences.getUChar(kProbeSetupKey);
  preferences.end();
  return found;
}

bool SettingsStore::saveProbeSetup(uint8_t stored) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written = preferences.putUChar(kProbeSetupKey, stored) == sizeof(stored);
  preferences.end();
  return written;
}

bool SettingsStore::loadHeatSettings(control::HeatSettings& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool found = preferences.isKey(kSetpointKey) && preferences.isKey(kHysteresisKey);
  if (found) {
    out.setpointC = preferences.getFloat(kSetpointKey);
    out.hysteresisC = preferences.getFloat(kHysteresisKey);
  }
  preferences.end();
  return found;
}

bool SettingsStore::saveHeatSettings(const control::HeatSettings& settings) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written = preferences.putFloat(kSetpointKey, settings.setpointC) == sizeof(float) &&
                       preferences.putFloat(kHysteresisKey, settings.hysteresisC) == sizeof(float);
  preferences.end();
  return written;
}

bool SettingsStore::loadTimeWindows(control::TimeWindows& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  control::TimeWindows stored{};
  const bool found =
      preferences.isKey(kTimeWindowsKey) &&
      preferences.getBytesLength(kTimeWindowsKey) == sizeof(stored) &&
      preferences.getBytes(kTimeWindowsKey, &stored, sizeof(stored)) == sizeof(stored);
  preferences.end();
  if (found) out = stored;
  return found;
}

bool SettingsStore::saveTimeWindows(const control::TimeWindows& windows) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written =
      preferences.putBytes(kTimeWindowsKey, &windows, sizeof(windows)) == sizeof(windows);
  preferences.end();
  return written;
}

bool SettingsStore::loadTimezone(char (&out)[control::kTimezoneSize]) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  char stored[control::kTimezoneSize] = {};
  const bool found = preferences.isKey(kTimezoneKey) &&
                     preferences.getString(kTimezoneKey, stored, sizeof(stored)) > 0;
  preferences.end();
  if (found) std::memcpy(out, stored, sizeof(out));
  return found;
}

bool SettingsStore::saveTimezone(const char* timezone) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written = preferences.putString(kTimezoneKey, timezone) == std::strlen(timezone);
  preferences.end();
  return written;
}

bool SettingsStore::loadFanSettings(control::FanSettings& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  uint8_t stored[kFanStoredSize] = {};
  const bool found = preferences.isKey(kFanKey) &&
                     preferences.getBytesLength(kFanKey) == kFanStoredSize &&
                     preferences.getBytes(kFanKey, stored, sizeof(stored)) == kFanStoredSize;
  preferences.end();
  if (found) {
    out.durationMin = static_cast<uint16_t>(stored[0] | (stored[1] << 8));
    out.periodMin = static_cast<uint16_t>(stored[2] | (stored[3] << 8));
    out.dailyMinute = static_cast<uint16_t>(stored[4] | (stored[5] << 8));
  }
  return found;
}

bool SettingsStore::saveFanSettings(const control::FanSettings& settings) {
  const uint8_t stored[kFanStoredSize] = {static_cast<uint8_t>(settings.durationMin & 0xFF),
                                          static_cast<uint8_t>(settings.durationMin >> 8),
                                          static_cast<uint8_t>(settings.periodMin & 0xFF),
                                          static_cast<uint8_t>(settings.periodMin >> 8),
                                          static_cast<uint8_t>(settings.dailyMinute & 0xFF),
                                          static_cast<uint8_t>(settings.dailyMinute >> 8)};
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written = preferences.putBytes(kFanKey, stored, sizeof(stored)) == sizeof(stored);
  preferences.end();
  return written;
}

bool SettingsStore::loadNetwork(supervision::NetworkSettings& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  supervision::NetworkSettings stored{};
  const bool found = preferences.isKey(kNetworkKey) &&
                     preferences.getBytesLength(kNetworkKey) == sizeof(stored) &&
                     preferences.getBytes(kNetworkKey, &stored, sizeof(stored)) == sizeof(stored);
  preferences.end();
  if (!found || supervision::validate(stored) != supervision::SettingsError::kNone) return false;
  out = stored;
  return true;
}

bool SettingsStore::saveNetwork(const supervision::NetworkSettings& settings) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool written =
      preferences.putBytes(kNetworkKey, &settings, sizeof(settings)) == sizeof(settings);
  preferences.end();
  return written;
}

bool SettingsStore::loadOrCreateApPassword(char (&out)[supervision::kApPasswordLength + 1]) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  char stored[supervision::kApPasswordLength + 1] = {};
  if (preferences.isKey(kApPasswordKey)) {
    preferences.getString(kApPasswordKey, stored, sizeof(stored));
  }
  bool ok = true;
  if (!supervision::isValidApPassword(stored)) {
    uint8_t random[supervision::kApPasswordLength];
    esp_fill_random(random, sizeof(random));
    supervision::makeApPassword(random, stored);
    ok = preferences.putString(kApPasswordKey, stored) == supervision::kApPasswordLength;
  }
  preferences.end();
  std::memcpy(out, stored, sizeof(out));
  return ok;
}

bool SettingsStore::loadUpdateLicence(char (&out)[update::kLicenceSize]) {
  return loadString(kUpdateLicenceKey, out);
}

bool SettingsStore::saveUpdateLicence(const char* licence) {
  return saveString(kUpdateLicenceKey, licence);
}

bool SettingsStore::loadUpdateAttempt(char (&out)[update::kVersionTextSize]) {
  return loadString(kUpdateAttemptKey, out);
}

bool SettingsStore::saveUpdateAttempt(const char* version) {
  return saveString(kUpdateAttemptKey, version);
}

bool SettingsStore::clearUpdateAttempt() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) return false;
  const bool cleared =
      !preferences.isKey(kUpdateAttemptKey) || preferences.remove(kUpdateAttemptKey);
  preferences.end();
  return cleared;
}

}  // namespace terrarium::storage
