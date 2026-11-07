#include <unity.h>

#include "mode_machine.h"
#include "serial_command.h"

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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_each_mode_command);
  RUN_TEST(test_ignores_a_trailing_carriage_return);
  RUN_TEST(test_rejects_anything_else_and_leaves_mode_untouched);
  return UNITY_END();
}
