#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "posix_tz.h"

using namespace terrarium::control;

void setUp() {}
void tearDown() {}

// Decision 05-fuseau: the default, Paris with its summer time.
void test_accepts_common_timezones() {
  for (const char* good :
       {"CET-1CEST,M3.5.0,M10.5.0/3", "UTC0", "GMT0BST,M3.5.0/1,M10.5.0", "EST5EDT,M3.2.0,M11.1.0",
        "<+0530>-5:30", "<-03>3", "JST-9", "AEST-10AEDT,M10.1.0,M4.1.0/3",
        "NZST-12NZDT,M9.5.0,M4.1.0/3", "CET-1CEST-2,M3.5.0/2:00:00,M10.5.0/3:00:00", "XYZ+24",
        "XYZ-24", "IST-5:30"}) {
    TEST_ASSERT_TRUE_MESSAGE(isValidPosixTimezone(good), good);
  }
}

void test_rejects_malformed_timezones() {
  for (const char* bad :
       {"", "Europe/Paris", "CET", "CE-1", "CET-25", "CET-1:60", "CET-1CEST", "CET-1CEST,M3.5.0",
        "CET-1CEST,M13.5.0,M10.5.0", "CET-1CEST,M3.6.0,M10.5.0", "CET-1CEST,M3.5.7,M10.5.0",
        "CET-1CEST,J60,J300", "CET-1CEST,M3.5.0,M10.5.0/168", "CET-1 ", "CET-1CEST,M3.5.0,M10.5.0,",
        "<+05", "<+5>-5", "CET-1\"", "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV0"}) {
    TEST_ASSERT_FALSE_MESSAGE(isValidPosixTimezone(bad), bad);
  }
  TEST_ASSERT_FALSE(isValidPosixTimezone(nullptr));
}

void test_length_limit() {
  // 47 characters: accepted; 48: refused.
  const char ok[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRST0";
  TEST_ASSERT_EQUAL(47, std::strlen(ok));
  TEST_ASSERT_TRUE(isValidPosixTimezone(ok));
  const char tooLong[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTU0";
  TEST_ASSERT_FALSE(isValidPosixTimezone(tooLong));
}

// A payload is not NUL-terminated: only `length` bytes count.
void test_parses_from_a_payload() {
  char out[kTimezoneSize] = "UTC0";
  const char raw[] = "CET-1CEST,M3.5.0,M10.5.0/3garbage";
  TEST_ASSERT_TRUE(parsePosixTimezone(raw, 26, out));
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out);
  TEST_ASSERT_FALSE(parsePosixTimezone(raw, 33, out));
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out);
  TEST_ASSERT_FALSE(parsePosixTimezone(nullptr, 0, out));
  const char withNul[] = "UTC0\0X";
  TEST_ASSERT_FALSE(parsePosixTimezone(withNul, 6, out));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_common_timezones);
  RUN_TEST(test_rejects_malformed_timezones);
  RUN_TEST(test_length_limit);
  RUN_TEST(test_parses_from_a_payload);
  return UNITY_END();
}
