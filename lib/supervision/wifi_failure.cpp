#include "wifi_failure.h"

namespace terrarium::supervision {

namespace {

// wifi_err_reason_t values, ESP-IDF 4.4, repeated here to keep this file
// free of the SDK.
constexpr uint8_t kAssocLeave = 8;
constexpr uint8_t kFourWayHandshakeTimeout = 15;
constexpr uint8_t kGroupCipherInvalid = 18;
constexpr uint8_t kPairwiseCipherInvalid = 19;
constexpr uint8_t kAkmpInvalid = 20;
constexpr uint8_t kBadCipherOrAkm = 29;
constexpr uint8_t kBeaconTimeout = 200;
constexpr uint8_t kNoApFound = 201;
constexpr uint8_t kAuthFail = 202;
constexpr uint8_t kAssocFail = 203;
constexpr uint8_t kHandshakeTimeout = 204;
constexpr uint8_t kConnectionFail = 205;

}  // namespace

const char* describeWifiFailure(uint8_t reason) {
  switch (reason) {
    case kAssocLeave:
      return nullptr;
    case kNoApFound:
      return "reseau introuvable : absent, trop loin, ou en 5 GHz seulement";
    case kFourWayHandshakeTimeout:
    case kHandshakeTimeout:
      return "mot de passe refuse";
    case kAuthFail:
      return "authentification refusee : mot de passe, ou box en WPA3 seul";
    case kGroupCipherInvalid:
    case kPairwiseCipherInvalid:
    case kAkmpInvalid:
    case kBadCipherOrAkm:
      return "securite non prise en charge : box en WPA3 seul ?";
    case kBeaconTimeout:
      return "signal perdu : trop loin de la box ?";
    case kAssocFail:
    case kConnectionFail:
      return "refus de la box";
    default:
      return "cause non identifiee";
  }
}

}  // namespace terrarium::supervision
