#include <unity.h>

#include <cstring>

#include "identity.h"

using namespace terrarium::supervision;

namespace {

constexpr uint8_t kMac[6] = {0x24, 0x6f, 0x28, 0xa1, 0xb2, 0xc3};

}  // namespace

void setUp() {}
void tearDown() {}

void test_device_id_is_the_mac_in_lowercase_hex() {
  char id[kDeviceIdSize];
  formatDeviceId(kMac, id);
  TEST_ASSERT_EQUAL_STRING("246f28a1b2c3", id);
}

// RG-configuration-wifi-sur-place: the network name ends with the last four hex digits.
void test_access_point_name_ends_with_the_mac() {
  char ssid[kApSsidSize];
  formatApSsid(kMac, ssid);
  TEST_ASSERT_EQUAL_STRING("ssk-terrarium-B2C3", ssid);
}

void test_access_point_password_has_twelve_unambiguous_characters() {
  uint8_t random[kApPasswordLength];
  for (size_t i = 0; i < kApPasswordLength; ++i) random[i] = static_cast<uint8_t>(i * 37 + 5);
  char password[kApPasswordLength + 1];
  makeApPassword(random, password);
  TEST_ASSERT_EQUAL_size_t(kApPasswordLength, std::strlen(password));
  for (size_t i = 0; i < kApPasswordLength; ++i) {
    TEST_ASSERT_NULL(std::strchr("01lIoO", password[i]));
    TEST_ASSERT_TRUE((password[i] >= 'a' && password[i] <= 'z') ||
                     (password[i] >= '2' && password[i] <= '9'));
  }
}

void test_access_point_password_follows_the_random_bytes() {
  uint8_t zeros[kApPasswordLength] = {};
  uint8_t ones[kApPasswordLength];
  std::memset(ones, 1, sizeof(ones));
  char a[kApPasswordLength + 1];
  char b[kApPasswordLength + 1];
  makeApPassword(zeros, a);
  makeApPassword(ones, b);
  TEST_ASSERT_TRUE(std::strcmp(a, b) != 0);
}

void test_stored_password_is_checked() {
  TEST_ASSERT_TRUE(isValidApPassword("abcdefghjk23"));
  TEST_ASSERT_FALSE(isValidApPassword("abcdefghjk2"));
  TEST_ASSERT_FALSE(isValidApPassword("abcdefghjk2O"));
  TEST_ASSERT_FALSE(isValidApPassword(nullptr));
}

// RG-topics-mqtt: everything lives under ssk-terrarium/<id>/.
void test_topic_under_the_device_prefix() {
  char topic[kTopicSize];
  TEST_ASSERT_TRUE(formatTopic("246f28a1b2c3", "state", topic, sizeof(topic)));
  TEST_ASSERT_EQUAL_STRING("ssk-terrarium/246f28a1b2c3/state", topic);
  TEST_ASSERT_TRUE(formatTopic("246f28a1b2c3", "cmd/#", topic, sizeof(topic)));
  TEST_ASSERT_EQUAL_STRING("ssk-terrarium/246f28a1b2c3/cmd/#", topic);
}

void test_topic_too_long_is_refused() {
  char topic[16];
  TEST_ASSERT_FALSE(formatTopic("246f28a1b2c3", "state", topic, sizeof(topic)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_device_id_is_the_mac_in_lowercase_hex);
  RUN_TEST(test_access_point_name_ends_with_the_mac);
  RUN_TEST(test_access_point_password_has_twelve_unambiguous_characters);
  RUN_TEST(test_access_point_password_follows_the_random_bytes);
  RUN_TEST(test_stored_password_is_checked);
  RUN_TEST(test_topic_under_the_device_prefix);
  RUN_TEST(test_topic_too_long_is_refused);
  return UNITY_END();
}
