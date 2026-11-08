#include <unity.h>

#include "mode_machine.h"

using terrarium::control::HeatingPolicy;
using terrarium::control::Mode;
using terrarium::control::modeFromStored;
using terrarium::control::ModeMachine;
using terrarium::control::ModePolicy;
using terrarium::control::modeToStored;
using terrarium::control::parseMode;
using terrarium::control::policyFor;
using terrarium::control::toString;

void setUp() {}
void tearDown() {}

// Cadrage, modes table.
void test_active_policy() {
  const ModePolicy policy = policyFor(Mode::kActive);
  TEST_ASSERT_EQUAL(HeatingPolicy::kSetpoint, policy.heating);
  TEST_ASSERT_TRUE(policy.mistAllowed);
  TEST_ASSERT_TRUE(policy.lightAllowed);
}

void test_hibernation_policy() {
  const ModePolicy policy = policyFor(Mode::kHibernation);
  TEST_ASSERT_EQUAL(HeatingPolicy::kFrostProtection, policy.heating);
  TEST_ASSERT_FALSE(policy.mistAllowed);
  TEST_ASSERT_FALSE(policy.lightAllowed);
}

void test_maintenance_policy() {
  const ModePolicy policy = policyFor(Mode::kMaintenance);
  TEST_ASSERT_EQUAL(HeatingPolicy::kOff, policy.heating);
  TEST_ASSERT_FALSE(policy.mistAllowed);
  TEST_ASSERT_FALSE(policy.lightAllowed);
}

void test_keeps_initial_mode_until_requested() {
  ModeMachine machine(Mode::kMaintenance);
  TEST_ASSERT_EQUAL(Mode::kMaintenance, machine.mode());
}

void test_every_transition_is_allowed() {
  const Mode modes[] = {Mode::kActive, Mode::kHibernation, Mode::kMaintenance};
  for (Mode from : modes) {
    for (Mode to : modes) {
      if (from == to) continue;
      ModeMachine machine(from);
      TEST_ASSERT_TRUE(machine.request(to));
      TEST_ASSERT_EQUAL(to, machine.mode());
    }
  }
}

void test_requesting_current_mode_changes_nothing() {
  ModeMachine machine(Mode::kActive);
  TEST_ASSERT_FALSE(machine.request(Mode::kActive));
  TEST_ASSERT_EQUAL(Mode::kActive, machine.mode());
}

void test_policy_follows_mode() {
  ModeMachine machine(Mode::kActive);
  machine.request(Mode::kMaintenance);
  TEST_ASSERT_EQUAL(HeatingPolicy::kOff, machine.policy().heating);
}

void test_wire_names_round_trip() {
  const Mode modes[] = {Mode::kActive, Mode::kHibernation, Mode::kMaintenance};
  for (Mode mode : modes) {
    Mode parsed = Mode::kMaintenance;
    TEST_ASSERT_TRUE(parseMode(toString(mode), parsed));
    TEST_ASSERT_EQUAL(mode, parsed);
  }
  TEST_ASSERT_EQUAL_STRING("ACTIF", toString(Mode::kActive));
  TEST_ASSERT_EQUAL_STRING("HIBERNATION", toString(Mode::kHibernation));
  TEST_ASSERT_EQUAL_STRING("MAINTENANCE", toString(Mode::kMaintenance));
}

void test_rejects_unknown_wire_names() {
  Mode out = Mode::kHibernation;
  TEST_ASSERT_FALSE(parseMode("actif", out));
  TEST_ASSERT_FALSE(parseMode("ACTIVE", out));
  TEST_ASSERT_FALSE(parseMode("", out));
  TEST_ASSERT_FALSE(parseMode(nullptr, out));
  TEST_ASSERT_EQUAL(Mode::kHibernation, out);
}

// RG-mode-au-demarrage: the stored byte round-trips; anything else is refused,
// and the caller then starts in MAINTENANCE.
void test_stored_mode_round_trips() {
  const Mode modes[] = {Mode::kActive, Mode::kHibernation, Mode::kMaintenance};
  for (Mode mode : modes) {
    Mode restored = Mode::kMaintenance;
    TEST_ASSERT_TRUE(modeFromStored(modeToStored(mode), restored));
    TEST_ASSERT_EQUAL(mode, restored);
  }
}

void test_rejects_an_unknown_stored_byte() {
  Mode out = Mode::kActive;
  TEST_ASSERT_FALSE(modeFromStored(4, out));
  TEST_ASSERT_FALSE(modeFromStored(0xFF, out));
  TEST_ASSERT_EQUAL(Mode::kActive, out);
}

// RG-sorties-par-mode: PLAGES heats by its windows, and allows what ACTIF allows.
void test_schedule_policy() {
  const ModePolicy policy = policyFor(Mode::kSchedule);
  TEST_ASSERT_EQUAL(HeatingPolicy::kSchedule, policy.heating);
  TEST_ASSERT_TRUE(policy.mistAllowed);
  TEST_ASSERT_TRUE(policy.lightAllowed);
}

// RG-topics-mqtt: cmd/mode accepts PLAGES; NVS keeps the bytes of the
// existing modes, so that an older saved mode still reads back the same.
void test_schedule_wire_name_and_stored_byte() {
  Mode out = Mode::kMaintenance;
  TEST_ASSERT_EQUAL_STRING("PLAGES", toString(Mode::kSchedule));
  TEST_ASSERT_TRUE(parseMode("PLAGES", out));
  TEST_ASSERT_EQUAL(Mode::kSchedule, out);
  TEST_ASSERT_EQUAL_UINT8(0, modeToStored(Mode::kActive));
  TEST_ASSERT_EQUAL_UINT8(1, modeToStored(Mode::kHibernation));
  TEST_ASSERT_EQUAL_UINT8(2, modeToStored(Mode::kMaintenance));
  TEST_ASSERT_EQUAL_UINT8(3, modeToStored(Mode::kSchedule));
  TEST_ASSERT_TRUE(modeFromStored(3, out));
  TEST_ASSERT_EQUAL(Mode::kSchedule, out);
  TEST_ASSERT_FALSE(modeFromStored(4, out));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_active_policy);
  RUN_TEST(test_hibernation_policy);
  RUN_TEST(test_maintenance_policy);
  RUN_TEST(test_keeps_initial_mode_until_requested);
  RUN_TEST(test_every_transition_is_allowed);
  RUN_TEST(test_requesting_current_mode_changes_nothing);
  RUN_TEST(test_policy_follows_mode);
  RUN_TEST(test_wire_names_round_trip);
  RUN_TEST(test_rejects_unknown_wire_names);
  RUN_TEST(test_stored_mode_round_trips);
  RUN_TEST(test_rejects_an_unknown_stored_byte);
  RUN_TEST(test_schedule_policy);
  RUN_TEST(test_schedule_wire_name_and_stored_byte);
  return UNITY_END();
}
