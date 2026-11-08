// Names derived from the board: device id, access point name and password,
// MQTT topics (RG-configuration-wifi-sur-place, RG-topics-mqtt). Pure logic:
// no hardware, no allocation.
#pragma once

#include <cstddef>
#include <cstdint>

namespace terrarium::supervision {

constexpr size_t kDeviceIdSize = 13;      // 12 hex digits + NUL.
constexpr size_t kApSsidSize = 19;        // "ssk-terrarium-XXXX" + NUL.
constexpr size_t kApPasswordLength = 12;  // ~60 bits, typed by hand on a phone.
constexpr size_t kTopicSize = 64;

// The MAC address in lowercase hex, e.g. "246f28a1b2c3".
void formatDeviceId(const uint8_t (&mac)[6], char (&out)[kDeviceIdSize]);

// "ssk-terrarium-" and the last four hex digits of the MAC, uppercase.
void formatApSsid(const uint8_t (&mac)[6], char (&out)[kApSsidSize]);

// Access point password drawn from `random` bytes, over lowercase letters and
// digits without the look-alikes 0, 1, l and o.
void makeApPassword(const uint8_t (&random)[kApPasswordLength], char (&out)[kApPasswordLength + 1]);

// Whether a password read back from NVS has the shape makeApPassword gives.
bool isValidApPassword(const char* password);

// "ssk-terrarium/<deviceId>/<suffix>". Returns false if it does not fit.
bool formatTopic(const char* deviceId, const char* suffix, char* out, size_t size);

}  // namespace terrarium::supervision
