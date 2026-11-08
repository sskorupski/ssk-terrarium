// MQTT over TLS to the broker (ADR-supervision-par-mqtt-tls, RG-topics-mqtt).
// Runs in the supervision task, never in the regulation loop: connecting may
// take seconds.
#pragma once

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include <cstddef>
#include <cstdint>

#include "backoff.h"
#include "identity.h"
#include "network_settings.h"

namespace terrarium::mqtt {

class MqttLink {
 public:
  MqttLink() : client_(tls_) {}

  // Broker, account and topics. Drops any open connection.
  void configure(const supervision::NetworkSettings& settings, const char* deviceId);

  // Connects when the Wi-Fi is up, retrying with a growing wait, and serves
  // the connection. Call it regularly.
  void loop(uint32_t nowMs, bool wifiUp);

  bool connected() { return client_.connected(); }

  // Publishes the state JSON, retained.
  bool publishState(const char* json, size_t length);

 private:
  void connect(uint32_t nowMs);

  WiFiClientSecure tls_;
  PubSubClient client_;
  supervision::Backoff backoff_{5UL * 1000UL, 5UL * 60UL * 1000UL};
  bool configured_ = false;
  bool wasConnected_ = false;
  char host_[sizeof(supervision::NetworkSettings::brokerHost)] = {};
  uint16_t port_ = 0;
  char user_[sizeof(supervision::NetworkSettings::mqttUser)] = {};
  char password_[sizeof(supervision::NetworkSettings::mqttPassword)] = {};
  char clientId_[supervision::kDeviceIdSize] = {};
  char stateTopic_[supervision::kTopicSize] = {};
  char statusTopic_[supervision::kTopicSize] = {};
};

}  // namespace terrarium::mqtt
