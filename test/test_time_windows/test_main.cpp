#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "time_windows.h"

using namespace terrarium::control;

namespace {

constexpr uint16_t at(uint16_t hour, uint16_t minute) { return hour * 60 + minute; }

// The firmware defaults (config.h): 01:35-06:30 and 14:05-17:05.
TimeWindows defaults() { return TimeWindows{2, {{at(1, 35), at(6, 30)}, {at(14, 5), at(17, 5)}}}; }

bool parseText(const char* text, TimeWindow& out) {
  return parseTimeWindow(text, std::strlen(text), out);
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-chauffage-par-plages: "HH:MM-HH:MM", to the minute.
void test_parses_a_window() {
  TimeWindow out{};
  TEST_ASSERT_TRUE(parseText("01:35-06:30", out));
  TEST_ASSERT_EQUAL_UINT16(at(1, 35), out.startMinute);
  TEST_ASSERT_EQUAL_UINT16(at(6, 30), out.endMinute);
  TEST_ASSERT_TRUE(parseText("22:00-02:00", out));
  TEST_ASSERT_EQUAL_UINT16(at(22, 0), out.startMinute);
  TEST_ASSERT_EQUAL_UINT16(at(2, 0), out.endMinute);
  TEST_ASSERT_TRUE(parseText("00:00-23:59", out));
}

void test_rejects_malformed_windows() {
  TimeWindow out{at(8, 0), at(9, 0)};
  for (const char* bad :
       {"", "1:35-06:30", "01:35-6:30", "01:35 06:30", "24:00-01:00", "01:60-02:00", "01:35-06:30 ",
        " 01:35-06:30", "aa:bb-cc:dd", "08:00-08:00", "01:35-06:300", "+1:35-06:30"}) {
    TEST_ASSERT_FALSE_MESSAGE(parseText(bad, out), bad);
  }
  TEST_ASSERT_EQUAL_UINT16(at(8, 0), out.startMinute);
  TEST_ASSERT_FALSE(parseTimeWindow(nullptr, 0, out));
}

// A payload is not NUL-terminated: only `length` bytes count.
void test_reads_only_its_length() {
  TimeWindow out{};
  const char raw[] = "01:35-06:30garbage";
  TEST_ASSERT_TRUE(parseTimeWindow(raw, 11, out));
  TEST_ASSERT_EQUAL_UINT16(at(6, 30), out.endMinute);
}

// Start included, end excluded.
void test_window_bounds() {
  const TimeWindows windows = defaults();
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(1, 34)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(1, 35)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(6, 29)));
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(6, 30)));
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(12, 0)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(14, 5)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(17, 4)));
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(17, 5)));
}

void test_window_across_midnight() {
  const TimeWindows windows{1, {{at(22, 0), at(2, 0)}}};
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(21, 59)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(22, 0)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(23, 59)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(0, 0)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(1, 59)));
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(2, 0)));
}

// Overlapping windows: in a window as soon as in one of them.
void test_overlapping_windows() {
  const TimeWindows windows{2, {{at(8, 0), at(10, 0)}, {at(9, 0), at(11, 0)}}};
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(9, 30)));
  TEST_ASSERT_TRUE(inAnyWindow(windows, at(10, 30)));
  TEST_ASSERT_FALSE(inAnyWindow(windows, at(11, 0)));
}

void test_no_window_is_never_in_a_window() {
  const TimeWindows none{0, {}};
  for (uint16_t minute = 0; minute < kMinutesPerDay; minute += 7) {
    TEST_ASSERT_FALSE(inAnyWindow(none, minute));
  }
}

// RG-mode-au-demarrage: what NVS gives back is checked again.
void test_validates_a_stored_set() {
  TEST_ASSERT_TRUE(isValidTimeWindows(defaults()));
  TEST_ASSERT_TRUE(isValidTimeWindows(TimeWindows{0, {}}));
  TimeWindows tooMany = defaults();
  tooMany.count = 5;
  TEST_ASSERT_FALSE(isValidTimeWindows(tooMany));
  TimeWindows empty = defaults();
  empty.windows[1] = {at(9, 0), at(9, 0)};
  TEST_ASSERT_FALSE(isValidTimeWindows(empty));
  TimeWindows outOfDay = defaults();
  outOfDay.windows[0].endMinute = kMinutesPerDay;
  TEST_ASSERT_FALSE(isValidTimeWindows(outOfDay));
}

// Serial: PLAGES 01:35-06:30 14:05-17:05, or PLAGES AUCUNE.
void test_parses_a_serial_list() {
  TimeWindows out{};
  TEST_ASSERT_TRUE(parseTimeWindowList("01:35-06:30 14:05-17:05", out));
  TEST_ASSERT_EQUAL_UINT8(2, out.count);
  TEST_ASSERT_EQUAL_UINT16(at(14, 5), out.windows[1].startMinute);
  TEST_ASSERT_TRUE(parseTimeWindowList("00:00-01:00 02:00-03:00 04:00-05:00 06:00-07:00", out));
  TEST_ASSERT_EQUAL_UINT8(4, out.count);
  TEST_ASSERT_TRUE(parseTimeWindowList("AUCUNE", out));
  TEST_ASSERT_EQUAL_UINT8(0, out.count);
}

void test_rejects_a_bad_serial_list() {
  TimeWindows out = defaults();
  for (const char* bad :
       {"", " ", "01:35-06:30  14:05-17:05", "01:35-06:30 ", "aucune", "01:35-06:30 x",
        "00:00-01:00 02:00-03:00 04:00-05:00 06:00-07:00 08:00-09:00", "08:00-08:00"}) {
    TEST_ASSERT_FALSE_MESSAGE(parseTimeWindowList(bad, out), bad);
  }
  TEST_ASSERT_EQUAL_UINT8(2, out.count);
  TEST_ASSERT_FALSE(parseTimeWindowList(nullptr, out));
}

void test_formats_windows() {
  char text[kTimeWindowsTextSize];
  TEST_ASSERT_TRUE(formatTimeWindows(defaults(), text));
  TEST_ASSERT_EQUAL_STRING("01:35-06:30 14:05-17:05", text);
  TEST_ASSERT_TRUE(formatTimeWindows(TimeWindows{0, {}}, text));
  TEST_ASSERT_EQUAL_STRING("aucune", text);
  TEST_ASSERT_TRUE(
      formatTimeWindows(TimeWindows{4, {{0, 60}, {120, 180}, {240, 300}, {1380, 1439}}}, text));
  TEST_ASSERT_EQUAL_STRING("00:00-01:00 02:00-03:00 04:00-05:00 23:00-23:59", text);
  TimeWindows bad = defaults();
  bad.count = 9;
  TEST_ASSERT_FALSE(formatTimeWindows(bad, text));
}

// RG-chauffage-par-plages: setpoint in a window, floor outside.
void test_scheduled_heating_follows_the_windows() {
  TEST_ASSERT_EQUAL(ScheduledHeating::kSetpoint, scheduledHeating(true, defaults(), at(2, 0)));
  TEST_ASSERT_EQUAL(ScheduledHeating::kFloor, scheduledHeating(true, defaults(), at(12, 0)));
  TEST_ASSERT_EQUAL(ScheduledHeating::kFloor, scheduledHeating(true, TimeWindows{0, {}}, at(2, 0)));
}

// Decision 01-sans-heure: an unknown time heats to the setpoint, never the floor.
void test_unknown_time_heats_to_the_setpoint() {
  TEST_ASSERT_EQUAL(ScheduledHeating::kSetpoint, scheduledHeating(false, defaults(), at(12, 0)));
  TEST_ASSERT_EQUAL(ScheduledHeating::kSetpoint,
                    scheduledHeating(false, TimeWindows{0, {}}, at(12, 0)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_a_window);
  RUN_TEST(test_rejects_malformed_windows);
  RUN_TEST(test_reads_only_its_length);
  RUN_TEST(test_window_bounds);
  RUN_TEST(test_window_across_midnight);
  RUN_TEST(test_overlapping_windows);
  RUN_TEST(test_no_window_is_never_in_a_window);
  RUN_TEST(test_validates_a_stored_set);
  RUN_TEST(test_parses_a_serial_list);
  RUN_TEST(test_rejects_a_bad_serial_list);
  RUN_TEST(test_formats_windows);
  RUN_TEST(test_scheduled_heating_follows_the_windows);
  RUN_TEST(test_unknown_time_heats_to_the_setpoint);
  return UNITY_END();
}
