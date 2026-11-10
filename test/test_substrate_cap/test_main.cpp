#include <unity.h>

#include <cmath>
#include <cstdint>

#include "substrate_cap.h"

using terrarium::control::SubstrateCap;
using terrarium::control::SubstrateCapChange;

namespace {

// RG-coupure-par-le-substrat: cut at 24 °C, resume strictly below 18.5 °C,
// a silent probe lifts the cut after 2 min.
constexpr float kCutAtC = 24.0f;
constexpr float kResumeBelowC = 18.5f;
constexpr uint32_t kMaxSilentMs = 2UL * 60UL * 1000UL;

SubstrateCap makeCap() { return SubstrateCap(kCutAtC, kResumeBelowC, kMaxSilentMs); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_does_not_cut_below_the_threshold() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(0, true, 23.9f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_cuts_when_the_substrate_reaches_the_threshold() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, cap.update(0, true, 24.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_stays_cut_down_to_the_resume_threshold() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(30000, true, 20.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(60000, true, 18.5f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_resumes_strictly_below_the_resume_threshold() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedCold, cap.update(30000, true, 18.4f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_does_not_cut_again_between_the_thresholds() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  cap.update(30000, true, 18.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(60000, true, 23.9f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_never_cuts_without_a_probe() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(0, false, 99.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(kMaxSilentMs * 10, false, 99.0f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_ignores_implausible_readings() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(0, true, 60.1f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(30000, true, NAN));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_accepts_the_edges_of_the_plausible_range() {
  SubstrateCap cap = makeCap();
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, cap.update(0, true, 60.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedCold, cap.update(30000, true, -20.0f));
}

void test_an_implausible_reading_does_not_resume() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(30000, true, -20.1f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_holds_the_cut_while_the_probe_is_briefly_silent() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(30000, false, 0.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(30000 + kMaxSilentMs, false, 0.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_lifts_the_cut_once_the_probe_is_silent_too_long() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  cap.update(30000, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedSilent,
                    cap.update(30000 + kMaxSilentMs + 1, false, 0.0f));
  TEST_ASSERT_FALSE(cap.cut());
}

void test_a_valid_reading_restarts_the_silence() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  cap.update(30000, false, 0.0f);
  cap.update(90000, true, 22.0f);
  cap.update(120000, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(120000 + kMaxSilentMs, false, 0.0f));
  TEST_ASSERT_TRUE(cap.cut());
}

void test_silence_survives_the_millis_wrap() {
  SubstrateCap cap = makeCap();
  const uint32_t nearWrap = UINT32_MAX - 1000;
  cap.update(nearWrap - 30000, true, 24.0f);
  cap.update(nearWrap, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kNone, cap.update(nearWrap + 60000, false, 0.0f));
  TEST_ASSERT_EQUAL(SubstrateCapChange::kResumedSilent,
                    cap.update(nearWrap + kMaxSilentMs + 1, false, 0.0f));
}

void test_cuts_again_after_a_silent_resume() {
  SubstrateCap cap = makeCap();
  cap.update(0, true, 24.0f);
  cap.update(30000, false, 0.0f);
  cap.update(30000 + kMaxSilentMs + 1, false, 0.0f);
  TEST_ASSERT_EQUAL(SubstrateCapChange::kCut, cap.update(300000, true, 24.5f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_does_not_cut_below_the_threshold);
  RUN_TEST(test_cuts_when_the_substrate_reaches_the_threshold);
  RUN_TEST(test_stays_cut_down_to_the_resume_threshold);
  RUN_TEST(test_resumes_strictly_below_the_resume_threshold);
  RUN_TEST(test_does_not_cut_again_between_the_thresholds);
  RUN_TEST(test_never_cuts_without_a_probe);
  RUN_TEST(test_ignores_implausible_readings);
  RUN_TEST(test_accepts_the_edges_of_the_plausible_range);
  RUN_TEST(test_an_implausible_reading_does_not_resume);
  RUN_TEST(test_holds_the_cut_while_the_probe_is_briefly_silent);
  RUN_TEST(test_lifts_the_cut_once_the_probe_is_silent_too_long);
  RUN_TEST(test_a_valid_reading_restarts_the_silence);
  RUN_TEST(test_silence_survives_the_millis_wrap);
  RUN_TEST(test_cuts_again_after_a_silent_resume);
  return UNITY_END();
}
