// Known vectors made with minisign 0.12 and a test key pair, never compiled
// into the firmware: its secret key had no passphrase and was thrown away.
//
//   minisign -G -W -p test.pub -s test.key
//   minisign -S -s test.key -m image.bin -t "ssk-terrarium esp32-phase-1 1.3.0"
#include <unity.h>

#include <cstring>
#include <string>

#include "minisign.h"

using namespace terrarium::update;

namespace {

constexpr char kTestKey[] = "RWSHVvZfUPYsB+be2WT4kDypRtlMv6hUG0l18XxV9VxNsKlCP/n8j4VT";
constexpr char kOtherKey[] = "RWTdhYIfAo8cYn4ZW1PWd95rM4rwnAqpHoaVV2mGV0tx5sHYUB1P4btB";
constexpr char kImage[] = "ssk-terrarium: image de test pour la verification minisign\n";

constexpr char kSignatureFile[] =
    "untrusted comment: signature from minisign secret key\n"
    "RUSHVvZfUPYsB/M2OXi/"
    "K5ImRQwxXCT8fqpYBVsg1+rNXJdT6bYUesHewBsiSHUKTTAzkdyBKcGeH9dLfTUtqa5I+nGg7RYYMw8=\n"
    "trusted comment: ssk-terrarium esp32-phase-1 1.3.0\n"
    "IsyuLz/1HTuJ8Rxce6nUE3gvWW+dK80obfl4bkTpotD2+uaJErZ67aUIe6CeqEurzq/mNjBdnCrE+/gOouZmDw==\n";

// The same file, base64 on one line, as Skaron serves it (Tauri's .sig).
constexpr char kSkaronSignature[] =
    "dW50cnVzdGVkIGNvbW1lbnQ6IHNpZ25hdHVyZSBmcm9tIG1pbmlzaWduIHNlY3JldCBrZXkKUlVTSFZ2WmZVUFlzQi9NMk"
    "9Y"
    "aS9LNUltUlF3eFhDVDhmcXBZQlZzZzErck5YSmRUNmJZVWVzSGV3QnNpU0hVS1RUQXprZHlCS2NHZUg5ZExmVFV0cWE1SS"
    "tu"
    "R2c3UllZTXc4PQp0cnVzdGVkIGNvbW1lbnQ6IHNzay10ZXJyYXJpdW0gZXNwMzItcGhhc2UtMSAxLjMuMApJc3l1THovMU"
    "hU"
    "dUo4UnhjZTZuVUUzZ3ZXVytkSzgwb2JmbDRia1Rwb3REMit1YUpFclo2N2FVSWU2Q2VxRXVyenEvbU5qQmRuQ3JFKy9nT2"
    "91"
    "Wm1Edz09Cg==";

// minisign -S -l: a legacy signature over the whole file.
constexpr char kLegacySignatureFile[] =
    "untrusted comment: signature from minisign secret key\n"
    "RWSHVvZfUPYsBx8rYnkfzD7zRyGRDLFmHjAp5Uuy/dCRUagyvuROb3WgnGXRVY8dawuxoOv7VQ3atyurAtD1/pHAe/"
    "tsaKMOMQY=\n"
    "trusted comment: ssk-terrarium esp32-phase-1 1.3.0\n"
    "xtJOVsnsh3A9iC1t9Tqzpwjb5b/ZpA9SNf0JfnXvl0PLwJDH1DZSZ+7AMIAX6iMOi8iafYEhq+zLbJc2n7sHCA==\n";

PublicKey key(const char* line) {
  PublicKey out{};
  TEST_ASSERT_TRUE(parsePublicKey(line, out));
  return out;
}

Signature signature(const char* text) {
  Signature out{};
  TEST_ASSERT_EQUAL(static_cast<int>(SignatureError::kNone),
                    static_cast<int>(parseSignature(text, out)));
  return out;
}

SignatureError verifyImage(const Signature& sig, const PublicKey& pk, const char* image,
                           size_t chunk) {
  ImageHash hash;
  hash.begin();
  const size_t length = std::strlen(image);
  for (size_t at = 0; at < length; at += chunk) {
    const size_t part = length - at < chunk ? length - at : chunk;
    hash.update(reinterpret_cast<const uint8_t*>(image) + at, part);
  }
  return hash.verify(sig, pk);
}

void assertError(SignatureError expected, SignatureError actual) {
  TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual));
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_the_owner_s_image_verifies_whatever_the_chunks() {
  const PublicKey pk = key(kTestKey);
  const Signature sig = signature(kSkaronSignature);
  assertError(SignatureError::kNone, checkTrustedComment(sig, pk));
  for (size_t chunk : {1u, 7u, 64u, 4096u}) {
    assertError(SignatureError::kNone, verifyImage(sig, pk, kImage, chunk));
  }
}

void test_the_trusted_comment_is_read() {
  TEST_ASSERT_EQUAL_STRING("ssk-terrarium esp32-phase-1 1.3.0",
                           signature(kSkaronSignature).trustedComment);
}

void test_the_signature_file_is_also_accepted_as_is() {
  const Signature sig = signature(kSignatureFile);
  assertError(SignatureError::kNone, verifyImage(sig, key(kTestKey), kImage, 16));
}

void test_one_changed_byte_of_the_image_is_refused() {
  std::string image(kImage);
  image[5] ^= 0x01;
  assertError(SignatureError::kBadSignature,
              verifyImage(signature(kSkaronSignature), key(kTestKey), image.c_str(), 16));
  image = kImage;
  image.pop_back();  // A truncated image.
  assertError(SignatureError::kBadSignature,
              verifyImage(signature(kSkaronSignature), key(kTestKey), image.c_str(), 16));
}

void test_a_changed_trusted_comment_is_refused() {
  std::string text(kSignatureFile);
  text.replace(text.find("1.3.0"), 5, "1.4.0");
  Signature sig = signature(text.c_str());
  assertError(SignatureError::kBadSignature, checkTrustedComment(sig, key(kTestKey)));
  assertError(SignatureError::kBadSignature, verifyImage(sig, key(kTestKey), kImage, 16));
}

void test_another_key_is_refused() {
  const Signature sig = signature(kSkaronSignature);
  assertError(SignatureError::kWrongKey, checkTrustedComment(sig, key(kOtherKey)));
  assertError(SignatureError::kWrongKey, verifyImage(sig, key(kOtherKey), kImage, 16));
}

void test_a_legacy_signature_is_refused() {
  Signature sig{};
  assertError(SignatureError::kNotPrehashed, parseSignature(kLegacySignatureFile, sig));
}

void test_a_malformed_signature_is_refused_whole() {
  Signature sig{};
  sig.trustedComment[0] = 'x';
  std::string missingLine(kSignatureFile);
  missingLine.erase(0, missingLine.find('\n') + 1);
  std::string noTrusted(kSignatureFile);
  noTrusted.replace(noTrusted.find("trusted comment: "), 17, "comment: ");
  std::string shortSig(kSignatureFile);
  shortSig.erase(shortSig.find("Mw8="), 4);
  std::string longComment(kSignatureFile);
  longComment.insert(longComment.find("1.3.0"), std::string(kTrustedCommentSize, 'x'));
  for (const std::string& text : {std::string(), std::string("abc"), std::string("@@@@"),
                                  missingLine, noTrusted, shortSig, longComment}) {
    assertError(SignatureError::kMalformed, parseSignature(text.c_str(), sig));
  }
  assertError(SignatureError::kMalformed, parseSignature(nullptr, sig));
  TEST_ASSERT_EQUAL_CHAR('x', sig.trustedComment[0]);
}

void test_a_malformed_public_key_is_refused() {
  PublicKey pk{};
  for (const char* line : {"", "RWS", "untrusted comment: minisign public key 072CF6505FF65687",
                           // The signature line: right base64, wrong length and algorithm.
                           "RUSHVvZfUPYsB/M2OXi/K5ImRQwxXCT8fqpYBVsg1+rNXJdT6bYUesHewBsiSHUKTTAzkd"
                           "yBKcGeH9dLfTUtqa5I+nGg7RYYMw8="}) {
    TEST_ASSERT_FALSE_MESSAGE(parsePublicKey(line, pk), line);
  }
  TEST_ASSERT_FALSE(parsePublicKey(nullptr, pk));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_owner_s_image_verifies_whatever_the_chunks);
  RUN_TEST(test_the_trusted_comment_is_read);
  RUN_TEST(test_the_signature_file_is_also_accepted_as_is);
  RUN_TEST(test_one_changed_byte_of_the_image_is_refused);
  RUN_TEST(test_a_changed_trusted_comment_is_refused);
  RUN_TEST(test_another_key_is_refused);
  RUN_TEST(test_a_legacy_signature_is_refused);
  RUN_TEST(test_a_malformed_signature_is_refused_whole);
  RUN_TEST(test_a_malformed_public_key_is_refused);
  return UNITY_END();
}
