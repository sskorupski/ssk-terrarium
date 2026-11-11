// Minisign signatures of a firmware image (RG-image-signee): the owner signs
// on his workstation, the board checks with the public key compiled in.
// Prehashed signatures only ("ED"): the image is hashed with BLAKE2b-512 as it
// is written, never held in memory. Primitives from libsodium, on the board as
// on the host. No hardware, no allocation.
#pragma once

#include <sodium.h>

#include <cstddef>
#include <cstdint>

namespace terrarium::update {

constexpr size_t kKeyIdSize = 8;
constexpr size_t kTrustedCommentSize = 128;
// The signature file as Skaron carries it: base64 of four short lines.
constexpr size_t kSignatureTextSize = 1024;

struct PublicKey {
  uint8_t keyId[kKeyIdSize];
  uint8_t key[crypto_sign_PUBLICKEYBYTES];
};

struct Signature {
  uint8_t keyId[kKeyIdSize];
  uint8_t signature[crypto_sign_BYTES];        // Over the BLAKE2b-512 of the image.
  char trustedComment[kTrustedCommentSize];    // Signed too, by globalSignature.
  uint8_t globalSignature[crypto_sign_BYTES];  // Over signature and trusted comment.
};

enum class SignatureError : uint8_t {
  kNone,
  kMalformed,     // Not a minisign signature file.
  kNotPrehashed,  // A legacy "Ed" signature, over the whole image.
  kWrongKey,      // Signed by another key than the owner's.
  kBadSignature,  // The trusted comment, or the image, does not match.
};

// The second line of a minisign public key file ("RW…"). Returns false,
// leaving `out` untouched, on anything else.
bool parsePublicKey(const char* line, PublicKey& out);

// The minisign signature file, either base64-encoded on one line as Tauri and
// Skaron carry it, or as is. On any error, `out` is left untouched.
SignatureError parseSignature(const char* text, Signature& out);

// Whether `signature` comes from `key` and its trusted comment is genuine.
// Says nothing yet about the image: see ImageHash.
SignatureError checkTrustedComment(const Signature& signature, const PublicKey& key);

// BLAKE2b-512 of the image, fed as it is downloaded, then checked against the
// signature.
class ImageHash {
 public:
  void begin();
  void update(const uint8_t* data, size_t length);
  // Ends the hash. kNone only if `key` signed exactly the bytes fed since
  // begin(), with this trusted comment.
  SignatureError verify(const Signature& signature, const PublicKey& key);

 private:
  crypto_generichash_state state_;
};

}  // namespace terrarium::update
