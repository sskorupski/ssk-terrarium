#include <unity.h>

#include "backoff.h"

using terrarium::supervision::Backoff;

void setUp() {}
void tearDown() {}

void test_first_attempt_is_immediate() {
  Backoff backoff(5000, 300000);
  TEST_ASSERT_TRUE(backoff.ready(0));
}

// Each failure doubles the wait, up to the maximum.
void test_wait_doubles_after_each_failure_up_to_the_maximum() {
  Backoff backoff(5000, 20000);
  backoff.failed(0);
  TEST_ASSERT_FALSE(backoff.ready(4999));
  TEST_ASSERT_TRUE(backoff.ready(5000));
  backoff.failed(5000);
  TEST_ASSERT_FALSE(backoff.ready(14999));
  TEST_ASSERT_TRUE(backoff.ready(15000));
  backoff.failed(15000);
  TEST_ASSERT_TRUE(backoff.ready(35000));
  backoff.failed(35000);
  TEST_ASSERT_FALSE(backoff.ready(54999));
  TEST_ASSERT_TRUE(backoff.ready(55000));
}

void test_success_resets_the_wait() {
  Backoff backoff(5000, 20000);
  backoff.failed(0);
  backoff.failed(5000);
  backoff.succeeded();
  TEST_ASSERT_TRUE(backoff.ready(5001));
  backoff.failed(6000);
  TEST_ASSERT_TRUE(backoff.ready(11000));
}

void test_survives_millis_wraparound() {
  Backoff backoff(5000, 20000);
  backoff.failed(UINT32_MAX - 1000);
  TEST_ASSERT_FALSE(backoff.ready(1000));
  TEST_ASSERT_TRUE(backoff.ready(4000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_attempt_is_immediate);
  RUN_TEST(test_wait_doubles_after_each_failure_up_to_the_maximum);
  RUN_TEST(test_success_resets_the_wait);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}
