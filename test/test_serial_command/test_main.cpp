#include <unity.h>

#include "mode_machine.h"
#include "serial_command.h"

using terrarium::control::parseTimezoneCommand;
using terrarium::control::parseWindowsCommand;
using terrarium::control::SettingCommand;
using terrarium::control::TimeWindows;

using terrarium::control::Mode;
using terrarium::control::parseModeCommand;

void setUp() {}
void tearDown() {}

// RG-changement-de-mode-manuel: the mode changes on an explicit serial command.
void test_parses_each_mode_command() {
  Mode out = Mode::kMaintenance;
  TEST_ASSERT_TRUE(parseModeCommand("MODE ACTIF", out));
  TEST_ASSERT_EQUAL(Mode::kActive, out);
  TEST_ASSERT_TRUE(parseModeCommand("MODE HIBERNATION", out));
  TEST_ASSERT_EQUAL(Mode::kHibernation, out);
  TEST_ASSERT_TRUE(parseModeCommand("MODE MAINTENANCE", out));
  TEST_ASSERT_EQUAL(Mode::kMaintenance, out);
}

// Serial monitors end lines with CR LF or LF: the caller strips the LF only.
void test_ignores_a_trailing_carriage_return() {
  Mode out = Mode::kMaintenance;
  TEST_ASSERT_TRUE(parseModeCommand("MODE ACTIF\r", out));
  TEST_ASSERT_EQUAL(Mode::kActive, out);
}

void test_rejects_anything_else_and_leaves_mode_untouched() {
  Mode out = Mode::kHibernation;
  TEST_ASSERT_FALSE(parseModeCommand("MODE actif", out));
  TEST_ASSERT_FALSE(parseModeCommand("MODE", out));
  TEST_ASSERT_FALSE(parseModeCommand("MODE ", out));
  TEST_ASSERT_FALSE(parseModeCommand("MODEACTIF", out));
  TEST_ASSERT_FALSE(parseModeCommand("MODE  ACTIF", out));
  TEST_ASSERT_FALSE(parseModeCommand("MODE ACTIF extra", out));
  TEST_ASSERT_FALSE(parseModeCommand("ACTIF", out));
  TEST_ASSERT_FALSE(parseModeCommand("", out));
  TEST_ASSERT_FALSE(parseModeCommand(nullptr, out));
  TEST_ASSERT_EQUAL(Mode::kHibernation, out);
}

// RG-configuration-wifi-sur-place: WIFI CONFIGURER reopens the portal.
void test_parses_the_portal_command() {
  TEST_ASSERT_TRUE(terrarium::control::isWifiPortalCommand("WIFI CONFIGURER"));
  TEST_ASSERT_TRUE(terrarium::control::isWifiPortalCommand("WIFI CONFIGURER\r"));
  TEST_ASSERT_FALSE(terrarium::control::isWifiPortalCommand("WIFI configurer"));
  TEST_ASSERT_FALSE(terrarium::control::isWifiPortalCommand("WIFI CONFIGURER x"));
  TEST_ASSERT_FALSE(terrarium::control::isWifiPortalCommand("WIFI"));
  TEST_ASSERT_FALSE(terrarium::control::isWifiPortalCommand(""));
  TEST_ASSERT_FALSE(terrarium::control::isWifiPortalCommand(nullptr));
}

// RG-reglages-bornes: PLAGES shows or replaces the windows.
void test_parses_the_windows_command() {
  TimeWindows out{9, {}};
  TEST_ASSERT_EQUAL(SettingCommand::kShow, parseWindowsCommand("PLAGES", out));
  TEST_ASSERT_EQUAL(SettingCommand::kShow, parseWindowsCommand("PLAGES\r", out));
  TEST_ASSERT_EQUAL_UINT8(9, out.count);
  TEST_ASSERT_EQUAL(SettingCommand::kSet,
                    parseWindowsCommand("PLAGES 01:35-06:30 14:05-17:05", out));
  TEST_ASSERT_EQUAL_UINT8(2, out.count);
  TEST_ASSERT_EQUAL(SettingCommand::kSet, parseWindowsCommand("PLAGES AUCUNE\r", out));
  TEST_ASSERT_EQUAL_UINT8(0, out.count);
}

void test_rejects_a_bad_windows_command() {
  TimeWindows out{2, {}};
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid, parseWindowsCommand("PLAGES 25:00-01:00", out));
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid, parseWindowsCommand("PLAGES ", out));
  TEST_ASSERT_EQUAL_UINT8(2, out.count);
  TEST_ASSERT_EQUAL(SettingCommand::kNone, parseWindowsCommand("PLAGESX", out));
  TEST_ASSERT_EQUAL(SettingCommand::kNone, parseWindowsCommand("MODE PLAGES", out));
  TEST_ASSERT_EQUAL(SettingCommand::kNone, parseWindowsCommand(nullptr, out));
}

// ADR-heure-par-ntp: FUSEAU shows or replaces the time zone.
void test_parses_the_timezone_command() {
  char out[terrarium::control::kTimezoneSize] = "UTC0";
  TEST_ASSERT_EQUAL(SettingCommand::kShow, parseTimezoneCommand("FUSEAU", out));
  TEST_ASSERT_EQUAL_STRING("UTC0", out);
  TEST_ASSERT_EQUAL(SettingCommand::kSet,
                    parseTimezoneCommand("FUSEAU CET-1CEST,M3.5.0,M10.5.0/3\r", out));
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out);
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid, parseTimezoneCommand("FUSEAU Europe/Paris", out));
  TEST_ASSERT_EQUAL(SettingCommand::kInvalid, parseTimezoneCommand("FUSEAU ", out));
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out);
  TEST_ASSERT_EQUAL(SettingCommand::kNone, parseTimezoneCommand("FUSEAUX", out));
  TEST_ASSERT_EQUAL(SettingCommand::kNone, parseTimezoneCommand(nullptr, out));
}

void test_parses_the_schedule_mode_command() {
  Mode out = Mode::kMaintenance;
  TEST_ASSERT_TRUE(parseModeCommand("MODE PLAGES", out));
  TEST_ASSERT_EQUAL(Mode::kSchedule, out);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_each_mode_command);
  RUN_TEST(test_ignores_a_trailing_carriage_return);
  RUN_TEST(test_rejects_anything_else_and_leaves_mode_untouched);
  RUN_TEST(test_parses_the_portal_command);
  RUN_TEST(test_parses_the_windows_command);
  RUN_TEST(test_rejects_a_bad_windows_command);
  RUN_TEST(test_parses_the_timezone_command);
  RUN_TEST(test_parses_the_schedule_mode_command);
  return UNITY_END();
}
