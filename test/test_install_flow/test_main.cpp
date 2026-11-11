#include <unity.h>

#include <cstring>
#include <string>

#include "install_flow.h"

using namespace terrarium::update;

namespace {

constexpr Situation kReady{false, false, true, true};
constexpr uint32_t kStallMs = 60UL * 1000UL;
constexpr uint32_t kDeadlineMs = 5UL * 60UL * 1000UL;
constexpr size_t kPartition = 0x140000;

SemVer version(const char* text) {
  SemVer out{};
  TEST_ASSERT_TRUE_MESSAGE(parseVersion(text, out), text);
  return out;
}

void assertRefusal(Refusal expected, bool admitted, Refusal actual) {
  TEST_ASSERT_FALSE(admitted);
  TEST_ASSERT_EQUAL_STRING(describe(expected), describe(actual));
}

Answer answer(int status, const char* json, Offer& offer, Refusal& refusal) {
  JsonDocument filter;
  offerFilter(filter);
  JsonDocument body;
  if (json != nullptr) {
    TEST_ASSERT_EQUAL(DeserializationError::Ok,
                      deserializeJson(body, json, DeserializationOption::Filter(filter)).code());
  }
  return readAnswer(status, body.as<JsonVariantConst>(), offer, refusal);
}

std::string offerJson(const char* version, const char* url, const char* signature) {
  return std::string("{\"version\":\"") + version +
         "\",\"notes\":\"Corrige la reprise du chauffage.\",\"pub_date\":\"2026-11-11T10:00:00Z\","
         "\"url\":\"" +
         url + "\",\"signature\":\"" + signature + "\"}";
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_a_check_is_refused_only_while_busy_or_without_licence_or_version() {
  Refusal refusal = Refusal::kFault;
  TEST_ASSERT_TRUE(admitCheck(kReady, refusal));
  // A fault does not prevent asking what Skaron offers.
  TEST_ASSERT_TRUE(admitCheck({false, true, true, true}, refusal));
  assertRefusal(Refusal::kBusy, admitCheck({true, false, true, true}, refusal), refusal);
  assertRefusal(Refusal::kNoLicence, admitCheck({false, false, false, true}, refusal), refusal);
  assertRefusal(Refusal::kUnknownVersion, admitCheck({false, false, true, false}, refusal),
                refusal);
}

void test_an_installation_is_refused_during_a_fault() {
  Refusal refusal = Refusal::kBusy;
  TEST_ASSERT_TRUE(admitInstall(kReady, version("1.2.0"), version("1.3.0"), refusal));
  assertRefusal(
      Refusal::kFault,
      admitInstall({false, true, true, true}, version("1.2.0"), version("1.3.0"), refusal),
      refusal);
  // Busy first: the fault would only add to it.
  assertRefusal(Refusal::kBusy,
                admitInstall({true, true, true, true}, version("1.2.0"), version("1.3.0"), refusal),
                refusal);
  assertRefusal(
      Refusal::kNoLicence,
      admitInstall({false, false, false, true}, version("1.2.0"), version("1.3.0"), refusal),
      refusal);
  assertRefusal(
      Refusal::kUnknownVersion,
      admitInstall({false, false, true, false}, version("1.2.0"), version("1.3.0"), refusal),
      refusal);
}

void test_an_installation_of_a_version_not_newer_is_not_offered() {
  Refusal refusal = Refusal::kBusy;
  assertRefusal(Refusal::kNotOffered,
                admitInstall(kReady, version("1.3.0"), version("1.3.0"), refusal), refusal);
  assertRefusal(Refusal::kNotOffered,
                admitInstall(kReady, version("1.3.0"), version("1.2.9"), refusal), refusal);
}

void test_skaron_is_asked_with_the_phase_and_version_in_place() {
  char path[128];
  TEST_ASSERT_TRUE(formatSkaronPath(version("1.2.0"), path, sizeof(path)));
  TEST_ASSERT_EQUAL_STRING("/v1/ssk-terrarium/esp32/phase-1/1.2.0", path);
  SemVer between{};
  TEST_ASSERT_TRUE(fromDescribe("phase-2.1-3-g1a2b3c4", between));
  TEST_ASSERT_TRUE(formatSkaronPath(between, path, sizeof(path)));
  TEST_ASSERT_EQUAL_STRING("/v1/ssk-terrarium/esp32/phase-2/2.1.0+3.g1a2b3c4", path);
  TEST_ASSERT_FALSE(formatSkaronPath(version("1.2.0"), path, 10));
}

void test_skaron_s_status_gives_the_refusal() {
  Offer offer{};
  Refusal refusal = Refusal::kFault;
  TEST_ASSERT_EQUAL(static_cast<int>(Answer::kUpToDate),
                    static_cast<int>(answer(204, nullptr, offer, refusal)));
  const struct {
    int status;
    Refusal refusal;
  } cases[] = {{401, Refusal::kLicenceRefused}, {403, Refusal::kLicenceRefused},
               {429, Refusal::kQuota},          {500, Refusal::kUnreachable},
               {503, Refusal::kUnreachable},    {404, Refusal::kUnreachable},
               {-1, Refusal::kUnreachable},     {0, Refusal::kUnreachable}};
  for (const auto& c : cases) {
    TEST_ASSERT_EQUAL(static_cast<int>(Answer::kRefused),
                      static_cast<int>(answer(c.status, "{\"error\":\"x\"}", offer, refusal)));
    TEST_ASSERT_EQUAL_STRING(describe(c.refusal), describe(refusal));
  }
}

void test_an_offer_is_read_without_its_notes() {
  Offer offer{};
  Refusal refusal = Refusal::kFault;
  const std::string json = offerJson(
      "1.3.0", "https://skaron-artifacts.s3.fr-par.scw.cloud/ssk-terrarium/1.3.0/a.bin?X=1",
      "dW50cnVzdGVk");
  TEST_ASSERT_EQUAL(static_cast<int>(Answer::kOffer),
                    static_cast<int>(answer(200, json.c_str(), offer, refusal)));
  TEST_ASSERT_EQUAL(0, compare(version("1.3.0"), offer.version));
  TEST_ASSERT_EQUAL_STRING(
      "https://skaron-artifacts.s3.fr-par.scw.cloud/ssk-terrarium/1.3.0/a.bin?X=1", offer.url);
  TEST_ASSERT_EQUAL_STRING("dW50cnVzdGVk", offer.signature);
}

void test_a_malformed_offer_means_an_unreachable_service() {
  const std::string bad[] = {
      offerJson("1.3", "https://h/a.bin", "c2ln"),
      offerJson("1.3.0", "http://h/a.bin", "c2ln"),  // TLS is never skipped.
      offerJson("1.3.0", "ftp://h/a.bin", "c2ln"),
      offerJson("1.3.0", "https://h/a.bin", ""),
      offerJson("1.3.0", ("https://h/" + std::string(kUrlSize, 'a')).c_str(), "c2ln"),
      offerJson("1.3.0", "https://h/a.bin", std::string(kSignatureTextSize, 'a').c_str()),
      std::string("{\"version\":\"1.3.0\"}"),
      std::string("[]"),
  };
  for (const std::string& json : bad) {
    Offer offer{};
    Refusal refusal = Refusal::kFault;
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(Answer::kRefused),
                              static_cast<int>(answer(200, json.c_str(), offer, refusal)),
                              json.c_str());
    TEST_ASSERT_EQUAL_STRING(describe(Refusal::kUnreachable), describe(refusal));
  }
}

void test_the_trusted_comment_names_product_phase_and_version() {
  char comment[kTrustedCommentSize];
  TEST_ASSERT_TRUE(formatTrustedComment(version("1.3.0"), comment, sizeof(comment)));
  TEST_ASSERT_EQUAL_STRING("ssk-terrarium esp32-phase-1 1.3.0", comment);
}

void test_only_the_requested_release_of_this_phase_passes() {
  const SemVer current = version("1.2.0");
  const SemVer requested = version("1.3.0");
  TEST_ASSERT_EQUAL(
      static_cast<int>(ReleaseCheck::kOk),
      static_cast<int>(checkRelease("ssk-terrarium esp32-phase-1 1.3.0", current, requested)));
  TEST_ASSERT_EQUAL(
      static_cast<int>(ReleaseCheck::kOtherPhase),
      static_cast<int>(checkRelease("ssk-terrarium esp32-phase-2 1.3.0", current, requested)));
  // An older signed image served under a newer number.
  TEST_ASSERT_EQUAL(
      static_cast<int>(ReleaseCheck::kNotNewer),
      static_cast<int>(checkRelease("ssk-terrarium esp32-phase-1 1.1.0", current, requested)));
  TEST_ASSERT_EQUAL(
      static_cast<int>(ReleaseCheck::kNotNewer),
      static_cast<int>(checkRelease("ssk-terrarium esp32-phase-1 1.2.0", current, requested)));
  for (const char* other :
       {"ssk-terrarium esp32-phase-1 1.4.0", "autre esp32-phase-1 1.3.0",
        "ssk-terrarium esp32-phase-1 1.3.0 ", "ssk-terrarium esp32-phase-1", "",
        "timestamp:1762850000\tfile:firmware.bin\thashed", "ssk-terrarium esp32-phase-x 1.3.0",
        "ssk-terrarium esp32-phase-01 1.3.0", "ssk-terrarium windows-x86_64 1.3.0"}) {
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(ReleaseCheck::kNotRequested),
                              static_cast<int>(checkRelease(other, current, requested)), other);
  }
}

void test_an_image_that_cannot_fit_is_refused_before_writing() {
  DownloadProgress progress(kStallMs);
  Failure failure = Failure::kFault;
  TEST_ASSERT_FALSE(progress.start(kPartition + 1, kPartition, 0, failure));
  TEST_ASSERT_EQUAL_STRING(describe(Failure::kTooLarge), describe(failure));
  TEST_ASSERT_FALSE(progress.start(-1, kPartition, 0, failure));
  TEST_ASSERT_EQUAL_STRING(describe(Failure::kBadImage), describe(failure));
  TEST_ASSERT_FALSE(progress.start(0, kPartition, 0, failure));
  TEST_ASSERT_EQUAL_STRING(describe(Failure::kBadImage), describe(failure));
  TEST_ASSERT_TRUE(progress.start(kPartition, kPartition, 0, failure));
}

void test_progress_is_reported_every_ten_percent() {
  DownloadProgress progress(kStallMs);
  Failure failure = Failure::kFault;
  TEST_ASSERT_TRUE(progress.start(1000, kPartition, 0, failure));
  TEST_ASSERT_EQUAL_UINT8(0, progress.advance(99, 10));
  TEST_ASSERT_EQUAL_UINT8(10, progress.advance(1, 20));
  TEST_ASSERT_EQUAL_UINT8(0, progress.advance(50, 30));
  // A big chunk crossing several steps reports the last one.
  TEST_ASSERT_EQUAL_UINT8(40, progress.advance(260, 40));
  TEST_ASSERT_FALSE(progress.complete());
  TEST_ASSERT_EQUAL(590u, progress.remaining());
  TEST_ASSERT_EQUAL_UINT8(100, progress.advance(590, 50));
  TEST_ASSERT_TRUE(progress.complete());
  TEST_ASSERT_EQUAL(0u, progress.remaining());
}

void test_a_download_without_progress_for_60_s_is_stalled() {
  DownloadProgress progress(kStallMs);
  Failure failure = Failure::kFault;
  TEST_ASSERT_TRUE(progress.start(1000, kPartition, 1000, failure));
  TEST_ASSERT_FALSE(progress.stalled(1000 + kStallMs - 1));
  TEST_ASSERT_TRUE(progress.stalled(1000 + kStallMs));
  progress.advance(10, 50000);
  TEST_ASSERT_FALSE(progress.stalled(50000 + kStallMs - 1));
  progress.advance(0, 100000);  // Nothing received is no progress.
  TEST_ASSERT_TRUE(progress.stalled(50000 + kStallMs));
}

void test_the_stall_survives_the_millis_wrap() {
  DownloadProgress progress(kStallMs);
  Failure failure = Failure::kFault;
  TEST_ASSERT_TRUE(progress.start(1000, kPartition, 0xFFFFFF00u, failure));
  TEST_ASSERT_FALSE(progress.stalled(1000));
  TEST_ASSERT_TRUE(progress.stalled(kStallMs));
}

void test_a_new_firmware_is_valid_once_it_regulates_and_joins_the_broker() {
  ValidationWatch watch(kDeadlineMs);
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kPending),
                    static_cast<int>(watch.update(1000, false, false)));
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kPending),
                    static_cast<int>(watch.update(30000, true, false)));
  // The air measure retained once is enough, even if it is lost since.
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kValid),
                    static_cast<int>(watch.update(90000, false, true)));
}

void test_a_new_firmware_that_does_not_make_it_in_5_minutes_is_turned_down() {
  ValidationWatch broker(kDeadlineMs);
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kPending),
                    static_cast<int>(broker.update(kDeadlineMs - 1, false, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kExpired),
                    static_cast<int>(broker.update(kDeadlineMs, true, true)));
  // Once expired, it stays so.
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kExpired),
                    static_cast<int>(broker.update(kDeadlineMs + 1, true, true)));
  ValidationWatch air(kDeadlineMs);
  TEST_ASSERT_EQUAL(static_cast<int>(Validation::kExpired),
                    static_cast<int>(air.update(kDeadlineMs, true, false)));
}

void test_a_restored_firmware_reports_the_version_turned_down() {
  SemVer attempted{};
  TEST_ASSERT_EQUAL(static_cast<int>(AttemptNote::kCancelled),
                    static_cast<int>(readAttemptNote("1.3.0", true, version("1.2.0"), attempted)));
  TEST_ASSERT_EQUAL(0, compare(version("1.3.0"), attempted));
  // Unknown running version: it cannot be the one tried.
  TEST_ASSERT_EQUAL(static_cast<int>(AttemptNote::kCancelled),
                    static_cast<int>(readAttemptNote("1.3.0", false, SemVer{}, attempted)));
}

void test_the_validated_firmware_just_clears_its_note() {
  SemVer attempted{};
  SemVer running{};
  TEST_ASSERT_TRUE(fromDescribe("phase-1.3", running));
  TEST_ASSERT_EQUAL(static_cast<int>(AttemptNote::kStale),
                    static_cast<int>(readAttemptNote("1.3.0", true, running, attempted)));
  TEST_ASSERT_EQUAL(static_cast<int>(AttemptNote::kStale),
                    static_cast<int>(readAttemptNote("x", true, running, attempted)));
  TEST_ASSERT_EQUAL(static_cast<int>(AttemptNote::kStale),
                    static_cast<int>(readAttemptNote(nullptr, true, running, attempted)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_check_is_refused_only_while_busy_or_without_licence_or_version);
  RUN_TEST(test_an_installation_is_refused_during_a_fault);
  RUN_TEST(test_an_installation_of_a_version_not_newer_is_not_offered);
  RUN_TEST(test_skaron_is_asked_with_the_phase_and_version_in_place);
  RUN_TEST(test_skaron_s_status_gives_the_refusal);
  RUN_TEST(test_an_offer_is_read_without_its_notes);
  RUN_TEST(test_a_malformed_offer_means_an_unreachable_service);
  RUN_TEST(test_the_trusted_comment_names_product_phase_and_version);
  RUN_TEST(test_only_the_requested_release_of_this_phase_passes);
  RUN_TEST(test_an_image_that_cannot_fit_is_refused_before_writing);
  RUN_TEST(test_progress_is_reported_every_ten_percent);
  RUN_TEST(test_a_download_without_progress_for_60_s_is_stalled);
  RUN_TEST(test_the_stall_survives_the_millis_wrap);
  RUN_TEST(test_a_new_firmware_is_valid_once_it_regulates_and_joins_the_broker);
  RUN_TEST(test_a_new_firmware_that_does_not_make_it_in_5_minutes_is_turned_down);
  RUN_TEST(test_a_restored_firmware_reports_the_version_turned_down);
  RUN_TEST(test_the_validated_firmware_just_clears_its_note);
  return UNITY_END();
}
