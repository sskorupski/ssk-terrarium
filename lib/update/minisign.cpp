#include "minisign.h"

#include <cstring>

namespace terrarium::update {

namespace {

// Two bytes of algorithm, then the key id: "Ed" for a key or a legacy
// signature, "ED" for a signature over the BLAKE2b-512 of the file.
constexpr uint8_t kKeyAlgorithm[2] = {'E', 'd'};
constexpr uint8_t kPrehashedAlgorithm[2] = {'E', 'D'};
constexpr size_t kAlgorithmSize = sizeof(kKeyAlgorithm);
constexpr size_t kPublicKeyBytes = kAlgorithmSize + kKeyIdSize + crypto_sign_PUBLICKEYBYTES;
constexpr size_t kSignatureBytes = kAlgorithmSize + kKeyIdSize + crypto_sign_BYTES;
constexpr size_t kHashBytes = crypto_generichash_BYTES_MAX;  // BLAKE2b-512.
static_assert(kHashBytes == 64, "minisign prehashes with BLAKE2b-512");

constexpr char kUntrustedPrefix[] = "untrusted comment:";
constexpr char kTrustedPrefix[] = "trusted comment: ";
constexpr size_t kLineSize = 160;  // The longest line is the trusted comment.

// No sodium_init(): verifying and hashing need no random source nor runtime
// choice of implementation, and the call would link Argon2 and the signing
// tables, 60 kB of a partition already 87 % full.

// Decodes one standard base64 line into exactly `size` bytes.
bool decodeExactly(const char* text, size_t length, uint8_t* out, size_t size) {
  size_t decoded = 0;
  const char* end = nullptr;
  return sodium_base642bin(out, size, text, length, nullptr, &decoded, &end,
                           sodium_base64_VARIANT_ORIGINAL) == 0 &&
         end == text + length && decoded == size;
}

// Copies the next line of `text` into `out`, without its LF (or CRLF), and
// advances `text`. False if there is none, or if it does not fit.
bool nextLine(const char*& text, char (&out)[kLineSize]) {
  if (*text == '\0') return false;
  const char* end = std::strchr(text, '\n');
  size_t length = end != nullptr ? static_cast<size_t>(end - text) : std::strlen(text);
  const char* next = end != nullptr ? end + 1 : text + length;
  if (length > 0 && text[length - 1] == '\r') --length;
  if (length >= sizeof(out)) return false;
  std::memcpy(out, text, length);
  out[length] = '\0';
  text = next;
  return true;
}

// The signature file proper: four lines, the last newline optional.
SignatureError parseFile(const char* text, Signature& out) {
  char line[kLineSize];
  uint8_t raw[kSignatureBytes];
  Signature next{};
  if (!nextLine(text, line) || std::strncmp(line, kUntrustedPrefix, sizeof(kUntrustedPrefix) - 1)) {
    return SignatureError::kMalformed;
  }
  if (!nextLine(text, line) || !decodeExactly(line, std::strlen(line), raw, sizeof(raw))) {
    return SignatureError::kMalformed;
  }
  if (std::memcmp(raw, kKeyAlgorithm, kAlgorithmSize) == 0) return SignatureError::kNotPrehashed;
  if (std::memcmp(raw, kPrehashedAlgorithm, kAlgorithmSize) != 0) return SignatureError::kMalformed;
  std::memcpy(next.keyId, raw + kAlgorithmSize, kKeyIdSize);
  std::memcpy(next.signature, raw + kAlgorithmSize + kKeyIdSize, crypto_sign_BYTES);

  if (!nextLine(text, line) || std::strncmp(line, kTrustedPrefix, sizeof(kTrustedPrefix) - 1)) {
    return SignatureError::kMalformed;
  }
  const char* comment = line + sizeof(kTrustedPrefix) - 1;
  if (std::strlen(comment) >= sizeof(next.trustedComment)) return SignatureError::kMalformed;
  std::strcpy(next.trustedComment, comment);

  if (!nextLine(text, line) ||
      !decodeExactly(line, std::strlen(line), next.globalSignature, crypto_sign_BYTES)) {
    return SignatureError::kMalformed;
  }
  if (*text != '\0') return SignatureError::kMalformed;
  out = next;
  return SignatureError::kNone;
}

}  // namespace

bool parsePublicKey(const char* line, PublicKey& out) {
  if (line == nullptr) return false;
  uint8_t raw[kPublicKeyBytes];
  if (!decodeExactly(line, std::strlen(line), raw, sizeof(raw)) ||
      std::memcmp(raw, kKeyAlgorithm, kAlgorithmSize) != 0) {
    return false;
  }
  std::memcpy(out.keyId, raw + kAlgorithmSize, kKeyIdSize);
  std::memcpy(out.key, raw + kAlgorithmSize + kKeyIdSize, crypto_sign_PUBLICKEYBYTES);
  return true;
}

SignatureError parseSignature(const char* text, Signature& out) {
  if (text == nullptr) return SignatureError::kMalformed;
  if (std::strncmp(text, kUntrustedPrefix, sizeof(kUntrustedPrefix) - 1) == 0) {
    return parseFile(text, out);
  }
  // Base64 of the file, as Skaron carries it: decoded, it is text.
  char file[kSignatureTextSize];
  size_t decoded = 0;
  const char* end = nullptr;
  const size_t length = std::strlen(text);
  if (sodium_base642bin(reinterpret_cast<uint8_t*>(file), sizeof(file) - 1, text, length, nullptr,
                        &decoded, &end, sodium_base64_VARIANT_ORIGINAL) != 0 ||
      end != text + length || decoded == 0) {
    return SignatureError::kMalformed;
  }
  file[decoded] = '\0';
  if (std::strlen(file) != decoded) return SignatureError::kMalformed;  // A NUL inside.
  return parseFile(file, out);
}

SignatureError checkTrustedComment(const Signature& signature, const PublicKey& key) {
  if (std::memcmp(signature.keyId, key.keyId, kKeyIdSize) != 0) return SignatureError::kWrongKey;
  // The global signature covers the image's signature, then the comment.
  uint8_t signed_[crypto_sign_BYTES + kTrustedCommentSize];
  const size_t commentLength = std::strlen(signature.trustedComment);
  std::memcpy(signed_, signature.signature, crypto_sign_BYTES);
  std::memcpy(signed_ + crypto_sign_BYTES, signature.trustedComment, commentLength);
  return crypto_sign_verify_detached(signature.globalSignature, signed_,
                                     crypto_sign_BYTES + commentLength, key.key) == 0
             ? SignatureError::kNone
             : SignatureError::kBadSignature;
}

void ImageHash::begin() { crypto_generichash_init(&state_, nullptr, 0, kHashBytes); }

void ImageHash::update(const uint8_t* data, size_t length) {
  crypto_generichash_update(&state_, data, length);
}

SignatureError ImageHash::verify(const Signature& signature, const PublicKey& key) {
  uint8_t hash[kHashBytes];
  crypto_generichash_final(&state_, hash, sizeof(hash));
  const SignatureError comment = checkTrustedComment(signature, key);
  if (comment != SignatureError::kNone) return comment;
  return crypto_sign_verify_detached(signature.signature, hash, sizeof(hash), key.key) == 0
             ? SignatureError::kNone
             : SignatureError::kBadSignature;
}

}  // namespace terrarium::update
