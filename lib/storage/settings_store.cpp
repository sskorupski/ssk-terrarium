#include "settings_store.h"

#include <Preferences.h>

namespace terrarium::storage {

namespace {

constexpr char kNamespace[] = "terrarium";
constexpr char kModeKey[] = "mode";

}  // namespace

// TODO(phase 2): Preferences in namespace "terrarium", with a schema version and
// range checks against the safety limits before accepting stored values.
bool SettingsStore::load(Settings& /*out*/) { return false; }

// TODO(phase 2): write only changed keys to spare the flash.
bool SettingsStore::save(const Settings& /*settings*/) { return false; }

bool SettingsStore::loadMode(uint8_t& out) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/true)) return false;
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

}  // namespace terrarium::storage
