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

}  // namespace terrarium::storage
