#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "network_settings.h"

using terrarium::supervision::fillFromForm;
using terrarium::supervision::NetworkSettings;
using terrarium::supervision::SettingsError;

namespace {

struct Form {
  const char* ssid = "MaBox";
  const char* wifiPassword = "motdepasse";
  const char* host = "mq.ssk-it.fr";
  const char* port = "443";
  const char* user = "terrarium";
  const char* password = "secret";
};

SettingsError fill(const Form& form, NetworkSettings& out) {
  return fillFromForm(form.ssid, form.wifiPassword, form.host, form.port, form.user, form.password,
                      out);
}

NetworkSettings untouched() {
  NetworkSettings settings{};
  std::strcpy(settings.wifiSsid, "ancien");
  return settings;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_accepts_a_complete_form() {
  NetworkSettings out{};
  TEST_ASSERT_EQUAL(SettingsError::kNone, fill(Form{}, out));
  TEST_ASSERT_EQUAL_STRING("MaBox", out.wifiSsid);
  TEST_ASSERT_EQUAL_STRING("motdepasse", out.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("mq.ssk-it.fr", out.brokerHost);
  TEST_ASSERT_EQUAL_UINT16(443, out.brokerPort);
  TEST_ASSERT_EQUAL_STRING("terrarium", out.mqttUser);
  TEST_ASSERT_EQUAL_STRING("secret", out.mqttPassword);
}

// An open Wi-Fi has no password; a WPA2 one has 8 to 63 characters.
void test_wifi_password_empty_or_wpa2_length() {
  NetworkSettings out{};
  Form form;
  form.wifiPassword = "";
  TEST_ASSERT_EQUAL(SettingsError::kNone, fill(form, out));
  form.wifiPassword = "1234567";
  TEST_ASSERT_EQUAL(SettingsError::kWifiPassword, fill(form, out));
  form.wifiPassword = "123456789012345678901234567890123456789012345678901234567890123";
  TEST_ASSERT_EQUAL(SettingsError::kNone, fill(form, out));
  form.wifiPassword = "1234567890123456789012345678901234567890123456789012345678901234";
  TEST_ASSERT_EQUAL(SettingsError::kWifiPassword, fill(form, out));
}

void test_ssid_one_to_32_bytes() {
  NetworkSettings out{};
  Form form;
  form.ssid = "";
  TEST_ASSERT_EQUAL(SettingsError::kWifiSsid, fill(form, out));
  form.ssid = "12345678901234567890123456789012";
  TEST_ASSERT_EQUAL(SettingsError::kNone, fill(form, out));
  form.ssid = "123456789012345678901234567890123";
  TEST_ASSERT_EQUAL(SettingsError::kWifiSsid, fill(form, out));
}

void test_broker_host_is_a_plain_hostname() {
  NetworkSettings out{};
  Form form;
  form.host = "";
  TEST_ASSERT_EQUAL(SettingsError::kBrokerHost, fill(form, out));
  form.host = "mq.ssk-it.fr/x";
  TEST_ASSERT_EQUAL(SettingsError::kBrokerHost, fill(form, out));
  form.host = "mqtts://mq.ssk-it.fr";
  TEST_ASSERT_EQUAL(SettingsError::kBrokerHost, fill(form, out));
  form.host = "mq ssk-it.fr";
  TEST_ASSERT_EQUAL(SettingsError::kBrokerHost, fill(form, out));
}

void test_broker_port_digits_from_1_to_65535() {
  NetworkSettings out{};
  Form form;
  for (const char* bad : {"", "0", "65536", "44a", "-1", " 443", "4430000000"}) {
    form.port = bad;
    TEST_ASSERT_EQUAL_MESSAGE(SettingsError::kBrokerPort, fill(form, out), bad);
  }
  form.port = "8883";
  TEST_ASSERT_EQUAL(SettingsError::kNone, fill(form, out));
  TEST_ASSERT_EQUAL_UINT16(8883, out.brokerPort);
  form.port = "65535";
  TEST_ASSERT_EQUAL(SettingsError::kNone, fill(form, out));
}

void test_mqtt_account_is_required() {
  NetworkSettings out{};
  Form form;
  form.user = "";
  TEST_ASSERT_EQUAL(SettingsError::kMqttUser, fill(form, out));
  form = Form{};
  form.password = "";
  TEST_ASSERT_EQUAL(SettingsError::kMqttPassword, fill(form, out));
}

// The page posts what the browser sends: a missing field is an error, not a crash.
void test_missing_field_is_rejected() {
  NetworkSettings out{};
  Form form;
  form.ssid = nullptr;
  TEST_ASSERT_EQUAL(SettingsError::kWifiSsid, fill(form, out));
  form = Form{};
  form.port = nullptr;
  TEST_ASSERT_EQUAL(SettingsError::kBrokerPort, fill(form, out));
}

void test_rejected_form_leaves_settings_untouched() {
  NetworkSettings out = untouched();
  Form form;
  form.password = "";
  fill(form, out);
  TEST_ASSERT_EQUAL_STRING("ancien", out.wifiSsid);
}

void test_saved_settings_are_checked_again() {
  NetworkSettings settings{};
  fill(Form{}, settings);
  TEST_ASSERT_EQUAL(SettingsError::kNone, terrarium::supervision::validate(settings));
  settings.brokerPort = 0;
  TEST_ASSERT_EQUAL(SettingsError::kBrokerPort, terrarium::supervision::validate(settings));
}

void test_each_error_reads_in_french() {
  TEST_ASSERT_EQUAL_STRING("Port du courtier : un nombre de 1 a 65535",
                           terrarium::supervision::describe(SettingsError::kBrokerPort));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_a_complete_form);
  RUN_TEST(test_wifi_password_empty_or_wpa2_length);
  RUN_TEST(test_ssid_one_to_32_bytes);
  RUN_TEST(test_broker_host_is_a_plain_hostname);
  RUN_TEST(test_broker_port_digits_from_1_to_65535);
  RUN_TEST(test_mqtt_account_is_required);
  RUN_TEST(test_missing_field_is_rejected);
  RUN_TEST(test_rejected_form_leaves_settings_untouched);
  RUN_TEST(test_saved_settings_are_checked_again);
  RUN_TEST(test_each_error_reads_in_french);
  return UNITY_END();
}
