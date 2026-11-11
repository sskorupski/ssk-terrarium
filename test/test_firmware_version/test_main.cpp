#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "firmware_version.h"

using namespace terrarium::update;

namespace {

SemVer described(const char* text) {
  SemVer version{};
  TEST_ASSERT_TRUE_MESSAGE(fromDescribe(text, version), text);
  return version;
}

SemVer parsed(const char* text) {
  SemVer version{};
  TEST_ASSERT_TRUE_MESSAGE(parseVersion(text, version), text);
  return version;
}

void assertText(const char* expected, const SemVer& version) {
  char text[kVersionTextSize];
  TEST_ASSERT_TRUE(format(version, text));
  TEST_ASSERT_EQUAL_STRING(expected, text);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_a_phase_tag_is_its_major_version() { assertText("1.0.0", described("phase-1")); }

void test_a_fix_tag_is_its_minor_version() { assertText("1.3.0", described("phase-1.3")); }

void test_a_build_between_tags_carries_commits_and_hash() {
  assertText("1.2.0+3.g1a2b3c4", described("phase-1.2-3-g1a2b3c4"));
  assertText("1.0.0+12.gabcdef0", described("phase-1-12-gabcdef0"));
}

void test_uncommitted_changes_are_build_metadata() {
  assertText("1.2.0+3.g1a2b3c4.dirty", described("phase-1.2-3-g1a2b3c4-dirty"));
  assertText("1.0.0+dirty", described("phase-1-dirty"));
}

void test_a_version_without_a_phase_tag_is_unknown() {
  SemVer version{7, 7, 7, "x"};
  for (const char* text :
       {"inconnue", "1a2b3c4", "", "phase-", "phase-x", "phase-1.", "phase-1.2.3", "phase-01",
        "phase-1.2-3", "phase-1.2-3-1a2b3c4", "phase-1.2-x-g1a2b3c4", "phase-1.2-3-g",
        "phase-1.2-3-g1a2b3c4-sale", "Phase-1", "phase-70000"}) {
    TEST_ASSERT_FALSE_MESSAGE(fromDescribe(text, version), text);
  }
  TEST_ASSERT_FALSE(fromDescribe(nullptr, version));
  assertText("7.7.7+x", version);
}

void test_a_commanded_version_is_exactly_x_y_z() {
  assertText("1.3.0", parsed("1.3.0"));
  assertText("10.0.12", parsed("10.0.12"));
  SemVer version{7, 7, 7, ""};
  for (const char* text :
       {"", "1", "1.3", "1.3.0.0", "v1.3.0", "1.3.0+3.gabc", "1.3.0-beta", "01.3.0", "1.03.0",
        "1.3.00", "1..0", " 1.3.0", "1.3.0 ", "1.-3.0", "65536.0.0", "1.3.x"}) {
    TEST_ASSERT_FALSE_MESSAGE(parseVersion(text, version), text);
  }
  TEST_ASSERT_FALSE(parseVersion(nullptr, version));
  assertText("7.7.7", version);
}

void test_zero_parts_are_accepted() { assertText("0.0.0", parsed("0.0.0")); }

void test_precedence_is_numeric() {
  TEST_ASSERT_TRUE(compare(parsed("1.10.0"), parsed("1.2.0")) > 0);
  TEST_ASSERT_TRUE(compare(parsed("1.2.0"), parsed("1.10.0")) < 0);
  TEST_ASSERT_TRUE(compare(parsed("2.0.0"), parsed("1.9.9")) > 0);
  TEST_ASSERT_TRUE(compare(parsed("1.2.1"), parsed("1.2.0")) > 0);
  TEST_ASSERT_EQUAL(0, compare(parsed("1.2.0"), parsed("1.2.0")));
}

void test_a_build_between_tags_ranks_as_its_tag() {
  // Its tag is never offered again, and the next tag is.
  TEST_ASSERT_EQUAL(0, compare(described("phase-1.2-3-g1a2b3c4"), parsed("1.2.0")));
  TEST_ASSERT_TRUE(compare(parsed("1.3.0"), described("phase-1.2-3-g1a2b3c4")) > 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_phase_tag_is_its_major_version);
  RUN_TEST(test_a_fix_tag_is_its_minor_version);
  RUN_TEST(test_a_build_between_tags_carries_commits_and_hash);
  RUN_TEST(test_uncommitted_changes_are_build_metadata);
  RUN_TEST(test_a_version_without_a_phase_tag_is_unknown);
  RUN_TEST(test_a_commanded_version_is_exactly_x_y_z);
  RUN_TEST(test_zero_parts_are_accepted);
  RUN_TEST(test_precedence_is_numeric);
  RUN_TEST(test_a_build_between_tags_ranks_as_its_tag);
  return UNITY_END();
}
