#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "day_night.h"

using terrarium::control::DayNight;
using terrarium::control::DayNightMode;
using terrarium::control::DayWindow;
using terrarium::control::Moment;
using terrarium::control::Season;

namespace {

uint16_t day(const char* monthDay) {
  uint16_t out = 0;
  TEST_ASSERT_TRUE_MESSAGE(terrarium::control::parseMonthDay(monthDay, std::strlen(monthDay), out),
                           monthDay);
  return out;
}

constexpr uint16_t hm(uint16_t hour, uint16_t minute) { return hour * 60 + minute; }

// The four seasons of the cadrage's example (2026-015, step 3).
DayNight fourSeasons() {
  DayNight settings{};
  settings.mode = DayNightMode::kSeasons;
  settings.seasonCount = 4;
  settings.seasons[0] = {day("03-20"), {hm(7, 0), hm(20, 0)}};
  settings.seasons[1] = {day("06-21"), {hm(6, 0), hm(22, 0)}};
  settings.seasons[2] = {day("09-22"), {hm(7, 30), hm(19, 30)}};
  settings.seasons[3] = {day("12-21"), {hm(8, 30), hm(17, 30)}};
  return settings;
}

DayNight metronome(uint16_t start, uint16_t end) {
  DayNight settings{};
  settings.mode = DayNightMode::kMetronome;
  settings.fixed = {start, end};
  return settings;
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-jour-et-nuit: days count on a 365-day year, 29 February excluded.
void test_parses_a_month_and_day() {
  TEST_ASSERT_EQUAL_UINT16(0, day("01-01"));
  TEST_ASSERT_EQUAL_UINT16(31, day("02-01"));
  TEST_ASSERT_EQUAL_UINT16(58, day("02-28"));
  TEST_ASSERT_EQUAL_UINT16(59, day("03-01"));
  TEST_ASSERT_EQUAL_UINT16(364, day("12-31"));
}

void test_refuses_a_bad_month_and_day() {
  uint16_t out = 7;
  for (const char* text :
       {"02-29", "02-30", "04-31", "13-01", "00-10", "01-00", "1-01", "01/01", "01-1", "aa-bb"}) {
    TEST_ASSERT_FALSE_MESSAGE(terrarium::control::parseMonthDay(text, std::strlen(text), out),
                              text);
  }
  TEST_ASSERT_EQUAL_UINT16(7, out);
}

void test_formats_a_month_and_day() {
  char text[terrarium::control::kMonthDayTextLength + 1];
  TEST_ASSERT_TRUE(terrarium::control::formatMonthDay(day("06-21"), text));
  TEST_ASSERT_EQUAL_STRING("06-21", text);
  TEST_ASSERT_FALSE(terrarium::control::formatMonthDay(365, text));
}

// The calendar date maps a leap day onto 28 February.
void test_day_of_year_from_a_date() {
  TEST_ASSERT_EQUAL_UINT16(58, terrarium::control::dayOfYear(2, 28));
  TEST_ASSERT_EQUAL_UINT16(58, terrarium::control::dayOfYear(2, 29));
  TEST_ASSERT_EQUAL_UINT16(59, terrarium::control::dayOfYear(3, 1));
  TEST_ASSERT_EQUAL_UINT16(364, terrarium::control::dayOfYear(12, 31));
}

// A day does not cross midnight, lasts at least an hour, and leaves a night
// of at least an hour.
void test_day_window_bounds() {
  TEST_ASSERT_TRUE(terrarium::control::isValidDayWindow({hm(8, 0), hm(21, 0)}));
  TEST_ASSERT_TRUE(terrarium::control::isValidDayWindow({hm(8, 0), hm(9, 0)}));
  TEST_ASSERT_TRUE(terrarium::control::isValidDayWindow({hm(0, 30), hm(23, 30)}));
  TEST_ASSERT_FALSE(terrarium::control::isValidDayWindow({hm(8, 0), hm(8, 59)}));
  TEST_ASSERT_FALSE(terrarium::control::isValidDayWindow({hm(21, 0), hm(8, 0)}));
  TEST_ASSERT_FALSE(terrarium::control::isValidDayWindow({hm(0, 29), hm(23, 30)}));
  TEST_ASSERT_FALSE(terrarium::control::isValidDayWindow({hm(8, 0), 1440}));
}

void test_valid_settings() {
  TEST_ASSERT_TRUE(terrarium::control::isValidDayNight(metronome(hm(8, 0), hm(21, 0))));
  TEST_ASSERT_TRUE(terrarium::control::isValidDayNight(fourSeasons()));
  DayNight one{};
  one.mode = DayNightMode::kSeasons;
  one.seasonCount = 1;
  one.seasons[0] = {day("01-01"), {hm(8, 0), hm(18, 0)}};
  TEST_ASSERT_TRUE(terrarium::control::isValidDayNight(one));
}

void test_invalid_settings() {
  DayNight none = fourSeasons();
  none.seasonCount = 0;
  TEST_ASSERT_FALSE(terrarium::control::isValidDayNight(none));
  DayNight seven = fourSeasons();
  seven.seasonCount = 7;
  TEST_ASSERT_FALSE(terrarium::control::isValidDayNight(seven));
  DayNight twice = fourSeasons();
  twice.seasons[1].startDay = twice.seasons[0].startDay;
  TEST_ASSERT_FALSE(terrarium::control::isValidDayNight(twice));
  DayNight badDay = fourSeasons();
  badDay.seasons[2].day = {hm(20, 0), hm(7, 0)};
  TEST_ASSERT_FALSE(terrarium::control::isValidDayNight(badDay));
  TEST_ASSERT_FALSE(terrarium::control::isValidDayNight(metronome(hm(21, 0), hm(8, 0))));
}

// Seasons are kept in date order, whatever order they were given in.
void test_sorts_seasons_by_date() {
  DayNight settings = fourSeasons();
  const Season first = settings.seasons[0];
  settings.seasons[0] = settings.seasons[3];
  settings.seasons[3] = first;
  terrarium::control::sortSeasons(settings);
  TEST_ASSERT_EQUAL_UINT16(day("03-20"), settings.seasons[0].startDay);
  TEST_ASSERT_EQUAL_UINT16(day("12-21"), settings.seasons[3].startDay);
}

void test_metronome_is_the_same_every_day() {
  const DayNight settings = metronome(hm(8, 0), hm(21, 0));
  for (uint16_t d : {0, 100, 364}) {
    const DayWindow window = terrarium::control::dayFor(settings, d);
    TEST_ASSERT_EQUAL_UINT16(hm(8, 0), window.startMinute);
    TEST_ASSERT_EQUAL_UINT16(hm(21, 0), window.endMinute);
  }
}

// On the first day of a season, its own day.
void test_season_start_gives_its_own_day() {
  const DayWindow window = terrarium::control::dayFor(fourSeasons(), day("06-21"));
  TEST_ASSERT_EQUAL_UINT16(hm(6, 0), window.startMinute);
  TEST_ASSERT_EQUAL_UINT16(hm(22, 0), window.endMinute);
}

// The cadrage's example: 5 May, 46 days after 20 March out of 93.
void test_day_slides_towards_the_next_season() {
  const DayWindow window = terrarium::control::dayFor(fourSeasons(), day("05-05"));
  TEST_ASSERT_EQUAL_UINT16(hm(6, 30), window.startMinute);
  TEST_ASSERT_EQUAL_UINT16(hm(20, 59), window.endMinute);
}

// The day before a season starts is one step short of it.
void test_day_before_the_next_season() {
  const DayWindow window = terrarium::control::dayFor(fourSeasons(), day("06-20"));
  TEST_ASSERT_EQUAL_UINT16(hm(6, 1), window.startMinute);
  TEST_ASSERT_EQUAL_UINT16(hm(21, 59), window.endMinute);
}

// Seasons loop over the year: before the first one, the last one runs.
void test_seasons_loop_over_the_new_year() {
  const DayNight settings = fourSeasons();
  const DayWindow january = terrarium::control::dayFor(settings, day("01-01"));
  // 11 days after 21 December, out of 89 to 20 March.
  TEST_ASSERT_EQUAL_UINT16(hm(8, 19), january.startMinute);
  TEST_ASSERT_EQUAL_UINT16(hm(17, 49), january.endMinute);
  const DayWindow lastDay = terrarium::control::dayFor(settings, day("03-19"));
  TEST_ASSERT_EQUAL_UINT16(hm(7, 1), lastDay.startMinute);
  TEST_ASSERT_EQUAL_UINT16(hm(19, 58), lastDay.endMinute);
}

void test_a_single_season_holds_all_year() {
  DayNight one{};
  one.mode = DayNightMode::kSeasons;
  one.seasonCount = 1;
  one.seasons[0] = {day("04-01"), {hm(7, 0), hm(19, 0)}};
  for (uint16_t d : {0, 90, 91, 364}) {
    const DayWindow window = terrarium::control::dayFor(one, d);
    TEST_ASSERT_EQUAL_UINT16(hm(7, 0), window.startMinute);
    TEST_ASSERT_EQUAL_UINT16(hm(19, 0), window.endMinute);
  }
}

// Every day of the year stays a valid day.
void test_every_interpolated_day_is_valid() {
  const DayNight settings = fourSeasons();
  for (uint16_t d = 0; d < 365; ++d) {
    TEST_ASSERT_TRUE(terrarium::control::isValidDayWindow(terrarium::control::dayFor(settings, d)));
  }
}

// Start included, end excluded; unknown time is day.
void test_moment_of_the_day() {
  const DayNight settings = metronome(hm(8, 0), hm(21, 0));
  TEST_ASSERT_EQUAL(Moment::kNight, terrarium::control::momentAt(settings, true, 10, hm(7, 59)));
  TEST_ASSERT_EQUAL(Moment::kDay, terrarium::control::momentAt(settings, true, 10, hm(8, 0)));
  TEST_ASSERT_EQUAL(Moment::kDay, terrarium::control::momentAt(settings, true, 10, hm(20, 59)));
  TEST_ASSERT_EQUAL(Moment::kNight, terrarium::control::momentAt(settings, true, 10, hm(21, 0)));
  TEST_ASSERT_EQUAL(Moment::kDay, terrarium::control::momentAt(settings, false, 10, hm(23, 0)));
}

void test_parses_and_formats_a_day_window() {
  DayWindow window{};
  TEST_ASSERT_TRUE(terrarium::control::parseDayWindow("08:00-21:00", 11, window));
  TEST_ASSERT_EQUAL_UINT16(hm(8, 0), window.startMinute);
  TEST_ASSERT_EQUAL_UINT16(hm(21, 0), window.endMinute);
  TEST_ASSERT_FALSE(terrarium::control::parseDayWindow("21:00-08:00", 11, window));
  TEST_ASSERT_FALSE(terrarium::control::parseDayWindow("08:00 21:00", 11, window));
  char text[terrarium::control::kDayWindowTextLength + 1];
  TEST_ASSERT_TRUE(terrarium::control::formatDayWindow({hm(6, 30), hm(20, 59)}, text));
  TEST_ASSERT_EQUAL_STRING("06:30-20:59", text);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_a_month_and_day);
  RUN_TEST(test_refuses_a_bad_month_and_day);
  RUN_TEST(test_formats_a_month_and_day);
  RUN_TEST(test_day_of_year_from_a_date);
  RUN_TEST(test_day_window_bounds);
  RUN_TEST(test_valid_settings);
  RUN_TEST(test_invalid_settings);
  RUN_TEST(test_sorts_seasons_by_date);
  RUN_TEST(test_metronome_is_the_same_every_day);
  RUN_TEST(test_season_start_gives_its_own_day);
  RUN_TEST(test_day_slides_towards_the_next_season);
  RUN_TEST(test_day_before_the_next_season);
  RUN_TEST(test_seasons_loop_over_the_new_year);
  RUN_TEST(test_a_single_season_holds_all_year);
  RUN_TEST(test_every_interpolated_day_is_valid);
  RUN_TEST(test_moment_of_the_day);
  RUN_TEST(test_parses_and_formats_a_day_window);
  return UNITY_END();
}
