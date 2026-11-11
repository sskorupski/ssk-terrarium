#include <unity.h>

#include <cstring>
#include <initializer_list>
#include <string>

#include "update_command.h"

using terrarium::supervision::kEventSize;
using namespace terrarium::update;

namespace {

// A licence of the size Skaron issues.
constexpr char kLicence[] =
    "eyJhbGciOiJFZERTQSIsInR5cCI6IkpXVCIsImtpZCI6ImsyMDI2In0.eyJzdWIiOiIyNDZmMjhhMWIyYzMiLCJwcm"
    "QiOiJzc2stdGVycmFyaXVtIiwiZWQiOiJyZWd1bGF0ZXVyIiwianRpIjoiMDFKQ1hZWiIsImlhdCI6MTc2MjgwMDAw"
    "MCwiZXhwIjoxNzk0MzM2MDAwfQ.c2lnbmF0dXJlLWVkMjU1MTktZGUtc2thcm9uLXN1ci1zb2l4YW50ZS1xdWF0cmU"
    "tb2N0ZXRz";

UpdateCommandError parse(const std::string& json, UpdateCommand& out) {
  return parseUpdatePayload(reinterpret_cast<const uint8_t*>(json.data()), json.size(), out);
}

void assertError(UpdateCommandError expected, const std::string& json) {
  UpdateCommand command{};
  command.licence[0] = 'x';
  TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(expected), static_cast<int>(parse(json, command)),
                            json.c_str());
  TEST_ASSERT_EQUAL_CHAR('x', command.licence[0]);
}

SemVer version(const char* text) {
  SemVer out{};
  TEST_ASSERT_TRUE(parseVersion(text, out));
  return out;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_a_licence_is_kept_verbatim() {
  UpdateCommand command{};
  TEST_ASSERT_EQUAL(
      static_cast<int>(UpdateCommandError::kNone),
      static_cast<int>(parse(std::string("{\"licence\":\"") + kLicence + "\"}", command)));
  TEST_ASSERT_EQUAL(static_cast<int>(UpdateAction::kLicence), static_cast<int>(command.action));
  TEST_ASSERT_EQUAL_STRING(kLicence, command.licence);
}

void test_a_licence_that_is_empty_too_long_or_odd_is_refused() {
  assertError(UpdateCommandError::kBadLicence, "{\"licence\":\"\"}");
  assertError(UpdateCommandError::kBadLicence, "{\"licence\":12}");
  assertError(UpdateCommandError::kBadLicence,
              "{\"licence\":\"" + std::string(kLicenceMaxLength + 1, 'a') + "\"}");
  // Only base64url and dots: nothing that could break a header or a line.
  for (const char* odd : {"a b", "a\\nb", "a\\\"b", "a:b", "a\\u0000b"}) {
    assertError(UpdateCommandError::kBadLicence, std::string("{\"licence\":\"") + odd + "\"}");
  }
  UpdateCommand command{};
  TEST_ASSERT_EQUAL(static_cast<int>(UpdateCommandError::kNone),
                    static_cast<int>(parse(
                        "{\"licence\":\"" + std::string(kLicenceMaxLength, 'a') + "\"}", command)));
}

void test_check_takes_no_argument() {
  UpdateCommand command{};
  TEST_ASSERT_EQUAL(static_cast<int>(UpdateCommandError::kNone),
                    static_cast<int>(parse("{\"action\":\"verifier\"}", command)));
  TEST_ASSERT_EQUAL(static_cast<int>(UpdateAction::kCheck), static_cast<int>(command.action));
  assertError(UpdateCommandError::kUnknownField, "{\"action\":\"verifier\",\"version\":\"1.3.0\"}");
}

void test_install_names_an_exact_version() {
  UpdateCommand command{};
  TEST_ASSERT_EQUAL(
      static_cast<int>(UpdateCommandError::kNone),
      static_cast<int>(parse("{\"action\":\"installer\",\"version\":\"1.3.0\"}", command)));
  TEST_ASSERT_EQUAL(static_cast<int>(UpdateAction::kInstall), static_cast<int>(command.action));
  TEST_ASSERT_EQUAL(0, compare(version("1.3.0"), command.version));
  for (const char* bad :
       {"{\"action\":\"installer\"}", "{\"action\":\"installer\",\"version\":\"1.3\"}",
        "{\"action\":\"installer\",\"version\":\"1.3.0+3.gabc\"}",
        "{\"action\":\"installer\",\"version\":130}"}) {
    assertError(UpdateCommandError::kBadVersion, bad);
  }
}

void test_a_command_names_no_source() {
  // Only a version: never a URL, a host or an image (RG-mise-a-jour-a-distance).
  assertError(UpdateCommandError::kUnknownField,
              "{\"action\":\"installer\",\"version\":\"1.3.0\",\"url\":\"https://x/y.bin\"}");
  assertError(UpdateCommandError::kUnknownField, "{\"hote\":\"x\"}");
  assertError(UpdateCommandError::kUnknownField, "{}");
  assertError(UpdateCommandError::kUnknownField,
              std::string("{\"licence\":\"") + kLicence + "\",\"action\":\"verifier\"}");
}

void test_other_actions_and_malformed_json_are_refused() {
  assertError(UpdateCommandError::kBadAction, "{\"action\":\"redemarrer\"}");
  assertError(UpdateCommandError::kBadAction, "{\"action\":1}");
  assertError(UpdateCommandError::kMalformed, "verifier");
  assertError(UpdateCommandError::kMalformed, "[\"verifier\"]");
  assertError(UpdateCommandError::kMalformed, "");
  TEST_ASSERT_EQUAL_STRING("JSON invalide", describe(UpdateCommandError::kMalformed));
  TEST_ASSERT_EQUAL_STRING("champ inconnu", describe(UpdateCommandError::kUnknownField));
  TEST_ASSERT_EQUAL_STRING("action inconnue (verifier, installer)",
                           describe(UpdateCommandError::kBadAction));
  TEST_ASSERT_EQUAL_STRING("version invalide (x.y.z)", describe(UpdateCommandError::kBadVersion));
}

void test_the_serial_link_takes_a_licence_too() {
  char licence[kLicenceSize] = "x";
  TEST_ASSERT_EQUAL(static_cast<int>(LicenceLine::kNone),
                    static_cast<int>(parseLicenceLine("MODE ACTIF", licence)));
  TEST_ASSERT_EQUAL(static_cast<int>(LicenceLine::kNone),
                    static_cast<int>(parseLicenceLine("licences", licence)));
  for (const char* bad : {"licence", "licence ", "LICENCE a b", "licence a:b"}) {
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(LicenceLine::kInvalid),
                              static_cast<int>(parseLicenceLine(bad, licence)), bad);
  }
  TEST_ASSERT_EQUAL_STRING("x", licence);
  TEST_ASSERT_EQUAL(static_cast<int>(LicenceLine::kValid),
                    static_cast<int>(parseLicenceLine(
                        (std::string("licence ") + kLicence + "\r").c_str(), licence)));
  TEST_ASSERT_EQUAL_STRING(kLicence, licence);
  TEST_ASSERT_EQUAL(static_cast<int>(LicenceLine::kValid),
                    static_cast<int>(parseLicenceLine("LICENCE abc.def", licence)));
  TEST_ASSERT_EQUAL_STRING("abc.def", licence);
}

void test_licence_events_never_carry_the_token() {
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatLicenceEvent(LicenceOutcome::kApplied, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"licence\",\"resultat\":\"applique\"}", event);
  TEST_ASSERT_TRUE(formatLicenceEvent(LicenceOutcome::kNotSaved, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"licence\",\"resultat\":"
      "\"applique_non_enregistre\"}",
      event);
  TEST_ASSERT_TRUE(formatLicenceEvent(LicenceOutcome::kRefused, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"licence\",\"resultat\":\"refuse\"}", event);
}

void test_refusals_give_their_reason() {
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatRefusalEvent(Refusal::kBusy, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"refusee\",\"raison\":\"mise a jour en "
      "cours\"}",
      event);
  const struct {
    Refusal refusal;
    const char* reason;
  } reasons[] = {{Refusal::kBusy, "mise a jour en cours"},
                 {Refusal::kNoLicence, "sans licence"},
                 {Refusal::kNotOffered, "version non proposee"},
                 {Refusal::kLicenceRefused, "licence refusee"},
                 {Refusal::kQuota, "quota depasse"},
                 {Refusal::kUnreachable, "service injoignable"},
                 {Refusal::kUnknownVersion, "version inconnue"}};
  for (const auto& r : reasons) {
    TEST_ASSERT_EQUAL_STRING(r.reason, describe(r.refusal));
    TEST_ASSERT_TRUE(formatRefusalEvent(r.refusal, event));
  }
}

void test_steps_carry_their_version() {
  char event[kEventSize];
  const struct {
    VersionStep step;
    const char* name;
  } steps[] = {{VersionStep::kAvailable, "disponible"},
               {VersionStep::kUpToDate, "a-jour"},
               {VersionStep::kRestart, "redemarrage"},
               {VersionStep::kValidated, "validee"},
               {VersionStep::kCancelled, "annulee"}};
  for (const auto& s : steps) {
    TEST_ASSERT_TRUE(formatVersionEvent(s.step, version("1.3.0"), event));
    TEST_ASSERT_EQUAL_STRING((std::string("{\"evenement\":\"mise-a-jour\",\"etape\":\"") + s.name +
                              "\",\"version\":\"1.3.0\"}")
                                 .c_str(),
                             event);
  }
  SemVer between{};
  TEST_ASSERT_TRUE(fromDescribe("phase-1.2-3-g1a2b3c4", between));
  TEST_ASSERT_TRUE(formatVersionEvent(VersionStep::kUpToDate, between, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"a-jour\",\"version\":\"1.2.0+3.g1a2b3c4\"}",
      event);
}

void test_progress_is_a_whole_percentage() {
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatProgressEvent(version("1.3.0"), 40, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"telechargement\",\"version\":\"1.3.0\","
      "\"progression\":40}",
      event);
}

void test_failures_give_their_reason() {
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatFailureEvent(version("1.3.0"), Failure::kBadSignature, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"echec\",\"version\":\"1.3.0\","
      "\"raison\":\"signature invalide\"}",
      event);
  const struct {
    Failure failure;
    const char* reason;
  } reasons[] = {{Failure::kStalled, "telechargement interrompu"},
                 {Failure::kBadSignature, "signature invalide"},
                 {Failure::kBadImage, "image invalide"},
                 {Failure::kTooLarge, "image trop grande"},
                 {Failure::kNotNewer, "version non plus recente"},
                 {Failure::kOtherPhase, "phase differente"}};
  for (const auto& r : reasons) {
    TEST_ASSERT_EQUAL_STRING(r.reason, describe(r.failure));
    TEST_ASSERT_TRUE(formatFailureEvent(version("65535.65535.65535"), r.failure, event));
  }
}

void test_an_acceptance_without_fault_carries_only_its_version() {
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatAcceptedEvent(version("1.3.0"), 0, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"acceptee\",\"version\":\"1.3.0\"}", event);
}

void test_an_acceptance_during_a_fault_names_it() {
  // A fault no longer prevents an installation: it is said (RG-mise-a-jour-pendant-un-defaut).
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatAcceptedEvent(version("1.3.0"), kDht11Fault | kSubstrateRejected, event));
  TEST_ASSERT_EQUAL_STRING(
      "{\"evenement\":\"mise-a-jour\",\"etape\":\"acceptee\",\"version\":\"1.3.0\","
      "\"defauts\":[\"sonde dht11\",\"substrat rejete\"]}",
      event);
}

void test_each_fault_has_its_name() {
  const struct {
    UpdateFaults fault;
    const char* name;
  } faults[] = {{kAirFault, "defaut capteur"},       {kSht45Fault, "sonde sht45"},
                {kDht11Fault, "sonde dht11"},        {kSubstrateRejected, "substrat rejete"},
                {kSubstrateCut, "coupure substrat"}, {kHeatPaused, "pause chauffe"}};
  char event[kEventSize];
  for (const auto& f : faults) {
    TEST_ASSERT_TRUE(formatWarningEvent(version("1.3.0"), f.fault, event));
    TEST_ASSERT_EQUAL_STRING(
        (std::string("{\"evenement\":\"mise-a-jour\",\"etape\":"
                     "\"avertissement\",\"version\":\"1.3.0\",\"defauts\":[\"") +
         f.name + "\"]}")
            .c_str(),
        event);
  }
}

void test_every_fault_at_once_fits_an_event() {
  char event[kEventSize];
  TEST_ASSERT_TRUE(formatWarningEvent(version("65535.65535.65535"), kAllUpdateFaults, event));
  TEST_ASSERT_TRUE(formatAcceptedEvent(version("65535.65535.65535"), kAllUpdateFaults, event));
}

void test_a_warning_always_names_a_fault() {
  char event[kEventSize];
  TEST_ASSERT_FALSE(formatWarningEvent(version("1.3.0"), 0, event));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_licence_is_kept_verbatim);
  RUN_TEST(test_a_licence_that_is_empty_too_long_or_odd_is_refused);
  RUN_TEST(test_check_takes_no_argument);
  RUN_TEST(test_install_names_an_exact_version);
  RUN_TEST(test_a_command_names_no_source);
  RUN_TEST(test_other_actions_and_malformed_json_are_refused);
  RUN_TEST(test_the_serial_link_takes_a_licence_too);
  RUN_TEST(test_licence_events_never_carry_the_token);
  RUN_TEST(test_refusals_give_their_reason);
  RUN_TEST(test_steps_carry_their_version);
  RUN_TEST(test_progress_is_a_whole_percentage);
  RUN_TEST(test_failures_give_their_reason);
  RUN_TEST(test_an_acceptance_without_fault_carries_only_its_version);
  RUN_TEST(test_an_acceptance_during_a_fault_names_it);
  RUN_TEST(test_each_fault_has_its_name);
  RUN_TEST(test_every_fault_at_once_fits_an_event);
  RUN_TEST(test_a_warning_always_names_a_fault);
  return UNITY_END();
}
