#include <unity.h>

#include <cstring>

#include "state_message.h"

using terrarium::control::Mode;
using terrarium::supervision::formatState;
using terrarium::supervision::StateSnapshot;

namespace {

StateSnapshot sample() {
  StateSnapshot s{};
  s.airValid = true;
  s.airTemperatureC = 19.24f;
  s.airHumidityRhPercent = 84.4f;
  s.substrateValid = true;
  s.substrateTemperatureC = 20.5f;
  s.heater = true;
  s.mode = Mode::kActive;
  s.setpointC = 19.0f;
  s.hysteresisC = 0.5f;
  s.uptimeS = 3600;
  s.firmwareVersion = "phase-1.1";
  return s;
}

}  // namespace

void setUp() {}
void tearDown() {}

// RG-topics-mqtt: the state carries measures, outputs, mode, setpoint and uptime.
void test_formats_the_whole_state() {
  char out[512];
  TEST_ASSERT_GREATER_THAN(0, formatState(sample(), out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(
      "{\"air\":{\"temperature\":19.2,\"humidite\":84},\"substrat\":{\"temperature\":20.5},"
      "\"sorties\":{\"chauffage\":true,\"eclairage\":false,\"pompe\":false,\"ventilateur\":false},"
      "\"mode\":\"ACTIF\",\"consigne\":19.0,\"hysteresis\":0.5,\"defaut_capteur\":false,"
      "\"pause_chauffage\":false,\"uptime_s\":3600,\"version\":\"phase-1.1\"}",
      out);
}

void test_invalid_readings_are_null() {
  StateSnapshot s = sample();
  s.airValid = false;
  s.substrateValid = false;
  s.sensorFault = true;
  char out[512];
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"air\":{\"temperature\":null,\"humidite\":null}"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"substrat\":{\"temperature\":null}"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"defaut_capteur\":true"));
}

// A version string is quoted as is: one that would break the JSON is replaced.
void test_unsafe_version_is_not_copied() {
  StateSnapshot s = sample();
  s.firmwareVersion = "v1\"}";
  char out[512];
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"version\":\"inconnue\""));
  s.firmwareVersion = nullptr;
  formatState(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"version\":\"inconnue\""));
}

void test_too_small_buffer_gives_zero() {
  char out[32];
  TEST_ASSERT_EQUAL_size_t(0, formatState(sample(), out, sizeof(out)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_formats_the_whole_state);
  RUN_TEST(test_invalid_readings_are_null);
  RUN_TEST(test_unsafe_version_is_not_copied);
  RUN_TEST(test_too_small_buffer_gives_zero);
  return UNITY_END();
}
