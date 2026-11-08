#include <unity.h>

#include <initializer_list>

#include "wifi_failure.h"

using terrarium::supervision::describeWifiFailure;

void setUp() {}
void tearDown() {}

// Codes of esp_wifi_types.h (wifi_err_reason_t), ESP-IDF 4.4.
void test_network_not_found_suggests_the_band() {
  TEST_ASSERT_EQUAL_STRING("reseau introuvable : absent, trop loin, ou en 5 GHz seulement",
                           describeWifiFailure(201));
}

void test_handshake_timeouts_mean_a_wrong_password() {
  TEST_ASSERT_EQUAL_STRING("mot de passe refuse", describeWifiFailure(15));
  TEST_ASSERT_EQUAL_STRING("mot de passe refuse", describeWifiFailure(204));
}

void test_authentication_failure_names_both_causes() {
  TEST_ASSERT_EQUAL_STRING("authentification refusee : mot de passe, ou box en WPA3 seul",
                           describeWifiFailure(202));
}

void test_unsupported_security_suggests_wpa3() {
  for (uint8_t reason : {18, 19, 20, 29}) {
    TEST_ASSERT_EQUAL_STRING("securite non prise en charge : box en WPA3 seul ?",
                             describeWifiFailure(reason));
  }
}

void test_lost_signal_suggests_distance() {
  TEST_ASSERT_EQUAL_STRING("signal perdu : trop loin de la box ?", describeWifiFailure(200));
}

// The board leaves on purpose when new settings are saved: nothing to report.
void test_own_disconnection_is_not_a_failure() { TEST_ASSERT_NULL(describeWifiFailure(8)); }

void test_other_reasons_are_generic() {
  TEST_ASSERT_EQUAL_STRING("refus de la box", describeWifiFailure(203));
  TEST_ASSERT_EQUAL_STRING("refus de la box", describeWifiFailure(205));
  TEST_ASSERT_EQUAL_STRING("cause non identifiee", describeWifiFailure(1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_network_not_found_suggests_the_band);
  RUN_TEST(test_handshake_timeouts_mean_a_wrong_password);
  RUN_TEST(test_authentication_failure_names_both_causes);
  RUN_TEST(test_unsupported_security_suggests_wpa3);
  RUN_TEST(test_lost_signal_suggests_distance);
  RUN_TEST(test_own_disconnection_is_not_a_failure);
  RUN_TEST(test_other_reasons_are_generic);
  return UNITY_END();
}
