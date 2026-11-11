#include "mqtt_link.h"

#include <Arduino.h>

#include <cstring>

#include "root_certificates.h"
#include "state_message.h"
#include "update_command.h"

namespace terrarium::mqtt {

namespace {

constexpr uint16_t kBufferSize = 2176;  // State JSON up to supervision::kStateSize, plus the topic.
// PubSubClient refuses a packet whose header (MQTT_MAX_HEADER_SIZE), topic
// length, topic and payload exceed the buffer.
static_assert(kBufferSize >= MQTT_MAX_HEADER_SIZE + 2 + (supervision::kTopicSize - 1) +
                                 (supervision::kStateSize - 1),
              "MQTT buffer too small for the longest state");
// The longest cmd/mise-a-jour: {"licence":"<jeton>"}, with room for spaces.
static_assert(kBufferSize >= MQTT_MAX_HEADER_SIZE + 2 + (supervision::kTopicSize - 1) +
                                 update::kLicenceMaxLength + 32,
              "MQTT buffer too small for a licence");
// The OVH load balancer drops a connection idle for 50 s (measured).
constexpr uint16_t kKeepAliveS = 30;
constexpr uint16_t kSocketTimeoutS = 10;
constexpr uint32_t kHandshakeTimeoutS = 10;
constexpr char kOnline[] = "en-ligne";
constexpr char kOffline[] = "hors-ligne";

}  // namespace

const char* commandSubtopic(CommandTopic topic) {
  switch (topic) {
    case CommandTopic::kMode:
      return "cmd/mode";
    case CommandTopic::kConfig:
      return "cmd/config";
    case CommandTopic::kUpdate:
      return "cmd/mise-a-jour";
  }
  return "cmd";
}

void MqttLink::configure(const supervision::NetworkSettings& settings, const char* deviceId) {
  if (client_.connected()) client_.disconnect();
  std::strcpy(host_, settings.brokerHost);
  port_ = settings.brokerPort;
  std::strcpy(user_, settings.mqttUser);
  std::strcpy(password_, settings.mqttPassword);
  std::strncpy(clientId_, deviceId, sizeof(clientId_) - 1);
  configured_ =
      supervision::formatTopic(deviceId, "state", stateTopic_, sizeof(stateTopic_)) &&
      supervision::formatTopic(deviceId, "status", statusTopic_, sizeof(statusTopic_)) &&
      supervision::formatTopic(deviceId, "event", eventTopic_, sizeof(eventTopic_)) &&
      supervision::formatTopic(deviceId, "cmd/mode", modeTopic_, sizeof(modeTopic_)) &&
      supervision::formatTopic(deviceId, "cmd/config", configTopic_, sizeof(configTopic_)) &&
      supervision::formatTopic(deviceId, "cmd/mise-a-jour", updateTopic_, sizeof(updateTopic_));

  tls_.setCACert(kBrokerRootCertificates);
  tls_.setHandshakeTimeout(kHandshakeTimeoutS);
  client_.setServer(host_, port_);
  client_.setBufferSize(kBufferSize);
  client_.setKeepAlive(kKeepAliveS);
  client_.setSocketTimeout(kSocketTimeoutS);
  client_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
    receive(topic, payload, length);
  });
  backoff_.succeeded();
  wasConnected_ = false;
}

void MqttLink::loop(uint32_t nowMs, bool wifiUp) {
  if (!configured_) return;
  if (client_.connected()) {
    client_.loop();
    return;
  }
  if (wasConnected_) {
    wasConnected_ = false;
    gate_.disconnected();
    Serial.println("MQTT deconnecte");
  }
  if (wifiUp && backoff_.ready(nowMs)) connect(nowMs);
}

void MqttLink::connect(uint32_t nowMs) {
  // The last will marks the board offline if the connection drops.
  if (client_.connect(clientId_, user_, password_, statusTopic_, 0, true, kOffline)) {
    backoff_.succeeded();
    wasConnected_ = true;
    client_.publish(statusTopic_, kOnline, true);
    const bool subscribed = client_.subscribe(modeTopic_) && client_.subscribe(configTopic_) &&
                            client_.subscribe(updateTopic_);
    gate_.subscribed(nowMs);
    Serial.printf("MQTT connecte a %s:%u%s\r\n", host_, port_,
                  subscribed ? "" : ", ERREUR abonnement aux commandes");
    return;
  }
  backoff_.failed(nowMs);
  // state() < 0: network or TLS failure; > 0: refused by the broker (5 = bad account).
  Serial.printf("MQTT echec de connexion a %s:%u (code %d), nouvel essai plus tard\r\n", host_,
                port_, client_.state());
}

bool MqttLink::publishEvent(const char* json) {
  return client_.connected() && client_.publish(eventTopic_, json, false);
}

void MqttLink::receive(const char* topic, const uint8_t* payload, unsigned int length) {
  CommandTopic kind = CommandTopic::kMode;
  if (std::strcmp(topic, modeTopic_) == 0) {
    kind = CommandTopic::kMode;
  } else if (std::strcmp(topic, configTopic_) == 0) {
    kind = CommandTopic::kConfig;
  } else if (std::strcmp(topic, updateTopic_) == 0) {
    kind = CommandTopic::kUpdate;
  } else {
    return;
  }
  const char* subtopic = commandSubtopic(kind);
  if (!gate_.accepts(millis())) {
    char event[supervision::kEventSize];
    if (supervision::formatRejectedCommandEvent(
            subtopic, "retenue par le courtier ou recue a la connexion", event)) {
      publishEvent(event);
    }
    Serial.printf("MQTT commande %s ignoree : recue a la connexion\r\n", subtopic);
    return;
  }
  if (handler_) handler_(kind, payload, length);
}

bool MqttLink::publishState(const char* json, size_t length) {
  return client_.connected() && client_.publish(stateTopic_, reinterpret_cast<const uint8_t*>(json),
                                                static_cast<unsigned int>(length), true);
}

}  // namespace terrarium::mqtt
